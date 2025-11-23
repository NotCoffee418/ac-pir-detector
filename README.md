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

## Case
A 3D printable case for this project can be found [here](https://github.com/NotCoffee418/arbitrary-3d-models/ac_pir_detector_case).

## Setup

### Prerequisites
- [PlatformIO](https://platformio.org/) installed
- [LilyGO TTGO T-Camera Mic ESP32 board](https://github.com/lewisxhe/esp32-camera-series/blob/master/docs/T_CarmerV16.md)

### Configuration

1. Copy the example configuration file:
   ```bash
   cp data/config.example.json data/config.json
   ```

2. Edit `data/config.json` with your settings:
   ```json
   {
     "wifi": {
       "ssid": "your_wifi_ssid_here",
       "password": "your_wifi_password_here"
     },
     "api": {
       "host": "localhost",
       "port": 9040,
       "apiKey": "your_pir_api_key_here"
     },
     "device": {
       "name": "Veranda"
     },
     "pir": {
       "pin": 33,
       "detectionCooldownMs": 30000
     }
   }
   ```

   Configuration parameters:
   - **wifi.ssid**: Your WiFi network name
   - **wifi.password**: Your WiFi password
   - **api.host**: API server hostname/IP (e.g., "192.168.1.100" or "api.example.com")
   - **api.port**: API server port (default: 9040)
   - **api.apiKey**: Your PIR API key for authentication
   - **device.name**: Device identifier (default: "Veranda")
   - **pir.pin**: GPIO pin for PIR sensor (default: 33 for TTGO T-Camera)
   - **pir.detectionCooldownMs**: Cooldown between detections in milliseconds (default: 30000)

### Building and Uploading

```bash
# Build the project
pio run

# Upload the filesystem (config file)
pio run --target uploadfs

# Upload the firmware to the board
pio run --target upload

# Monitor serial output
pio device monitor
```

**Note:** You must upload the filesystem (`uploadfs`) before or after uploading the firmware to ensure your `config.json` is available on the device.

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
- Check SSID and password in data/config.json
- Ensure the device is within range of your WiFi network
- Check serial monitor for connection status
- Make sure you uploaded the filesystem with `pio run --target uploadfs`

### API Request Failures
- Verify PCC_HOST and PCC_PORT are correct
- Ensure API key is valid
- Check network connectivity
- Review serial monitor for HTTP response codes

### PIR Sensor Not Detecting
- Ensure pir.pin is set to 33 in config.json (default for TTGO T-Camera)
- Check serial monitor for "PIR Motion detected!" messages
- PIR sensor may need warm-up time after power-on

### Configuration Not Loading
- Verify config.json exists in the data folder
- Ensure you ran `pio run --target uploadfs` to upload the filesystem
- Check serial monitor for configuration loading errors
- Validate JSON syntax using a JSON validator

## License
MIT
