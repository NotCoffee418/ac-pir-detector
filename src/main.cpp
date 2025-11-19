#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <TFT_eSPI.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

// Hardware pins for TTGO T-Camera
#define BUTTON_PIN 34

// Configuration variables (loaded from JSON)
String wifiSsid;
String wifiPassword;
String pccHost;
int pccPort;
String pccApiKey;
String deviceName;
int pirPin;
unsigned long detectionCooldownMs;

// Global variables
TFT_eSPI tft = TFT_eSPI();
unsigned long lastDetectionTime = 0;
bool isConnected = false;
int detectionCount = 0;

// Function prototypes
bool loadConfig();
void setupWiFi();
void setupDisplay();
void updateDisplay(const char* status);
void handlePIRDetection();
bool sendDetectionAPI();
void checkPIRSensor();

void setup() {
  Serial.begin(115200);
  Serial.println("AC PIR Detector Starting...");
  
  // Initialize display
  setupDisplay();
  updateDisplay("Initializing...");
  
  // Initialize filesystem
  if (!LittleFS.begin(true)) {
    Serial.println("Failed to mount LittleFS!");
    updateDisplay("FS Error");
    while(1) delay(1000);
  }
  
  // Load configuration
  if (!loadConfig()) {
    Serial.println("Failed to load configuration!");
    updateDisplay("Config Error");
    while(1) delay(1000);
  }
  
  // Initialize PIR sensor pin
  pinMode(pirPin, INPUT);
  
  // Connect to WiFi
  setupWiFi();
  
  updateDisplay("Ready");
  Serial.println("Setup complete. Monitoring PIR sensor...");
}

void loop() {
  // Check WiFi connection
  if (WiFi.status() != WL_CONNECTED) {
    if (isConnected) {
      isConnected = false;
      updateDisplay("WiFi Lost");
      Serial.println("WiFi connection lost. Reconnecting...");
    }
    setupWiFi();
  } else {
    if (!isConnected) {
      isConnected = true;
      updateDisplay("Connected");
    }
  }
  
  // Check PIR sensor
  checkPIRSensor();
  
  delay(100);  // Small delay to prevent CPU overload
}

void setupWiFi() {
  updateDisplay("Connecting WiFi...");
  Serial.print("Connecting to WiFi: ");
  Serial.println(wifiSsid);
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifiSsid.c_str(), wifiPassword.c_str());
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    isConnected = true;
    Serial.println("\nWiFi connected!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    updateDisplay("WiFi Connected");
    delay(1000);
  } else {
    Serial.println("\nWiFi connection failed!");
    updateDisplay("WiFi Failed");
  }
}

void setupDisplay() {
  tft.init();
  tft.setRotation(1);  // Landscape orientation
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(2);
}

void updateDisplay(const char* status) {
  tft.fillScreen(TFT_BLACK);
  
  // Title
  tft.setTextSize(2);
  tft.drawString("PIR Detector", tft.width() / 2, 30);
  
  // Device name
  tft.setTextSize(1);
  tft.drawString(deviceName.c_str(), tft.width() / 2, 60);
  
  // Status
  tft.setTextSize(2);
  tft.drawString(status, tft.width() / 2, 90);
  
  // Detection count
  if (detectionCount > 0) {
    char countStr[32];
    sprintf(countStr, "Detections: %d", detectionCount);
    tft.setTextSize(1);
    tft.drawString(countStr, tft.width() / 2, 120);
  }
}

void checkPIRSensor() {
  int pirState = digitalRead(pirPin);
  
  if (pirState == HIGH) {
    unsigned long currentTime = millis();
    
    // Check if cooldown period has passed
    if (currentTime - lastDetectionTime >= detectionCooldownMs) {
      Serial.println("PIR Motion detected!");
      handlePIRDetection();
      lastDetectionTime = currentTime;
    }
  }
}

void handlePIRDetection() {
  updateDisplay("Motion Detected!");
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Sending detection to API...");
    bool success = sendDetectionAPI();
    
    if (success) {
      detectionCount++;
      Serial.println("Detection sent successfully!");
      updateDisplay("Sent to API");
      delay(2000);
      updateDisplay("Ready");
    } else {
      Serial.println("Failed to send detection to API");
      updateDisplay("API Failed");
      delay(2000);
      updateDisplay("Ready");
    }
  } else {
    Serial.println("WiFi not connected. Cannot send detection.");
    updateDisplay("No WiFi");
    delay(2000);
    updateDisplay("Ready");
  }
}

bool sendDetectionAPI() {
  HTTPClient http;
  
  // Build the URL with device query parameter
  char url[256];
  snprintf(url, sizeof(url), "http://%s:%d/api/pir/detect?device=%s", 
           pccHost.c_str(), pccPort, deviceName.c_str());
  
  Serial.print("API URL: ");
  Serial.println(url);
  
  http.begin(url);
  
  // Add Authorization header
  String authHeader = "ApiKey " + pccApiKey;
  http.addHeader("Authorization", authHeader);
  
  // Send POST request
  int httpResponseCode = http.POST("");
  
  bool success = false;
  if (httpResponseCode > 0) {
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
    
    String response = http.getString();
    Serial.print("Response: ");
    Serial.println(response);
    
    success = (httpResponseCode >= 200 && httpResponseCode < 300);
  } else {
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
  }
  
  http.end();
  return success;
}

bool loadConfig() {
  Serial.println("Loading configuration from /config.json...");
  
  File configFile = LittleFS.open("/config.json", "r");
  if (!configFile) {
    Serial.println("Failed to open config.json");
    Serial.println("Please upload config.json to the data folder");
    return false;
  }
  
  size_t size = configFile.size();
  if (size > 1024) {
    Serial.println("Config file size is too large");
    configFile.close();
    return false;
  }
  
  // Allocate a buffer to store contents of the file
  std::unique_ptr<char[]> buf(new char[size]);
  configFile.readBytes(buf.get(), size);
  configFile.close();
  
  // Parse JSON
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, buf.get());
  
  if (error) {
    Serial.print("Failed to parse config.json: ");
    Serial.println(error.c_str());
    return false;
  }
  
  // Load WiFi configuration
  wifiSsid = doc["wifi"]["ssid"].as<String>();
  wifiPassword = doc["wifi"]["password"].as<String>();
  
  // Load API configuration
  pccHost = doc["api"]["host"].as<String>();
  pccPort = doc["api"]["port"].as<int>();
  pccApiKey = doc["api"]["apiKey"].as<String>();
  
  // Load device configuration
  deviceName = doc["device"]["name"].as<String>();
  
  // Load PIR configuration
  pirPin = doc["pir"]["pin"].as<int>();
  detectionCooldownMs = doc["pir"]["detectionCooldownMs"].as<unsigned long>();
  
  // Print loaded configuration (without passwords/keys)
  Serial.println("Configuration loaded successfully:");
  Serial.print("  WiFi SSID: ");
  Serial.println(wifiSsid);
  Serial.print("  API Host: ");
  Serial.println(pccHost);
  Serial.print("  API Port: ");
  Serial.println(pccPort);
  Serial.print("  Device Name: ");
  Serial.println(deviceName);
  Serial.print("  PIR Pin: ");
  Serial.println(pirPin);
  Serial.print("  Detection Cooldown: ");
  Serial.print(detectionCooldownMs);
  Serial.println(" ms");
  
  return true;
}
