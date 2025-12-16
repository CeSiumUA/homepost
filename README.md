# Homepost

Homepost is an ESP32-based home monitoring and automation hub that combines BLE tracking, radiation monitoring, and MQTT integration into a single device.

## Features

- **BLE iBeacon Tracking**: Detects and tracks iBeacon devices for presence detection (e.g., phone tracking)
- **Geiger Counter Integration**: Monitors radiation levels and publishes data via MQTT
- **WiFi Connectivity**:
  - Station mode for connecting to existing networks
  - SoftAP mode for initial configuration
  - Automatic reconnection with configurable retry intervals
- **HTTP Server**: Web-based configuration interface
- **MQTT Publishing**: Sends sensor data and presence information to an MQTT broker
- **NVS Storage**: Persistent storage for WiFi credentials, MQTT settings, and configuration

## Hardware Requirements

- ESP32 development board
- Geiger counter sensor (connected via GPIO)
- Power supply (USB or external)

## Software Dependencies

- ESP-IDF v4.4 or later
- FreeRTOS (included with ESP-IDF)

## Building the Project

### Prerequisites

1. Install ESP-IDF following the [official installation guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html)
2. Set up the ESP-IDF environment:

   ```bash
   . $HOME/esp/esp-idf/export.sh
   ```

### Configure

Configure the project using menuconfig:

```bash
idf.py menuconfig
```

Navigate to `homepost configuration` to customize:

- **BT Options**: Bluetooth device name, inquiry length
- **Scanner Options**: RSSI filters, iBeacon major/minor IDs, scan timeout
- **WiFi Configuration**: SoftAP credentials, reconnection settings
- **HTTP Server Configuration**: Port settings
- **Storage Configuration**: NVS keys for credentials

### Build

Build the project:

```bash
idf.py build
```

### Flash

Flash the firmware to your ESP32:

```bash
idf.py -p PORT flash
```

Replace `PORT` with your ESP32's serial port (e.g., `COM3` on Windows or `/dev/ttyUSB0` on Linux).

### Monitor

View the serial output:

```bash
idf.py -p PORT monitor
```

Or combine flash and monitor:

```bash
idf.py -p PORT flash monitor
```

## Configuration

### Initial Setup

1. On first boot, the device will create a WiFi access point named `HOMEPOST` (default password: `12345678`)
2. Connect to this network and access the web interface at `http://192.168.4.1`
3. Configure:
   - WiFi credentials for your home network
   - MQTT broker settings (address, port, credentials, topic)
   - iBeacon tracking parameters
   - Publish intervals

### iBeacon Tracking

Configure the iBeacon major and minor IDs to match the devices you want to track:

- `HOMEPOST_SCAN_MAJOR_FILTER`: Major ID (default: 100)
- `HOMEPOST_SCAN_MINOR_FILTER`: Minor ID (default: 40004)
- `HOMEPOST_SCAN_TIMEOUT_MINUTES`: How long to wait before marking device as absent (default: 2 minutes)

### MQTT Topics

The device publishes to topics under the configured base topic:

- `{topic}/phone_present`: Presence detection status
- Additional topics for geiger counter data

## Project Structure

``` с
homepost/
├── CMakeLists.txt              # Project configuration
├── sdkconfig                   # Build configuration
├── main/
│   ├── main.c                  # Application entry point
│   ├── wifi.c                  # WiFi connection management
│   ├── http_server.c           # Web configuration interface
│   ├── ble_scanner.c           # BLE scanning functionality
│   ├── ble_ibeacon.c           # iBeacon protocol handling
│   ├── tracker_scanner.c       # Presence tracking logic
│   ├── geiger_counter.c        # Radiation sensor integration
│   ├── mqtt_connection.c       # MQTT client
│   ├── internal_storage.c      # NVS storage management
│   └── Kconfig.projbuild       # Configuration menu
└── inc/                        # Header files
```

## Troubleshooting

### WiFi Connection Issues

- Check the serial monitor for connection status
- Verify credentials are correct via the web interface
- The device will retry connection every 3 minutes by default

### MQTT Connection Problems

- Verify broker address and port are correct
- Check firewall settings on the broker
- Ensure credentials (if required) are properly configured

### BLE Scanning Not Working

- Verify the iBeacon major/minor IDs match your tracking device
- Check RSSI threshold if filtering is enabled
- Ensure Bluetooth (BLE Transmitter) is enabled on the tracking device

## License

This project is provided as-is for personal and educational use.
