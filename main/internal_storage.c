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

esp_err_t internal_storage_save_mqtt_client_id(const char *client_id){
    nvs_handle_t nvs_handle;
    esp_err_t err;

    err = nvs_open(INTERNAL_STORAGE_NAMESPACE, NVS_READWRITE, &nvs_handle);
    ESP_ERROR_CHECK(err);

    err = nvs_set_str(nvs_handle, CONFIG_HOMEPOST_MQTT_CLIENT_ID_STORAGE_KEY, client_id);
    ESP_ERROR_CHECK(err);

    err = nvs_commit(nvs_handle);
    ESP_ERROR_CHECK(err);

    nvs_close(nvs_handle);

    return ESP_OK;
}

esp_err_t internal_storage_get_mqtt_client_id(char *client_id){
    nvs_handle_t nvs_handle;
    esp_err_t err;
    size_t client_id_length;
    char mqtt_client_id[32] = {0};
    size_t client_id_len = (sizeof(mqtt_client_id) / sizeof(mqtt_client_id[0]));

    err = nvs_open(INTERNAL_STORAGE_NAMESPACE, NVS_READONLY, &nvs_handle);
    ESP_ERROR_CHECK(err);

    err = nvs_get_str(nvs_handle, CONFIG_HOMEPOST_MQTT_CLIENT_ID_STORAGE_KEY, mqtt_client_id, &client_id_len);
    ESP_ERROR_CHECK(err);

    client_id_length = strlen(mqtt_client_id);

    strncpy(client_id, mqtt_client_id, client_id_length);
    client_id[client_id_length] = '\0';

    nvs_close(nvs_handle);

    return ESP_OK;
}

bool internal_storage_check_mqtt_client_id_preserved(void){
    nvs_handle_t nvs_handle;
    esp_err_t err;
    nvs_iterator_t nvs_it = NULL;
    bool mqtt_client_id_preserved = false;

    err = nvs_open(INTERNAL_STORAGE_NAMESPACE, NVS_READONLY, &nvs_handle);

    if(err == ESP_ERR_NVS_NOT_FOUND){
        return false;
    }

    ESP_ERROR_CHECK(err);

    err = nvs_entry_find_in_handle(nvs_handle, NVS_TYPE_STR, &nvs_it);

    while(err == ESP_OK){
        nvs_entry_info_t nvs_info;
        nvs_entry_info(nvs_it, &nvs_info);

        if(strcmp(nvs_info.key, CONFIG_HOMEPOST_MQTT_CLIENT_ID_STORAGE_KEY) == 0){
            mqtt_client_id_preserved = true;
            break;
        }

        err = nvs_entry_next(&nvs_it);
    }

    nvs_release_iterator(nvs_it);

    nvs_close(nvs_handle);

    return mqtt_client_id_preserved;
}

esp_err_t internal_storage_save_mqtt_broker(const char *broker){
    nvs_handle_t nvs_handle;
    esp_err_t err;

    err = nvs_open(INTERNAL_STORAGE_NAMESPACE, NVS_READWRITE, &nvs_handle);
    ESP_ERROR_CHECK(err);

    err = nvs_set_str(nvs_handle, CONFIG_HOMEPOST_MQTT_BROKER_STORAGE_KEY, broker);
    ESP_ERROR_CHECK(err);

    err = nvs_commit(nvs_handle);
    ESP_ERROR_CHECK(err);

    nvs_close(nvs_handle);

    return ESP_OK;
}

esp_err_t internal_storage_get_mqtt_broker(char *broker){
    nvs_handle_t nvs_handle;
    esp_err_t err;
    size_t broker_length;
    char mqtt_broker[100] = {0};
    size_t broker_len = (sizeof(mqtt_broker) / sizeof(mqtt_broker[0]));

    err = nvs_open(INTERNAL_STORAGE_NAMESPACE, NVS_READONLY, &nvs_handle);
    ESP_ERROR_CHECK(err);

    err = nvs_get_str(nvs_handle, CONFIG_HOMEPOST_MQTT_BROKER_STORAGE_KEY, mqtt_broker, &broker_len);
    ESP_ERROR_CHECK(err);

    broker_length = strlen(mqtt_broker);

    strncpy(broker, mqtt_broker, broker_length);
    broker[broker_length] = '\0';

    nvs_close(nvs_handle);

    return ESP_OK;
}

