idf_component_register(SRCS "main.c" "internal_storage.c" "ble_scanner.c" "ble_ibeacon.c" "tracker_scanner.c" "wifi.c" "internal_storage.c" "http_server.c"
                        INCLUDE_DIRS "../inc"
                        EMBED_TXTFILES "web/index.html"
                        REQUIRES esp_event mqtt esp_wifi freertos nvs_flash bt esp_http_server esp_timer esp_system)