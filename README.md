# ac-pir-detector
PIR sensor for disabling AC using LilyGO TTGO T-Camera Mic ESP32

## Overview
This project uses the LilyGO TTGO T-Camera Mic ESP32 with its built-in PIR sensor to detect motion and send API notifications. When motion is detected, the device sends a POST request to a specified API endpoint with a 30-second cooldown between detections.

## Hardware
- **Device**: LilyGO TTGO T-Camera Mic ESP32
- **PIR Sensor**: Built-in on GPIO 33
- **Display**: ST7789 TFT display (135x240)

## Features
- Motion detection using PIR sensor
- Visual feedback on TFT display
- WiFi connectivity
- HTTP API integration with authentication
- 30-second cooldown between detections
- Detection counter
- Auto-reconnect on WiFi loss

## Setup

### Prerequisites
- [PlatformIO](https://platformio.org/) installed
- LilyGO TTGO T-Camera Mic ESP32 board

### Configuration

1. Copy the example configuration file:
   ```bash
   cp include/config.example.h include/config.h
   ```

2. Edit `include/config.h` with your settings:
   - **WIFI_SSID**: Your WiFi network name
   - **WIFI_PASSWORD**: Your WiFi password
   - **PCC_HOST**: API server hostname/IP (e.g., "192.168.1.100" or "api.example.com")
   - **PCC_PORT**: API server port (default: 9040)
   - **PCC_API_KEY**: Your PIR API key for authentication
   - **DEVICE_NAME**: Device identifier (default: "Veranda")

### Building and Uploading

```bash
# Build the project
pio run

# Upload to the board
pio run --target upload

# Monitor serial output
pio device monitor
```

## API Endpoint

The device sends POST requests to:
```
POST http://{PCC_HOST}:{PCC_PORT}/api/pir/detect?device={DEVICE_NAME}
```

**Headers:**
```
Authorization: ApiKey {PCC_API_KEY}
```

**Example using curl:**
```bash
curl -X POST "http://localhost:9040/api/pir/detect?device=Veranda" \
  -H "Authorization: ApiKey your_pir_api_key_here"
```

## Operation

1. On startup, the device connects to WiFi
2. The display shows the current status
3. When motion is detected by the PIR sensor:
   - Display shows "Motion Detected!"
   - If WiFi is connected, sends API request
   - Updates detection counter
   - Enters 30-second cooldown period
4. Status updates are shown on the display:
   - "Ready" - Monitoring for motion
   - "Motion Detected!" - PIR triggered
   - "Sent to API" - Successfully sent to server
   - "API Failed" - Failed to send
   - "No WiFi" - WiFi disconnected

## Troubleshooting

### WiFi Connection Issues
- Check SSID and password in config.h
- Ensure the device is within range of your WiFi network
- Check serial monitor for connection status

### API Request Failures
- Verify PCC_HOST and PCC_PORT are correct
- Ensure API key is valid
- Check network connectivity
- Review serial monitor for HTTP response codes

### PIR Sensor Not Detecting
- Ensure PIR_PIN is set to 33 (default for TTGO T-Camera)
- Check serial monitor for "PIR Motion detected!" messages
- PIR sensor may need warm-up time after power-on

## License
MIT
