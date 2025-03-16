#include "internal_storage.h"
#include <string.h>

#define INTERNAL_STORAGE_NAMESPACE              "storage"

static const char *TAG = __FILE__;

void internal_storage_init(void){
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "Erasing NVS flash...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "Internal storage initialized");
}

bool internal_storage_check_wifi_credentials_preserved(void){
    nvs_handle_t nvs_handle;
    esp_err_t err;
    nvs_iterator_t nvs_it = NULL;
    bool wifi_credentials_preserved = false;

    err = nvs_open(INTERNAL_STORAGE_NAMESPACE, NVS_READONLY, &nvs_handle);

    if(err == ESP_ERR_NVS_NOT_FOUND){
        return false;
    }

    ESP_ERROR_CHECK(err);

    ESP_LOGI(TAG, "Opened NVS handle");

    err = nvs_entry_find_in_handle(nvs_handle, NVS_TYPE_STR, &nvs_it);

    while(err == ESP_OK){
        nvs_entry_info_t nvs_info;
        nvs_entry_info(nvs_it, &nvs_info);

        if(strcmp(nvs_info.key, CONFIG_HOMEPOST_WIFI_CREDENTIALS_STORAGE_KEY) == 0){
            ESP_LOGI(TAG, "WiFi credentials found in NVS");
            wifi_credentials_preserved = true;
            break;
        }

        err = nvs_entry_next(&nvs_it);
    }

    nvs_release_iterator(nvs_it);

    nvs_close(nvs_handle);

    return wifi_credentials_preserved;
}

esp_err_t internal_storage_save_wifi_credentials(const char *ssid, const char *password){
    nvs_handle_t nvs_handle;
    esp_err_t err;
    char wifi_credentials[96] = {0};
    size_t ssid_length = strlen(ssid);
    size_t password_length = strlen(password);

    if(ssid_length + password_length + 1 > 96){
        ESP_LOGE(TAG, "SSID and password too long");
        return ESP_FAIL;
    }

    strncpy(wifi_credentials, ssid, ssid_length);
    wifi_credentials[ssid_length] = '\n';
    strncpy(wifi_credentials + ssid_length + 1, password, password_length);

    err = nvs_open(INTERNAL_STORAGE_NAMESPACE, NVS_READWRITE, &nvs_handle);
    ESP_ERROR_CHECK(err);

    err = nvs_set_str(nvs_handle, CONFIG_HOMEPOST_WIFI_CREDENTIALS_STORAGE_KEY, wifi_credentials);
    ESP_ERROR_CHECK(err);

    err = nvs_commit(nvs_handle);
    ESP_ERROR_CHECK(err);

    nvs_close(nvs_handle);

    return ESP_OK;
}

esp_err_t internal_storage_get_wifi_credentials(char *ssid, char *password){
    nvs_handle_t nvs_handle;
    esp_err_t err;
    size_t ssid_length;
    size_t password_length;
    char *delimiter;
    char wifi_credentials[96] = {0};
    size_t credentials_len = (sizeof(wifi_credentials) / sizeof(wifi_credentials[0]));

    err = nvs_open(INTERNAL_STORAGE_NAMESPACE, NVS_READONLY, &nvs_handle);
    ESP_ERROR_CHECK(err);

    err = nvs_get_str(nvs_handle, CONFIG_HOMEPOST_WIFI_CREDENTIALS_STORAGE_KEY, wifi_credentials, &credentials_len);
    ESP_ERROR_CHECK(err);

    memset(ssid, 0, 32);
    memset(password, 0, 64);

    delimiter = strchr(wifi_credentials, '\n');

    if(delimiter == NULL){
        nvs_close(nvs_handle);
        return ESP_FAIL;
    }

    ssid_length = delimiter - wifi_credentials;

    strncpy(ssid, wifi_credentials, ssid_length);
    ssid[ssid_length] = '\0';

    password_length = strlen(wifi_credentials) - ssid_length - 1;

    strncpy(password, delimiter + 1, password_length);
    password[password_length] = '\0';

    nvs_close(nvs_handle);

    return ESP_OK;
}

esp_err_t internal_storage_erase_wifi_credentials(void){
    nvs_handle_t nvs_handle;
    esp_err_t err;

    err = nvs_open(INTERNAL_STORAGE_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if(err != ESP_OK){
        return err;
    }

    err = nvs_erase_key(nvs_handle, CONFIG_HOMEPOST_WIFI_CREDENTIALS_STORAGE_KEY);
    if(err != ESP_OK){
        return err;
    }

    err = nvs_commit(nvs_handle);
    if(err != ESP_OK){
        return err;
    }

    nvs_close(nvs_handle);

    return ESP_OK;
}