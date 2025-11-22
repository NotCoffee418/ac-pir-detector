#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <SSD1306.h>

using fs::File;

// Configuration variables (loaded from JSON)
String wifiSsid;
String wifiPassword;
String pccHost;
int pccPort;
String pccApiKey;
String deviceName;
unsigned long detectionCooldownMs;
bool currentPirState = false;

// Global constants
const int PIR_PIN = 19;
SSD1306 display(0x3c, 21, 22); // Address, SDA, SCL

// Global variables
unsigned long lastDetectionTime = 0;
bool isConnected = false;
int detectionCount = 0;

// Function prototypes
bool loadConfig();
void setupWiFi();
void handlePIRDetection();
bool sendDetectionAPI();
void checkPIRSensor();
void setupDisplay();
void updateDisplay();

void setup()
{
  // Start display
  setupDisplay();

  // Start PIR detector
  Serial.begin(115200);
  Serial.println("AC PIR Detector Starting...");

  // Initialize filesystem
  if (!LittleFS.begin(true))
  {
    Serial.println("Failed to mount LittleFS!");
    while (1)
      delay(1000);
  }

  // Load configuration
  if (!loadConfig())
  {
    Serial.println("Failed to load configuration!");
    while (1)
      delay(1000);
  }

  // Initialize PIR sensor pin
  pinMode(PIR_PIN, INPUT);

  // Connect to WiFi
  setupWiFi();

  Serial.println("Setup complete. Monitoring PIR sensor...");
}

void loop()
{
  // Check WiFi connection
  if (WiFi.status() != WL_CONNECTED)
  {
    if (isConnected)
    {
      isConnected = false;
      Serial.println("WiFi connection lost. Reconnecting...");
    }
    // setupWiFi(); temp stop
  }
  else
  {
    if (!isConnected)
    {
      isConnected = true;
      Serial.println("WiFi Connected");
    }
  }

  // Check PIR sensor
  checkPIRSensor();

  delay(100);
}

void setupDisplay()
{
  display.init();
  display.flipScreenVertically();
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setFont(ArialMT_Plain_10);
  display.clear();
  display.display();
}

void setupWiFi()
{
  Serial.println("Connecting WiFi...");
  Serial.print("Connecting to WiFi: ");
  Serial.println(wifiSsid);

  WiFi.persistent(false);
  WiFi.mode(WIFI_OFF);
  delay(100);
  WiFi.mode(WIFI_STA);
  WiFi.setTxPower(WIFI_POWER_19_5dBm); // Reduce WiFi power
  WiFi.setSleep(false);
  WiFi.begin(wifiSsid.c_str(), wifiPassword.c_str());

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30)
  {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    isConnected = true;
    Serial.println("\nWiFi connected!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  }
  else
  {
    Serial.println("\nWiFi connection failed!");
  }
}

void checkPIRSensor()
{
  Serial.println("Checking PIR sensor...");
  int pirState = digitalRead(PIR_PIN);

  if (pirState == HIGH)
  {
    unsigned long currentTime = millis();

    // Check if cooldown period has passed
    if (currentTime - lastDetectionTime >= detectionCooldownMs)
    {
      Serial.println("PIR: Motion detected and handling!");
      handlePIRDetection();
      lastDetectionTime = currentTime;
    }
    else
    {
      Serial.println("PIR: Motion detected but in cooldown period.");
    }
  }
  else
  {
    Serial.println("PIR: No motion detected.");
  }
  updateDisplay();
}

void handlePIRDetection()
{
  Serial.println("Motion Detected!");

  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.println("Sending detection to API...");
    bool success = sendDetectionAPI();

    if (success)
    {
      detectionCount++;
      Serial.println("Detection sent successfully!");
      Serial.print("Total detections: ");
      Serial.println(detectionCount);
    }
    else
    {
      Serial.println("Failed to send detection to API");
    }
  }
  else
  {
    Serial.println("WiFi not connected. Cannot send detection.");
  }
}

bool sendDetectionAPI()
{
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
  if (httpResponseCode > 0)
  {
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);

    String response = http.getString();
    Serial.print("Response: ");
    Serial.println(response);

    success = (httpResponseCode >= 200 && httpResponseCode < 300);
  }
  else
  {
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
  }

  http.end();
  return success;
}

void updateDisplay()
{
  display.clear();

  // Title
  display.setFont(ArialMT_Plain_10);
  display.drawString(64, 0, deviceName);

  // WiFi status
  if (isConnected)
  {
    display.drawString(64, 12, "WiFi: OK");
  }
  else
  {
    display.drawString(64, 12, "WiFi: --");
  }

  // Big V or X in the middle
  display.setFont(ArialMT_Plain_24);
  if (currentPirState)
  {
    display.drawString(64, 30, "V");
  }
  else
  {
    display.drawString(64, 30, "X");
  }

  // Detection count at bottom
  display.setFont(ArialMT_Plain_10);
  String countStr = "Count: " + String(detectionCount);
  display.drawString(64, 54, countStr);

  display.display();
}

bool loadConfig()
{
  Serial.println("Loading configuration from /config.json...");

  fs::File configFile = LittleFS.open("/config.json", "r");
  if (!configFile)
  {
    Serial.println("Failed to open config.json");
    Serial.println("Please upload config.json to the data folder");
    return false;
  }

  size_t size = configFile.size();
  if (size > 1024)
  {
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

  if (error)
  {
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
  detectionCooldownMs = doc["pirDetectionCooldownMs"].as<unsigned long>();

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
  Serial.println(PIR_PIN);
  Serial.print("  Detection Cooldown: ");
  Serial.print(detectionCooldownMs);
  Serial.println(" ms");

  return true;
}