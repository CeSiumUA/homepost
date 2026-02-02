#include "tracker_scanner.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TRACKER_SCANNER_TASK_PRIORITY           6
#define TRACKER_SCANNER_TASK_STACK_SIZE         2048
#define TRACKER_SCANNER_TASK_NAME               "scanner"
#define TRACKER_SCANNER_EVENT_BIT               BIT0
#define TRACKER_SCANNER_SCAN_TIMEOUT            pdMS_TO_TICKS(CONFIG_HOMEPOST_SCAN_TIMEOUT_MINUTES * 60 * 1000)

static const char *TAG = __FILE__;
static EventGroupHandle_t tracker_scanner_event_group;
TaskHandle_t scanner_task_handle = NULL;
static char presence_payload[32] = {0};
static struct mqtt_connection_message_t presence_message = {
    .topic = CONFIG_HOMEPOST_MQTT_TOPIC "/phone_present",
    .payload = presence_payload,
    .qos = 0
};

static void tracker_scanner_cb(esp_ble_gap_cb_param_t *param){
    if(esp_ble_is_ibeacon_packet(param->scan_rst.ble_adv, param->scan_rst.adv_data_len)){
        esp_ble_ibeacon_t *ibeacon_data = (esp_ble_ibeacon_t*)(param->scan_rst.ble_adv);

        uint16_t major = ENDIAN_CHANGE_U16(ibeacon_data->ibeacon_vendor.major);
        uint16_t minor = ENDIAN_CHANGE_U16(ibeacon_data->ibeacon_vendor.minor);
        if (major == CONFIG_HOMEPOST_SCAN_MAJOR_FILTER && minor == CONFIG_HOMEPOST_SCAN_MINOR_FILTER){
#ifdef CONFIG_HOMEPOST_SCAN_USE_RSSI_FILTER
            if (param->scan_rst.rssi > CONFIG_HOMEPOST_SCAN_RSSI_THRESHOLD){
                xEventGroupSetBits(tracker_scanner_event_group, TRACKER_SCANNER_EVENT_BIT);
            }
#else
            ESP_LOGI(TAG, "iBeacon found (RSSI: %d dB)", param->scan_rst.rssi);
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
        bool tracker_present = false;
        int ret = 0;

        EventBits_t bits = xEventGroupWaitBits(tracker_scanner_event_group, TRACKER_SCANNER_EVENT_BIT, pdTRUE, pdFALSE, TRACKER_SCANNER_SCAN_TIMEOUT);
        if (bits & TRACKER_SCANNER_EVENT_BIT){
            ESP_LOGI(TAG, "Tracker found");
            tracker_present = true;
        }
        else{
            ESP_LOGI(TAG, "Tracker lost");
        }
        memset(presence_payload, 0, sizeof(presence_payload));
        ret = snprintf(presence_payload, sizeof(presence_payload), "{\"state\": \"%s\"}", tracker_present ? "ON" : "OFF");
        if (ret < 0 || ret >= sizeof(presence_payload)) {
            ESP_LOGE(TAG, "Failed to create presence payload");
            return;
        }

        if(mqtt_connection_put_publish_queue(&presence_message) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to enqueue presence message");
        }
    }
}

void tracker_scanner_start_task(void){
    if (scanner_task_handle != NULL) {
        ESP_LOGW(TAG, "Tracker scanner task already running");
        return;
    }
    tracker_scanner_event_group = xEventGroupCreate();
    xTaskCreate(tracker_scanner_task, TRACKER_SCANNER_TASK_NAME, TRACKER_SCANNER_TASK_STACK_SIZE, NULL, TRACKER_SCANNER_TASK_PRIORITY, &scanner_task_handle);
    configASSERT(scanner_task_handle);
}

void tracker_scanner_stop_task(void){
    if (scanner_task_handle != NULL) {
        vTaskDelete(scanner_task_handle);
        scanner_task_handle = NULL;
        ESP_LOGI(TAG, "Tracker scanner task stopped");
    }
    if (tracker_scanner_event_group != NULL) {
        vEventGroupDelete(tracker_scanner_event_group);
        tracker_scanner_event_group = NULL;
    }
}