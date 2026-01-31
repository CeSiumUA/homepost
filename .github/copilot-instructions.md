# Homepost ESP-IDF Project Instructions

## Project Overview
Homepost is an ESP32-based IoT hub combining BLE iBeacon tracking, Geiger counter monitoring, WiFi connectivity, HTTP configuration server, and MQTT publishing. Built with ESP-IDF v4.4+.

## Architecture & Component Structure

### Component Organization
- **main/**: Single component containing all application code (not multi-component architecture)
- **inc/**: Shared headers included via `INCLUDE_DIRS "../inc"` in [main/CMakeLists.txt](main/CMakeLists.txt)
- Components are individual `.c` files registered in main CMakeLists: `wifi.c`, `http_server.c`, `ble_scanner.c`, `tracker_scanner.c`, `mqtt_connection.c`, `geiger_counter.c`, `internal_storage.c`

### Startup & Initialization Flow ([main/main.c](main/main.c))
1. NVS storage initialization (`internal_storage_init()`)
2. WiFi init → STA connection attempt OR SoftAP fallback
3. Automatic WiFi reconnection via `esp_timer` (3-minute intervals by default)
4. HTTP server start (always runs for configuration)
5. Background tasks: `tracker_scanner_start_task()`, `mqtt_connection_start_task()`, `geiger_counter_start()`

### Key Data Flows
- **BLE → MQTT**: `ble_scanner.c` → `tracker_scanner.c` (FreeRTOS EventGroup) → MQTT queue → `mqtt_connection.c`
- **Geiger → MQTT**: GPIO ISR increments counter → timer callback calculates CPM → MQTT queue
- **HTTP → NVS → Actions**: Web form → parse POST data → save to NVS → trigger WiFi/MQTT connection

## ESP-IDF Specific Patterns

### Build System (Use ESP-IDF Extension)
- **NEVER** run `idf.py` commands directly in terminal - use VS Code ESP-IDF extension tools
- Build: ESP-IDF extension command palette or sidebar
- Flash/Monitor: Use extension's integrated tools
- Configuration: `idf.py menuconfig` accesses Kconfig settings in [main/Kconfig.projbuild](main/Kconfig.projbuild)

### Configuration System (Kconfig)
All project settings via menuconfig under "homepost configuration":
- BT/Scanner options: Device name, RSSI filters, iBeacon major/minor IDs, scan timeout
- WiFi: SoftAP credentials, reconnection timer (`CONFIG_HOMEPOST_WIFI_RECONNECTION_TIMER_PERIOD_US`)
- Storage: NVS key names (e.g., `CONFIG_HOMEPOST_WIFI_CREDENTIALS_STORAGE_KEY`)
- MQTT topic base configured at compile time, not runtime

Access in code via `CONFIG_*` macros (e.g., `CONFIG_HOMEPOST_SCAN_MAJOR_FILTER`)

### NVS Storage Pattern ([main/internal_storage.c](main/internal_storage.c))
- Namespace: `"storage"` (hardcoded)
- WiFi credentials stored as single string: `ssid\npassword` delimited by newline
- Check-then-get pattern: `internal_storage_check_*_preserved()` before `internal_storage_get_*()`
- All functions return `esp_err_t`, use `ESP_ERROR_CHECK()` for critical operations

### FreeRTOS Task Conventions
Tasks created with explicit stack size and priority constants:
```c
#define TASK_NAME_PRIORITY    6
#define TASK_NAME_STACK_SIZE  2048
xTaskCreate(task_func, "task_name", STACK_SIZE, NULL, PRIORITY, &handle);
```
See [main/tracker_scanner.c](main/tracker_scanner.c) and [main/mqtt_connection.c](main/mqtt_connection.c)

### Event Synchronization Pattern
FreeRTOS EventGroups used extensively for state management:
- WiFi: `WIFI_CONNECTED_BIT`, `WIFI_FAIL_BIT` in [main/wifi.c](main/wifi.c)
- Tracker: `TRACKER_SCANNER_EVENT_BIT` with timeout for presence detection
- MQTT: `MQTT_CONNECTION_CONNECTED_EVENT_BIT` for connection state

### MQTT Publishing Pattern ([main/mqtt_connection.c](main/mqtt_connection.c))
Queue-based async publishing:
1. Components enqueue messages via `mqtt_connection_put_publish_queue(&msg)`
2. MQTT task waits for connection → dequeues → publishes → waits for `MQTT_EVENT_PUBLISHED`
3. Message struct: `{.topic, .payload, .qos}`
4. Topics are string literals like `CONFIG_HOMEPOST_MQTT_TOPIC "/phone_present"`

### WiFi Dual-Mode Strategy ([main/wifi.c](main/wifi.c))
- Mode: `WIFI_MODE_APSTA` (SoftAP + Station simultaneously)
- SoftAP always active after first boot for configuration at `192.168.4.1`
- STA attempts connection if NVS credentials exist
- Failed reconnection → restart device (unless in SoftAP-only mode initially)

### HTTP Server Pattern ([main/http_server.c](main/http_server.c))
- Embedded HTML: `EMBED_TXTFILES "web/index.html"` in CMakeLists
- Access via `extern const char rootpage_start[] asm("_binary_index_html_start")`
- POST handlers parse URL-encoded form data manually (no JSON)
- Restart device via `esp_timer` callback after configuration changes

### BLE iBeacon Tracking ([main/tracker_scanner.c](main/tracker_scanner.c))
- Passive scanning only (`BLE_SCAN_TYPE_PASSIVE`)
- Filters by major/minor IDs from Kconfig
- Presence = iBeacon seen within timeout window (default 2 min)
- Uses event bits to signal detection, timeout determines "lost" state

## Common Development Tasks

### Adding New Sensor
1. Create `.c` file in `main/`, header in `inc/`
2. Register in [main/CMakeLists.txt](main/CMakeLists.txt) SRCS list
3. Add required ESP-IDF components to REQUIRES
4. Create FreeRTOS task following existing pattern
5. Publish data via `mqtt_connection_put_publish_queue()`

### Adding Configuration Option
1. Add Kconfig option in [main/Kconfig.projbuild](main/Kconfig.projbuild)
2. Access via `CONFIG_*` in code
3. If runtime-configurable: add NVS storage functions to [internal_storage.c](internal_storage.c)
4. Add HTTP form field and POST handler to [http_server.c](http_server.c)

### Debugging
- Serial monitor via ESP-IDF extension
- Log levels per-file via `static const char *TAG = __FILE__`
- Use `ESP_LOGI()`, `ESP_LOGW()`, `ESP_LOGE()`, `ESP_LOGD()`
- GPIO spinlock pattern for ISRs: `portENTER_CRITICAL(&spinlock)` in [geiger_counter.c](geiger_counter.c)

## Critical Gotchas
- WiFi credentials format: `ssid\npassword` with newline delimiter in NVS
- MQTT connection starts only AFTER credentials saved via HTTP POST
- Device auto-restarts on connection failures unless in initial SoftAP mode
- Task handles must be checked: `configASSERT(task_handle)` after creation
- Embedded files require assembly linkage: `asm("_binary_*")`
- Scan timeout uses `pdMS_TO_TICKS()` with minutes × 60 × 1000

## Project-Specific Conventions
- Error handling: `ESP_ERROR_CHECK()` for init failures, return `esp_err_t` otherwise
- String sizes: WiFi SSID=32, password=64, MQTT strings=64-100 bytes
- Task naming: lowercase with underscores, max 16 chars (FreeRTOS limit)
- No dynamic memory in ISRs - increment counters only
- Queue depth typically 10 messages for inter-task communication
