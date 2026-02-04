# Homepost

Homepost is an ESP32-based home monitoring and automation hub that combines BLE tracking, environmental sensing, radiation monitoring, and MQTT integration into a single device.

## Features

- **BLE iBeacon Tracking**: Detects and tracks iBeacon devices for presence detection (e.g., phone tracking)
- **HTU21 Temperature & Humidity Sensor**: Monitors environmental conditions via I2C and publishes data via MQTT
- **Geiger Counter Integration**: Monitors radiation levels and publishes data via MQTT
- **WiFi Connectivity**:
  - Station mode for connecting to existing networks
  - SoftAP mode for initial configuration
  - Automatic reconnection with configurable retry intervals
- **HTTP Server**: Web-based configuration interface
- **MQTT Publishing**: Sends sensor data and presence information to an MQTT broker
- **NVS Storage**: Persistent storage for WiFi credentials, MQTT settings, and configuration

## Hardware Requirements

- ESP32 development board (4MB flash minimum)
- HTU21D temperature/humidity sensor (I2C, default: SDA=GPIO21, SCL=GPIO22)
- Geiger counter sensor (connected via GPIO)
- Power supply (USB or external)

> **Note**: A custom PCB design is available in the `hardware/` directory (KiCad 8 format).

## Software Dependencies

- ESP-IDF v5.5 or later
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

- `{topic}/phone_present`: Presence detection status (iBeacon tracking)
- `{topic}/temperature`: Temperature readings in JSON format (`{"temperature": XX.XX}`)
- `{topic}/humidity`: Humidity readings in JSON format (`{"humidity": XX.XX}`)
- `{topic}/geiger`: Geiger counter CPM (counts per minute) data

### HTU21 Temperature & Humidity Sensor

Configure the HTU21 sensor via menuconfig:

- `HOMEPOST_HTU21_TIMER_PERIOD_MS`: Reading interval (default: 60000ms / 1 minute)
- `HOMEPOST_HTU21_I2C_SDA_GPIO`: I2C SDA pin (default: GPIO 21)
- `HOMEPOST_HTU21_I2C_SCL_GPIO`: I2C SCL pin (default: GPIO 22)
- `HOMEPOST_HTU21_I2C_FREQ_HZ`: I2C clock frequency (default: 100kHz)

## Project Structure

```text
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
│   ├── htu21_sensor.c          # HTU21 temperature/humidity sensor
│   ├── mqtt_connection.c       # MQTT client
│   ├── internal_storage.c      # NVS storage management
│   ├── ota_update.c            # OTA firmware update
│   └── Kconfig.projbuild       # Configuration menu
├── inc/                        # Header files
└── hardware/                   # KiCad PCB design files
    └── manufacturing/          # Gerber files for PCB fabrication
```

## OTA (Over-The-Air) Updates

Homepost supports automatic firmware updates from GitHub releases.

### Requirements

1. **Flash Size**: Requires 4MB flash (configured in `menuconfig` → `Serial flasher config` → `Flash size`)

2. **Partition Table**: Must use OTA-compatible partition layout. In `menuconfig`:
   - Navigate to `Partition Table`
   - Select `Two large size OTA partitions` (required for 4MB flash with OTA)

3. **Bootloader Rollback** (recommended): Enable automatic rollback on failed boot:
   - Navigate to `Bootloader config`
   - Enable `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE`

### GitHub Release Format

When creating releases on GitHub:

- **Tag format**: `release-v{version}` (e.g., `release-v1.3.0`)
- **Asset filename**: `homepost-{version}.bin` (e.g., `homepost-1.3.0.bin`)

### OTA Configuration

Configure OTA settings via `menuconfig` → `homepost configuration` → `OTA Update Configuration`:

- `HOMEPOST_OTA_ENABLED`: Enable/disable OTA updates (default: enabled)
- `HOMEPOST_OTA_GITHUB_OWNER`: GitHub repository owner/organization
- `HOMEPOST_OTA_GITHUB_REPO`: Repository name (default: `homepost`)
- `HOMEPOST_OTA_CHECK_INTERVAL_HOURS`: Update check interval (default: 4 hours)
- `HOMEPOST_OTA_INITIAL_DELAY_SECONDS`: Delay before first check (default: 30 seconds)

### Manual Update

Access the web interface and use the "Firmware Update" section to:

- View current and available versions
- Check for updates manually
- Trigger an update installation

### API Endpoints

- `GET /check-update`: Returns JSON with version info
- `POST /trigger-update`: Triggers immediate update check and installation

## Hardware Design

The `hardware/` directory contains a complete KiCad 8 project for a custom Homepost PCB:

- **Schematic**: `homepost.kicad_sch`
- **PCB Layout**: `homepost.kicad_pcb`
- **Manufacturing Files**: `manufacturing/` directory contains Gerber files and drill files ready for PCB fabrication

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
- Ensure Bluetooth (BLE advertising) is enabled on the tracking device

### HTU21 Sensor Not Working

- Verify I2C wiring: SDA to GPIO21, SCL to GPIO22 (default pins)
- Check that the sensor is powered (3.3V)
- Review serial monitor for I2C initialization errors
- Ensure no I2C address conflicts (HTU21 uses address 0x40)

## License

This project is provided as-is for personal and educational use.
