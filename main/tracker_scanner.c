#include "tracker_scanner.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TRACKER_SCANNER_TASK_PRIORITY           6
#define TRACKER_SCANNER_TASK_STACK_SIZE         4096
#define TRACKER_SCANNER_TASK_NAME               "scanner"
#define TRACKER_SCANNER_EVENT_BIT               BIT0
#define TRACKER_SCANNER_SCAN_TIMEOUT            pdMS_TO_TICKS(5 * 60 * 1000)

static const char *TAG = __FILE__;
static EventGroupHandle_t tracker_scanner_event_group;

static void tracker_scanner_cb(esp_ble_gap_cb_param_t *param){
    if(esp_ble_is_ibeacon_packet(param->scan_rst.ble_adv, param->scan_rst.adv_data_len)){
        ESP_LOGI(TAG, "iBeacon found (RSSI: %d dB)", param->scan_rst.rssi);
        esp_ble_ibeacon_t *ibeacon_data = (esp_ble_ibeacon_t*)(param->scan_rst.ble_adv);
        if (ibeacon_data->ibeacon_vendor.major == CONFIG_HOMEPOST_SCAN_MAJOR && ibeacon_data->ibeacon_vendor.minor == CONFIG_HOMEPOST_SCAN_MINOR){
#ifdef CONFIG_HOMEPOST_SCAN_USE_RSSI_FILTER
            if (param->scan_rst.rssi > CONFIG_HOMEPOST_SCAN_RSSI_THRESHOLD){
                xEventGroupSetBits(tracker_scanner_event_group, TRACKER_SCANNER_EVENT_BIT);
            }
#else
            xEventGroupSetBits(tracker_scanner_event_group, TRACKER_SCANNER_EVENT_BIT);
#endif
        }
    }
}

static esp_err_t tracker_scanner_start(void){
    esp_err_t ret;

    ret = ble_scanner_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ble_scanner_init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = ble_scanner_start(tracker_scanner_cb);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ble_scanner_start failed: %s", esp_err_to_name(ret));
        return ret;
    }

    return ESP_OK;
}

static void tracker_scanner_task(void *arg){
    esp_err_t ret;

    ret = tracker_scanner_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "tracker_scanner_start failed: %s", esp_err_to_name(ret));
        vTaskDelete(NULL);
    }

    while(true){
        EventBits_t bits = xEventGroupWaitBits(tracker_scanner_event_group, TRACKER_SCANNER_EVENT_BIT, pdTRUE, pdFALSE, TRACKER_SCANNER_SCAN_TIMEOUT);
        if (bits & TRACKER_SCANNER_EVENT_BIT){
            ESP_LOGI(TAG, "Tracker found");
        }
        else{
            ESP_LOGI(TAG, "Tracker lost");
        }
    }
}

void tracker_scanner_start_task(void){
    tracker_scanner_event_group = xEventGroupCreate();
    xTaskCreate(tracker_scanner_task, TRACKER_SCANNER_TASK_NAME, TRACKER_SCANNER_TASK_STACK_SIZE, NULL, TRACKER_SCANNER_TASK_PRIORITY, NULL);
}