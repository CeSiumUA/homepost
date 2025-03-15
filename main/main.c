#include <stdio.h>
#include "internal_storage.h"
#include "tracker_scanner.h"
#include "wifi.h"
#include "http_server.h"

void app_main(void)
{
    internal_storage_init();

    wifi_init();

    if(internal_storage_check_wifi_credentials_preserved()){
        wifi_connect_sta(false);
    } else {
        wifi_start_softap();
        http_server_init();
        http_server_start();
    }

    tracker_scanner_start_task();
}