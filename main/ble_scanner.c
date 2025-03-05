#include "ble_scanner.h"

static const char *TAG = __FILE__;

static esp_ble_scan_params_t ble_scan_params = {
    .scan_type              = BLE_SCAN_TYPE_ACTIVE,
    .own_addr_type          = BLE_ADDR_TYPE_PUBLIC,
    .scan_filter_policy     = BLE_SCAN_FILTER_ALLOW_ALL,
    .scan_interval          = 0x50,
    .scan_window            = 0x30
};

static void ble_scanner_get_device_name(esp_ble_gap_cb_param_t *param, char *name, int name_len){
    uint8_t *adv_name = NULL;
    uint8_t adv_name_len = 0;

    uint8_t *adv_addr = param->scan_rst.bda;
    ESP_LOGI(TAG, "MAC: %02x:%02x:%02x:%02x:%02x:%02x", 
             adv_addr[0], adv_addr[1], adv_addr[2], 
             adv_addr[3], adv_addr[4], adv_addr[5]);
    // adv_name = esp_ble_resolve_adv_data(param->scan_rst.ble_adv, ESP_BLE_AD_TYPE_NAME_CMPL, &adv_name_len);

    // if (adv_name != NULL && adv_name_len > 0) {
    //     snprintf(name, name_len, "%.*s", adv_name_len, adv_name);
    // }
    // else {
    //     snprintf(name, name_len, "Unknown device");
    // }

    // snprintf(name, "MAC: %02x:%02x:%02x:%02x:%02x:%02x", 
    //          adv_addr[0], adv_addr[1], adv_addr[2], 
    //          adv_addr[3], adv_addr[4], adv_addr[5]);
}

static void ble_scanner_gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param){
    char name[64];

    switch (event) {
        case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT:
            ESP_LOGI(TAG, "ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT");
            if (param->scan_param_cmpl.status == ESP_BT_STATUS_SUCCESS) {
                ESP_LOGI(TAG, "Scan parameters set successfully");
                esp_ble_gap_start_scanning(10);
            }
            else {
                ESP_LOGE(TAG, "Unable to set scan parameters");
            }
            break;
        case ESP_GAP_BLE_SCAN_START_COMPLETE_EVT:
            ESP_LOGI(TAG, "ESP_GAP_BLE_SCAN_START_COMPLETE_EVT");
            break;
        case ESP_GAP_BLE_SCAN_RESULT_EVT:
            if (param->scan_rst.search_evt == ESP_GAP_SEARCH_INQ_RES_EVT){
                memset(name, 0, sizeof(name));
                ble_scanner_get_device_name(param, name, sizeof(name));
                // ESP_LOGI(TAG, "Device found: %s, RSSI: %d dB", name, param->scan_rst.rssi);
            }
            break;
        case ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT:
            ESP_LOGI(TAG, "ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT");
            break;
        default:
            break;
    }
}

esp_err_t ble_scanner_init(void){
    esp_err_t ret;

    ESP_LOGI(TAG, "Initializing a BLE scanner...");

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ret = esp_bt_controller_init(&bt_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_bt_controller_init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_bt_controller_enable failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_bluedroid_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_bluedroid_init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_bluedroid_enable();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_bluedroid_enable failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "BLE scanner initialized");

    return ESP_OK;
}

esp_err_t ble_scanner_start(void){
    esp_err_t ret;

    ESP_LOGI(TAG, "Starting a BLE scanner...");

    ret = esp_ble_gap_register_callback(ble_scanner_gap_cb);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_ble_gap_register_callback failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_ble_gap_set_scan_params(&ble_scan_params);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Scan parameters set successfully");
    }
    else {
        ESP_LOGE(TAG, "Unable to set scan parameters");
    }

    return ret;
}