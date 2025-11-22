#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <SSD1306.h>
#include <esp_wifi.h>

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
  Serial.begin(115200);
  Serial.println("AC PIR Detector Starting...");

  // Fix WiFi power and sleep issues
  WiFi.persistent(false);
  WiFi.setSleep(false);         // CRITICAL: Disable power saving
  WiFi.setAutoReconnect(false); // We'll handle reconnection

  // Set WiFi to maximum power
  WiFi.setTxPower(WIFI_POWER_19_5dBm); // Maximum power

  // Set longer timeouts for beacon loss
  esp_wifi_set_inactive_time(WIFI_IF_STA, 30); // 30 seconds before disconnect

  setupDisplay();

  if (!LittleFS.begin(true))
  {
    Serial.println("Failed to mount LittleFS!");
    while (1)
      delay(1000);
  }

  if (!loadConfig())
  {
    Serial.println("Failed to load configuration!");
    while (1)
      delay(1000);
  }

  pinMode(PIR_PIN, INPUT);

  setupWiFi();

  Serial.println("Setup complete. Monitoring PIR sensor...");
}

void loop()
{
  if (WiFi.status() != WL_CONNECTED)
  {
    if (isConnected)
    {
      Serial.println("WiFi lost, reconnecting...");
      isConnected = false;
      WiFi.disconnect();
      delay(1000);
      setupWiFi();
    }
  }
  else
  {
    if (!isConnected)
    {
      Serial.println("WiFi Connected");
      isConnected = true;
    }
  }

  checkPIRSensor();
  updateDisplay();
  delay(1000);
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
  Serial.print("Connecting to WiFi: ");
  Serial.println(wifiSsid);

  WiFi.disconnect(true); // Clear any old connection
  delay(100);

  WiFi.mode(WIFI_STA);

  // Configure WiFi for stability
  wifi_config_t wifi_config = {};
  esp_wifi_get_config(WIFI_IF_STA, &wifi_config);
  wifi_config.sta.bssid_set = 0; // Don't lock to specific BSSID
  wifi_config.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
  wifi_config.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;
  esp_wifi_set_config(WIFI_IF_STA, &wifi_config);

  WiFi.begin(wifiSsid.c_str(), wifiPassword.c_str());

  if (WiFi.waitForConnectResult(35000UL) != WL_CONNECTED)
  {
    Serial.println("WiFi: Failed to connect");
    return;
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    isConnected = true;
    Serial.println("\nWiFi connected!");
    Serial.printf("IP: %s, RSSI: %d\n",
                  WiFi.localIP().toString().c_str(),
                  WiFi.RSSI());
    delay(500); // Let network stack stabilize
  }
}

void checkPIRSensor()
{
  int pirState = digitalRead(PIR_PIN);
  currentPirState = (pirState == HIGH);

  if (pirState == HIGH)
  {
    unsigned long currentTime = millis();

    if (currentTime - lastDetectionTime >= detectionCooldownMs)
    {
      Serial.println("PIR: Motion detected!");
      handlePIRDetection();
      lastDetectionTime = currentTime;
    }
  }
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
      Serial.println("Detection sent successfully!");
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
  // Check WiFi is actually connected before attempting
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("WiFi not connected, skipping API call");
    return false;
  }

  // Always use Method 2 - it's the only one that worked
  Serial.println("Using Simple HTTPClient method");

  HTTPClient http;

  char url[256];
  snprintf(url, sizeof(url), "http://%s:%d/api/pir/detect?device=%s",
           pccHost.c_str(), pccPort, deviceName.c_str());

  Serial.print("API URL: ");
  Serial.println(url);

  http.begin(url);
  http.setTimeout(3000); // Shorter timeout to avoid WiFi disconnect

  char authHeader[128];
  snprintf(authHeader, sizeof(authHeader), "ApiKey %s", pccApiKey.c_str());
  http.addHeader("Authorization", authHeader);
  http.addHeader("Content-Type", "application/json");

  int code = http.POST("{}");

  bool success = false;
  if (code > 0)
  {
    Serial.printf("HTTP Response: %d\n", code);
    if (code >= 200 && code < 300)
    {
      String response = http.getString();
      Serial.print("Response: ");
      Serial.println(response);
      success = true;
    }
  }
  else
  {
    Serial.printf("HTTP Error: %s\n", http.errorToString(code).c_str());
  }

  http.end();

  return success;
}

void updateDisplay()
{
  display.clear();

  display.setFont(ArialMT_Plain_10);
  display.drawString(64, 0, deviceName);

  if (isConnected)
  {
    display.drawString(64, 12, wifiSsid);
  }
  else
  {
    display.drawString(64, 12, "WiFi: --");
  }

  if (isConnected)
  {
    int rssi = WiFi.RSSI();
    String signalStr = "Signal: " + String(rssi) + " dBm";
    display.drawString(64, 24, signalStr);
  }
  else
  {
    display.drawString(64, 24, "");
  }

  display.setFont(ArialMT_Plain_24);
  if (currentPirState)
  {
    display.drawString(64, 40, "V");
  }
  else
  {
    display.drawString(64, 40, "X");
  }

  display.display();
}

bool loadConfig()
{
  Serial.println("Loading configuration from /config.json...");

  fs::File configFile = LittleFS.open("/config.json", "r");
  if (!configFile)
  {
    Serial.println("Failed to open config.json");
    return false;
  }

  size_t size = configFile.size();
  if (size > 1024)
  {
    Serial.println("Config file size is too large");
    configFile.close();
    return false;
  }

  std::unique_ptr<char[]> buf(new char[size]);
  configFile.readBytes(buf.get(), size);
  configFile.close();

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, buf.get());

  if (error)
  {
    Serial.print("Failed to parse config.json: ");
    Serial.println(error.c_str());
    return false;
  }

  wifiSsid = doc["wifi"]["ssid"].as<String>();
  wifiPassword = doc["wifi"]["password"].as<String>();

  pccHost = doc["api"]["host"].as<String>();
  pccPort = doc["api"]["port"].as<int>();
  pccApiKey = doc["api"]["apiKey"].as<String>();

  deviceName = doc["device"]["name"].as<String>();

  detectionCooldownMs = doc["pirDetectionCooldownMs"].as<unsigned long>();

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