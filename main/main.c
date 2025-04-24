#include <stdio.h>
#include "internal_storage.h"
#include "tracker_scanner.h"
#include "wifi.h"
#include "http_server.h"
#include "esp_log.h"

const char *TAG = __FILE__;

void app_main(void)
{
    bool connected_to_ap = false;
    internal_storage_init();

    wifi_init();

    if(internal_storage_check_wifi_credentials_preserved()){
        connected_to_ap = wifi_connect_sta(false);
    }
    
    if(!connected_to_ap){
        ESP_LOGI(TAG, "WiFi credentials not found in NVS, starting softAP mode");
        wifi_start_softap();
    }

    http_server_init();
    http_server_start();

    tracker_scanner_start_task();
}