bool internal_storage_check_mqtt_broker_preserved(void){
    nvs_handle_t nvs_handle;
    esp_err_t err;
    nvs_iterator_t nvs_it = NULL;
    bool mqtt_broker_preserved = false;

    err = nvs_open(INTERNAL_STORAGE_NAMESPACE, NVS_READONLY, &nvs_handle);

    if(err == ESP_ERR_NVS_NOT_FOUND){
        return false;
    }

    ESP_ERROR_CHECK(err);

    err = nvs_entry_find_in_handle(nvs_handle, NVS_TYPE_STR, &nvs_it);

    while(err == ESP_OK){
        nvs_entry_info_t nvs_info;
        nvs_entry_info(nvs_it, &nvs_info);

        if(strcmp(nvs_info.key, CONFIG_HOMEPOST_MQTT_BROKER_STORAGE_KEY) == 0){
            mqtt_broker_preserved = true;
            break;
        }

        err = nvs_entry_next(&nvs_it);
    }

    nvs_release_iterator(nvs_it);

    nvs_close(nvs_handle);

    return mqtt_broker_preserved;
}

esp_err_t internal_storage_save_mqtt_port(uint16_t port){
    nvs_handle_t nvs_handle;
    esp_err_t err;

    err = nvs_open(INTERNAL_STORAGE_NAMESPACE, NVS_READWRITE, &nvs_handle);
    ESP_ERROR_CHECK(err);

    err = nvs_set_u16(nvs_handle, CONFIG_HOMEPOST_MQTT_PORT_STORAGE_KEY, port);
    ESP_ERROR_CHECK(err);

    err = nvs_commit(nvs_handle);
    ESP_ERROR_CHECK(err);

    nvs_close(nvs_handle);

    return ESP_OK;
}

esp_err_t internal_storage_get_mqtt_port(uint16_t *port){
    nvs_handle_t nvs_handle;
    esp_err_t err;
    uint16_t mqtt_port;

    err = nvs_open(INTERNAL_STORAGE_NAMESPACE, NVS_READONLY, &nvs_handle);
    ESP_ERROR_CHECK(err);

    err = nvs_get_u16(nvs_handle, CONFIG_HOMEPOST_MQTT_PORT_STORAGE_KEY, &mqtt_port);
    ESP_ERROR_CHECK(err);

    *port = mqtt_port;

    nvs_close(nvs_handle);

    return ESP_OK;
}

bool internal_storage_check_mqtt_port_preserved(void){
    nvs_handle_t nvs_handle;
    esp_err_t err;
    nvs_iterator_t nvs_it = NULL;
    bool mqtt_port_preserved = false;

    err = nvs_open(INTERNAL_STORAGE_NAMESPACE, NVS_READONLY, &nvs_handle);

    if(err == ESP_ERR_NVS_NOT_FOUND){
        return false;
    }

    ESP_ERROR_CHECK(err);

    err = nvs_entry_find_in_handle(nvs_handle, NVS_TYPE_U16, &nvs_it);

    while(err == ESP_OK){
        nvs_entry_info_t nvs_info;
        nvs_entry_info(nvs_it, &nvs_info);

        if(strcmp(nvs_info.key, CONFIG_HOMEPOST_MQTT_PORT_STORAGE_KEY) == 0){
            mqtt_port_preserved = true;
            break;
        }

        err = nvs_entry_next(&nvs_it);
    }

    nvs_release_iterator(nvs_it);

    nvs_close(nvs_handle);

    return mqtt_port_preserved;
}

