#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <TFT_eSPI.h>
#include "config.h"

// Hardware pins for TTGO T-Camera
#define PIR_SENSOR_PIN PIR_PIN
#define BUTTON_PIN 34

// Global variables
TFT_eSPI tft = TFT_eSPI();
unsigned long lastDetectionTime = 0;
bool isConnected = false;
int detectionCount = 0;

// Function prototypes
void setupWiFi();
void setupDisplay();
void updateDisplay(const char* status);
void handlePIRDetection();
bool sendDetectionAPI();
void checkPIRSensor();

void setup() {
  Serial.begin(115200);
  Serial.println("AC PIR Detector Starting...");
  
  // Initialize PIR sensor pin
  pinMode(PIR_SENSOR_PIN, INPUT);
  
  // Initialize display
  setupDisplay();
  updateDisplay("Initializing...");
  
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
  Serial.println(WIFI_SSID);
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
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
  tft.drawString(DEVICE_NAME, tft.width() / 2, 60);
  
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
  int pirState = digitalRead(PIR_SENSOR_PIN);
  
  if (pirState == HIGH) {
    unsigned long currentTime = millis();
    
    // Check if cooldown period has passed
    if (currentTime - lastDetectionTime >= DETECTION_COOLDOWN_MS) {
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
           PCC_HOST, PCC_PORT, DEVICE_NAME);
  
  Serial.print("API URL: ");
  Serial.println(url);
  
  http.begin(url);
  
  // Add Authorization header
  char authHeader[128];
  snprintf(authHeader, sizeof(authHeader), "ApiKey %s", PCC_API_KEY);
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
