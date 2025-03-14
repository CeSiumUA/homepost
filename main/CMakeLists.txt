idf_component_register(SRCS "main.c" "internal_storage.c" "ble_scanner.c" "ble_ibeacon.c" "tracker_scanner.c"
                        INCLUDE_DIRS "../inc"
                        REQUIRES esp_system freertos esp_wifi nvs_flash bt)