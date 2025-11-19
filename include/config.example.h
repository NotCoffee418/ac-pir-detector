#ifndef CONFIG_H
#define CONFIG_H

// WiFi Configuration
#define WIFI_SSID "your_wifi_ssid_here"
#define WIFI_PASSWORD "your_wifi_password_here"

// API Configuration
#define PCC_HOST "localhost"
#define PCC_PORT 9040
#define PCC_API_KEY "your_pir_api_key_here"

// Device Configuration
#define DEVICE_NAME "Veranda"

// PIR Sensor Configuration
#define PIR_PIN 33  // PIR sensor pin on TTGO T-Camera
#define DETECTION_COOLDOWN_MS 30000  // 30 seconds cooldown between detections

#endif // CONFIG_H
