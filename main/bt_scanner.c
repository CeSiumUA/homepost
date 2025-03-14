#include "bt_scanner.h"

static const char *TAG = __FILE__;

void bt_scanner_gap_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param){
    char bda_str[18];
    char uuid_str[37];

    switch(event){
        case ESP_BT_GAP_READ_REMOTE_NAME_EVT:
            ESP_LOGI(TAG, "ESP_BT_GAP_READ_REMOTE_NAME_EVT, Remote name: %s", param->read_rmt_name.rmt_name);
            break;
        default:
            ESP_LOGI(TAG, "Event: %d", event);
            break;
    }
}

esp_err_t bt_scanner_start(void){
    esp_err_t ret;
    ESP_LOGI(TAG, "Starting a BT scanner...");

    esp_bt_gap_set_device_name(CONFIG_HOMEPOST_BT_DEV_NAME);

    ESP_ERROR_CHECK(esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE));

    ESP_ERROR_CHECK(esp_bt_gap_register_callback(bt_scanner_gap_cb));

    ret = esp_bt_gap_start_discovery(ESP_BT_INQ_MODE_GENERAL_INQUIRY, CONFIG_HOMEPOST_BT_INQ_LEN, 0);
    if(ret != ESP_OK){
        ESP_LOGE(TAG, "Failed to start a BT scanner");
        return ret;
    }

    return ESP_OK;
}

esp_err_t bt_scanner_init(void){
    ESP_LOGI(TAG, "Initializing a BT scanner...");    
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_BLE));

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_bt_controller_init(&bt_cfg));

    ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT));

    ESP_ERROR_CHECK(esp_bluedroid_init());

    ESP_ERROR_CHECK(esp_bluedroid_enable());

    return ESP_OK;
}