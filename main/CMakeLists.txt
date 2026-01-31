idf_component_register(SRCS "main.c" "internal_storage.c" "ble_scanner.c" "ble_ibeacon.c" "tracker_scanner.c" "wifi.c" "internal_storage.c" "http_server.c" "mqtt_connection.c" "geiger_counter.c" "htu21_sensor.c"
                        INCLUDE_DIRS "../inc"
                        EMBED_TXTFILES "web/index.html"
                        REQUIRES esp_event mqtt esp_wifi freertos nvs_flash bt esp_http_server esp_timer esp_system esp_driver_gpio esp_driver_i2c esp_common)