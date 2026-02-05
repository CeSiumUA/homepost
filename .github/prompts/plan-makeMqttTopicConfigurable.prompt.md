## Plan: Make MQTT Topic Runtime Configurable

Move MQTT topic from compile-time Kconfig to runtime configuration via web interface and NVS storage. The NVS storage functions already exist—main work involves updating the web form, HTTP handler with validation, and refactoring all components to build topic strings dynamically at initialization time.

### Steps

1. **Add MQTT topic input field** to [main/web/index.html](main/web/index.html) within the MQTT configuration form — simple text input named `mqtt-topic`, no default value display needed.

2. **Update POST handler** in [http_server.c](main/http_server.c) `mqtt_setup_post_handler` to parse `mqtt-topic` field, validate it (reject empty strings, check for invalid MQTT wildcard characters `+` and `#`), and call existing `internal_storage_save_mqtt_topic()`. Return HTTP 400 with error message if validation fails.

3. **Add topic provider function** in [mqtt_connection.c](main/mqtt_connection.c) — create `mqtt_connection_get_base_topic(char *topic_out)` that loads from NVS via `internal_storage_get_mqtt_topic()`, falling back to `CONFIG_HOMEPOST_MQTT_TOPIC` if not preserved. Expose via [mqtt_connection.h](inc/mqtt_connection.h).

4. **Refactor [mqtt_connection.c](main/mqtt_connection.c)** — change `version_message` topic from static literal to a 100-byte static buffer, populate via `snprintf(buf, size, "%s/homepost_version", base_topic)` in `mqtt_connection_publish_version()` before first use.

5. **Refactor [tracker_scanner.c](main/tracker_scanner.c)** — add static topic buffer, build `"{base}/phone_present"` in `tracker_scanner_start_task()` before publishing.

6. **Refactor [htu21_sensor.c](main/htu21_sensor.c)** — add static buffers for temperature and humidity topics, build `"{base}/temperature"` and `"{base}/humidity"` in `htu21_sensor_start()`.

7. **Refactor [geiger_counter.c](main/geiger_counter.c)** — add static topic buffer, build `"{base}/radiation"` in `geiger_counter_start()`.

8. **Update documentation** in [README.md](README.md) and [.github/copilot-instructions.md](.github/copilot-instructions.md) to note MQTT topic is now runtime-configurable via web interface.

9. **Bump version** in [sdkconfig](sdkconfig) `CONFIG_APP_PROJECT_VER`. Bump just a patch version, don't change major/minor.