esp_err_t internal_storage_save_mqtt_username(const char *username){
    nvs_handle_t nvs_handle;
    esp_err_t err;

    err = nvs_open(INTERNAL_STORAGE_NAMESPACE, NVS_READWRITE, &nvs_handle);
    ESP_ERROR_CHECK(err);

    err = nvs_set_str(nvs_handle, CONFIG_HOMEPOST_MQTT_USERNAME_STORAGE_KEY, username);
    ESP_ERROR_CHECK(err);

    err = nvs_commit(nvs_handle);
    ESP_ERROR_CHECK(err);

    nvs_close(nvs_handle);

    return ESP_OK;
}

esp_err_t internal_storage_get_mqtt_username(char *username){
    nvs_handle_t nvs_handle;
    esp_err_t err;
    size_t username_length;
    char mqtt_username[32] = {0};
    size_t username_len = (sizeof(mqtt_username) / sizeof(mqtt_username[0]));

    err = nvs_open(INTERNAL_STORAGE_NAMESPACE, NVS_READONLY, &nvs_handle);
    ESP_ERROR_CHECK(err);

    err = nvs_get_str(nvs_handle, CONFIG_HOMEPOST_MQTT_USERNAME_STORAGE_KEY, mqtt_username, &username_len);
    ESP_ERROR_CHECK(err);

    username_length = strlen(mqtt_username);

    strncpy(username, mqtt_username, username_length);
    username[username_length] = '\0';

    nvs_close(nvs_handle);

    return ESP_OK;
}

bool internal_storage_check_mqtt_username_preserved(void){
    nvs_handle_t nvs_handle;
    esp_err_t err;
    nvs_iterator_t nvs_it = NULL;
    bool mqtt_username_preserved = false;

    err = nvs_open(INTERNAL_STORAGE_NAMESPACE, NVS_READONLY, &nvs_handle);

    if(err == ESP_ERR_NVS_NOT_FOUND){
        return false;
    }

    ESP_ERROR_CHECK(err);

    err = nvs_entry_find_in_handle(nvs_handle, NVS_TYPE_STR, &nvs_it);

    while(err == ESP_OK){
        nvs_entry_info_t nvs_info;
        nvs_entry_info(nvs_it, &nvs_info);

        if(strcmp(nvs_info.key, CONFIG_HOMEPOST_MQTT_USERNAME_STORAGE_KEY) == 0){
            mqtt_username_preserved = true;
            break;
        }

        err = nvs_entry_next(&nvs_it);
    }

    nvs_release_iterator(nvs_it);

    nvs_close(nvs_handle);

    return mqtt_username_preserved;
}

esp_err_t internal_storage_save_mqtt_password(const char *password){
    nvs_handle_t nvs_handle;
    esp_err_t err;

    err = nvs_open(INTERNAL_STORAGE_NAMESPACE, NVS_READWRITE, &nvs_handle);
    ESP_ERROR_CHECK(err);

    err = nvs_set_str(nvs_handle, CONFIG_HOMEPOST_MQTT_PASSWORD_STORAGE_KEY, password);
    ESP_ERROR_CHECK(err);

    err = nvs_commit(nvs_handle);
    ESP_ERROR_CHECK(err);

    nvs_close(nvs_handle);

    return ESP_OK;
}

esp_err_t internal_storage_get_mqtt_password(char *password){
    nvs_handle_t nvs_handle;
    esp_err_t err;
    size_t password_length;
    char mqtt_password[64] = {0};
    size_t password_len = (sizeof(mqtt_password) / sizeof(mqtt_password[0]));

    err = nvs_open(INTERNAL_STORAGE_NAMESPACE, NVS_READONLY, &nvs_handle);
    ESP_ERROR_CHECK(err);

    err = nvs_get_str(nvs_handle, CONFIG_HOMEPOST_MQTT_PASSWORD_STORAGE_KEY, mqtt_password, &password_len);
    ESP_ERROR_CHECK(err);

    password_length = strlen(mqtt_password);

    strncpy(password, mqtt_password, password_length);
    password[password_length] = '\0';

    nvs_close(nvs_handle);

    return ESP_OK;
}

