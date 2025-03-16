#include "wifi.h"

static const char *TAG = __FILE__;

#define WIFI_CONNECTED_BIT                                              BIT0
#define WIFI_FAIL_BIT                                                   BIT1

static int connection_retries = 0;
static EventGroupHandle_t wifi_event_group;

static esp_netif_t *netif_ap = NULL;
static esp_netif_t *netif_sta = NULL;
static bool is_connected_to_internet = false;
static bool is_device_softap_mode = false;

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data){
    if(event_base == WIFI_EVENT){
        switch(event_id){
            case WIFI_EVENT_STA_START:
                ESP_LOGI(TAG, "WiFi started");
                esp_wifi_connect();
                break;
            case WIFI_EVENT_STA_DISCONNECTED:
                if(connection_retries < CONFIG_HOMEPOST_WIFI_STA_MAX_RETRIES){
                    esp_wifi_connect();
                    connection_retries++;
                    ESP_LOGI(TAG, "Connection retry %d", connection_retries);
                } else {
                    xEventGroupSetBits(wifi_event_group, WIFI_FAIL_BIT);
                    ESP_LOGE(TAG, "Connection failed after %d retries", connection_retries);
                    if(!is_device_softap_mode){
                        ESP_LOGW(TAG, "Device will restart now!");
                        esp_restart();
                    }
                }
                break;
            case WIFI_EVENT_AP_STACONNECTED:
                wifi_event_ap_staconnected_t* conn_event = (wifi_event_ap_staconnected_t*) event_data;
                ESP_LOGI(TAG, "Station "MACSTR" join, AID=%d", MAC2STR(conn_event ->mac), conn_event ->aid);
                break;
            case WIFI_EVENT_AP_STADISCONNECTED:
                wifi_event_ap_stadisconnected_t* disconn_event = (wifi_event_ap_stadisconnected_t*) event_data;
                ESP_LOGI(TAG, "Station "MACSTR" leave, AID=%d, reason=%d", MAC2STR(disconn_event->mac), disconn_event->aid, disconn_event->reason);
                break;
            default:
                break;
        }
    }
    else if(event_base == IP_EVENT){
        switch(event_id){
            case IP_EVENT_STA_GOT_IP:
                ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
                ESP_LOGI(TAG, "Got IP:" IPSTR, IP2STR(&event->ip_info.ip));
                connection_retries = 0;
                xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
                is_connected_to_internet = true;
                break;
            case IP_EVENT_STA_LOST_IP:
                ESP_LOGI(TAG, "Lost IP");
                is_connected_to_internet = false;
                ESP_LOGW(TAG, "Device will restart now!");
                esp_restart();
                break;
            default:
                break;
        }
    }
}

void wifi_init(void){
    wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    vTaskDelay(pdMS_TO_TICKS( 100 ));

    netif_ap = esp_netif_create_default_wifi_ap();
    assert(netif_ap);

    netif_sta = esp_netif_create_default_wifi_sta();
    assert(netif_sta);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));

    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_NULL));
    ESP_ERROR_CHECK(esp_wifi_start());
}

void wifi_start_softap(void){
    ESP_LOGI(TAG, "Starting softAP");

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = CONFIG_HOMEPOST_WIFI_SOFTAP_SSID,
            .ssid_len = strlen(CONFIG_HOMEPOST_WIFI_SOFTAP_SSID),
            .password = CONFIG_HOMEPOST_WIFI_SOFTAP_PASSWORD,
            .max_connection = 2,
            .authmode = WIFI_AUTH_WPA2_PSK,
        }
    };
    
    if(strlen(CONFIG_HOMEPOST_WIFI_SOFTAP_PASSWORD) == 0){
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_LOGI(TAG, "SoftAP started");
}

bool wifi_connect_sta(bool is_softap_mode){
    ESP_LOGI(TAG, "Connecting to WiFi network");

    wifi_config_t wifi_config = {
        .sta ={
        }
    };

    is_device_softap_mode = is_softap_mode;
    
    ESP_ERROR_CHECK(internal_storage_get_wifi_credentials((char *)(wifi_config.sta.ssid), (char *)(wifi_config.sta.password)));

    ESP_LOGI(TAG, "Connecting to %s", wifi_config.sta.ssid);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_netif_set_hostname(netif_sta, CONFIG_HOMEPOST_WIFI_SOFTAP_HOSTNAME));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    esp_wifi_connect();

    ESP_LOGI(TAG, "WiFi network connection started");

    EventBits_t bits = xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, pdFALSE, pdFALSE, portMAX_DELAY);
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "connected to ap");
        return true;
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Failed to connect to AP");
        if(is_device_softap_mode){
            ESP_LOGW(TAG, "Device is in SoftAP mode, so preserved credentials could be considered as invalid! Erasing...");
            internal_storage_erase_wifi_credentials();
        }
        return false;
    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
        return false;
    }
}

bool wifi_is_connected_to_internet(void){
    wifi_ap_record_t ap_info;
    esp_err_t err;
    err = esp_wifi_sta_get_ap_info(&ap_info);

    ESP_UNUSED(ap_info);

    return is_connected_to_internet && (err == ESP_OK);
}