#include <stdio.h>
#include "internal_storage.h"
#include "tracker_scanner.h"
#include "wifi.h"
#include "http_server.h"
#include "geiger_counter.h"
#include "htu21_sensor.h"
#include "esp_log.h"
#include <esp_timer.h>

static void wifi_reconnection_timer_cb(void *arg);

static esp_timer_handle_t wifi_reconnection_timer;

static const esp_timer_create_args_t wifi_reconnection_timer_args = {
    .callback = &wifi_reconnection_timer_cb,
};

static const char *TAG = __FILE__;

static void wifi_reconnection_timer_cb(void *arg)
{
    bool reconnection_succeeded = false;
    reconnection_succeeded = wifi_connect_sta(false);
    if(reconnection_succeeded){
        ESP_LOGI(TAG, "WiFi reconnection succeeded");
        esp_timer_stop(wifi_reconnection_timer);
    }
    else{
        ESP_LOGI(TAG, "WiFi reconnection failed");
    }
}

void app_main(void)
{
    bool connected_to_ap = false;
    internal_storage_init();

    wifi_init();

    if(internal_storage_check_wifi_credentials_preserved()){
        connected_to_ap = wifi_connect_sta(false);
        if (!connected_to_ap){
            ESP_LOGI(TAG, "WiFi connection failed, starting reconnection timer");
            esp_timer_create(&wifi_reconnection_timer_args, &wifi_reconnection_timer);
            esp_timer_start_periodic(wifi_reconnection_timer, CONFIG_HOMEPOST_WIFI_RECONNECTION_TIMER_PERIOD_US);
        }
        else{
            ESP_LOGI(TAG, "WiFi connection succeeded");
        }
    }
    
    if(!connected_to_ap){
        ESP_LOGI(TAG, "WiFi credentials not found in NVS, starting softAP mode");
        wifi_start_softap();
    }

    http_server_init();
    http_server_start();

    tracker_scanner_start_task();
    mqtt_connection_start_task();
    geiger_counter_start();
    htu21_sensor_start();
}