bool internal_storage_check_mqtt_password_preserved(void){
    nvs_handle_t nvs_handle;
    esp_err_t err;
    nvs_iterator_t nvs_it = NULL;
    bool mqtt_password_preserved = false;

    err = nvs_open(INTERNAL_STORAGE_NAMESPACE, NVS_READONLY, &nvs_handle);

    if(err == ESP_ERR_NVS_NOT_FOUND){
        return false;
    }

    ESP_ERROR_CHECK(err);

    err = nvs_entry_find_in_handle(nvs_handle, NVS_TYPE_STR, &nvs_it);

    while(err == ESP_OK){
        nvs_entry_info_t nvs_info;
        nvs_entry_info(nvs_it, &nvs_info);

        if(strcmp(nvs_info.key, CONFIG_HOMEPOST_MQTT_PASSWORD_STORAGE_KEY) == 0){
            mqtt_password_preserved = true;
            break;
        }

        err = nvs_entry_next(&nvs_it);
    }

    nvs_release_iterator(nvs_it);

    nvs_close(nvs_handle);

    return mqtt_password_preserved;
}

esp_err_t internal_storage_save_mqtt_topic(const char *topic){
    nvs_handle_t nvs_handle;
    esp_err_t err;

    err = nvs_open(INTERNAL_STORAGE_NAMESPACE, NVS_READWRITE, &nvs_handle);
    ESP_ERROR_CHECK(err);

    err = nvs_set_str(nvs_handle, CONFIG_HOMEPOST_MQTT_TOPIC_STORAGE_KEY, topic);
    ESP_ERROR_CHECK(err);

    err = nvs_commit(nvs_handle);
    ESP_ERROR_CHECK(err);

    nvs_close(nvs_handle);

    return ESP_OK;
}

esp_err_t internal_storage_get_mqtt_topic(char *topic){
    nvs_handle_t nvs_handle;
    esp_err_t err;
    size_t topic_length;
    char mqtt_topic[64] = {0};
    size_t topic_len = (sizeof(mqtt_topic) / sizeof(mqtt_topic[0]));

    err = nvs_open(INTERNAL_STORAGE_NAMESPACE, NVS_READONLY, &nvs_handle);
    ESP_ERROR_CHECK(err);

    err = nvs_get_str(nvs_handle, CONFIG_HOMEPOST_MQTT_TOPIC_STORAGE_KEY, mqtt_topic, &topic_len);
    ESP_ERROR_CHECK(err);

    topic_length = strlen(mqtt_topic);

    strncpy(topic, mqtt_topic, topic_length);
    topic[topic_length] = '\0';

    nvs_close(nvs_handle);

    return ESP_OK;
}

bool internal_storage_check_mqtt_topic_preserved(void){
    nvs_handle_t nvs_handle;
    esp_err_t err;
    nvs_iterator_t nvs_it = NULL;
    bool mqtt_topic_preserved = false;

    err = nvs_open(INTERNAL_STORAGE_NAMESPACE, NVS_READONLY, &nvs_handle);

    if(err == ESP_ERR_NVS_NOT_FOUND){
        return false;
    }

    ESP_ERROR_CHECK(err);

    err = nvs_entry_find_in_handle(nvs_handle, NVS_TYPE_STR, &nvs_it);

    while(err == ESP_OK){
        nvs_entry_info_t nvs_info;
        nvs_entry_info(nvs_it, &nvs_info);

        if(strcmp(nvs_info.key, CONFIG_HOMEPOST_MQTT_TOPIC_STORAGE_KEY) == 0){
            mqtt_topic_preserved = true;
            break;
        }

        err = nvs_entry_next(&nvs_it);
    }

    nvs_release_iterator(nvs_it);

    nvs_close(nvs_handle);

    return mqtt_topic_preserved;
}