#include "mqtt_connection.h"

#define MQTT_CONNECTION_TOPIC_MAX_LEN                       64
#define MQTT_CONNECTION_TASK_PRIORITY                       7
#define MQTT_CONNECTION_STACK_SIZE                          3072
#define MQTT_CONNECTION_TASK_NAME                           "mqtt_conn"
#define MQTT_CONNECTION_CONNECTED_EVENT_BIT                 BIT0
#define MQTT_CONNECTION_CONNECTION_ERROR_EVENT_BIT          BIT1
#define MQTT_CONNECTION_PUBLISH_EVENT_BIT                   BIT2

static const char *TAG = __FILE__;

QueueHandle_t mqtt_connection_message_queue = NULL;

static EventGroupHandle_t mqtt_connection_event_group;
static esp_mqtt_client_handle_t client = NULL;
static TaskHandle_t mqtt_connection_task_handle = NULL;
static bool mqtt_connection_task_running = false;

static char version_payload[32];
static char version_topic[100];
static struct mqtt_connection_message_t version_message = {
    .topic = version_topic,
    .payload = version_payload,
    .qos = 1
};

static void mqtt_connection_subscribe_topics(void){

}

static void mqtt_connection_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data){
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32, base, event_id);
    switch((esp_mqtt_event_id_t)event_id){
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
            mqtt_connection_subscribe_topics();
            xEventGroupSetBits(mqtt_connection_event_group, MQTT_CONNECTION_CONNECTED_EVENT_BIT);
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
            xEventGroupClearBits(mqtt_connection_event_group, MQTT_CONNECTION_CONNECTED_EVENT_BIT);
            break;
        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGD(TAG, "MQTT_EVENT_SUBSCRIBED");
            break;
        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGD(TAG, "MQTT_EVENT_UNSUBSCRIBED");
            break;
        case MQTT_EVENT_PUBLISHED:
            ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED");
            xEventGroupSetBits(mqtt_connection_event_group, MQTT_CONNECTION_PUBLISH_EVENT_BIT);
            break;
        case MQTT_EVENT_DATA:
            ESP_LOGD(TAG, "MQTT_EVENT_DATA");
            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "MQTT_EVENT_ERROR: %d", *(int *)event_data);
            xEventGroupSetBits(mqtt_connection_event_group, MQTT_CONNECTION_CONNECTION_ERROR_EVENT_BIT);
            break;
        default:
            ESP_LOGW(TAG, "Unknown event ID: %ld", event_id);
            break;
    }
}

static esp_err_t mqtt_connection_start(void){
    bool mqtt_broker_connection_allowed = true;
    char mqtt_broker[100] = {0};
    uint16_t mqtt_port = 0;
    char mqtt_username[64] = {0};
    char mqtt_password[64] = {0};
    char mqtt_client_id[64] = {0};
    char mqtt_broker_full_uri[128] = {0};

    mqtt_broker_connection_allowed &= internal_storage_check_mqtt_broker_preserved();
    mqtt_broker_connection_allowed &= internal_storage_check_mqtt_port_preserved();
    mqtt_broker_connection_allowed &= internal_storage_check_mqtt_username_preserved();
    mqtt_broker_connection_allowed &= internal_storage_check_mqtt_client_id_preserved();
    mqtt_broker_connection_allowed &= internal_storage_check_mqtt_password_preserved();

    if(!mqtt_broker_connection_allowed){
        ESP_LOGE(TAG, "MQTT broker connection not allowed, missing credentials");
        return ESP_ERR_INVALID_STATE;
    }

    ESP_ERROR_CHECK(internal_storage_get_mqtt_broker(mqtt_broker));
    ESP_ERROR_CHECK(internal_storage_get_mqtt_port(&mqtt_port));
    ESP_ERROR_CHECK(internal_storage_get_mqtt_username(mqtt_username));
    ESP_ERROR_CHECK(internal_storage_get_mqtt_password(mqtt_password));
    ESP_ERROR_CHECK(internal_storage_get_mqtt_client_id(mqtt_client_id));

    snprintf(mqtt_broker_full_uri, sizeof(mqtt_broker_full_uri), "mqtt://%s", mqtt_broker);

    ESP_LOGD(TAG, "MQTT broker: %s:%d", mqtt_broker, mqtt_port);
    ESP_LOGD(TAG, "MQTT username: %s", mqtt_username);
    ESP_LOGD(TAG, "MQTT client ID: %s", mqtt_client_id);

    const esp_mqtt_client_config_t mqtt_config = {
        .broker = {
            .address.uri = mqtt_broker_full_uri,
            .address.port = mqtt_port
        },
        .credentials = {
            .username = mqtt_username,
            .client_id = mqtt_client_id,
            .authentication.password = mqtt_password,
        },
    };

    client = esp_mqtt_client_init(&mqtt_config);
    if(client == NULL){
        ESP_LOGE(TAG, "Failed to create MQTT client");
        return ESP_FAIL;
    }

    ESP_ERROR_CHECK(esp_mqtt_client_register_event(client, MQTT_EVENT_ANY, mqtt_connection_event_handler, NULL));
    ESP_ERROR_CHECK(esp_mqtt_client_start(client));

    ESP_LOGI(TAG, "MQTT client started successfully, connecting...");

    xEventGroupWaitBits(mqtt_connection_event_group, MQTT_CONNECTION_CONNECTED_EVENT_BIT, pdFALSE, pdFALSE, portMAX_DELAY);

    ESP_LOGI(TAG, "MQTT client connected successfully");

    return ESP_OK;
}

static void mqtt_connection_publish_version(void){
    char base_topic[MQTT_CONNECTION_TOPIC_MAX_LEN];
    if (mqtt_connection_get_base_topic(base_topic, sizeof(base_topic)) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get base topic for version publish");
        return;
    }
    snprintf(version_topic, sizeof(version_topic), "%s/homepost_version", base_topic);
    snprintf(version_payload, sizeof(version_payload), "{\"version\":\"%s\"}", CONFIG_APP_PROJECT_VER);
    ESP_LOGI(TAG, "Publishing firmware version: %s", CONFIG_APP_PROJECT_VER);
    mqtt_connection_put_publish_queue(&version_message);
}

static void mqtt_connection_publish_loop(void){
    struct mqtt_connection_message_t msg = {0};
    int ret;

    ESP_LOGI(TAG, "MQTT publish loop started");

    while(mqtt_connection_task_running){
        xEventGroupWaitBits(mqtt_connection_event_group, MQTT_CONNECTION_CONNECTED_EVENT_BIT, pdFALSE, pdFALSE, portMAX_DELAY);

        if(xQueueReceive(mqtt_connection_message_queue, &msg, portMAX_DELAY) == pdTRUE){
            ESP_LOGI(TAG, "Publishing message to topic: %s", msg.topic);
            ret = esp_mqtt_client_publish(client, msg.topic, msg.payload, 0, msg.qos, 0);
            if(ret < 0){
                ESP_LOGE(TAG, "Failed to publish message to topic: %s", msg.topic);
            }
            else if (ret == 0) {
                ESP_LOGI(TAG, "Message with QoS 0 published without confirmation");
            }
            else {
                ESP_LOGI(TAG, "Message %d routed to publishing successfully", ret);
                xEventGroupWaitBits(mqtt_connection_event_group, MQTT_CONNECTION_PUBLISH_EVENT_BIT, pdFALSE, pdFALSE, portMAX_DELAY);
                ESP_LOGI(TAG, "Message published successfully to topic: %s", msg.topic);
                xEventGroupClearBits(mqtt_connection_event_group, MQTT_CONNECTION_PUBLISH_EVENT_BIT);
            }
        } else {
            ESP_LOGE(TAG, "Failed to receive message from queue");
        }
    }
}

static void mqtt_connection_stop(void){
    mqtt_connection_task_running = false;
    if (client != NULL){
        esp_mqtt_client_disconnect(client);
        esp_mqtt_client_stop(client);
        esp_mqtt_client_destroy(client);
        mqtt_connection_task_handle = NULL;
    }
}

static void mqtt_connection_task(void *arg){
    esp_err_t ret;

    if (mqtt_connection_event_group == NULL) {
        mqtt_connection_event_group = xEventGroupCreate();
        if (mqtt_connection_event_group == NULL) {
            ESP_LOGE(TAG, "Failed to create MQTT connection event group");
            vTaskDelete(NULL);
        }
    }

    if (mqtt_connection_message_queue == NULL) {
        mqtt_connection_message_queue = xQueueCreate(CONFIG_HOMEPOST_MQTT_PUBLISH_QUEUE_SIZE, sizeof(struct mqtt_connection_message_t));
        if (mqtt_connection_message_queue == NULL) {
            ESP_LOGE(TAG, "Failed to create MQTT connection message queue");
            vTaskDelete(NULL);
        }
    }

    ret = mqtt_connection_start();
    if(ret != ESP_OK){
        ESP_LOGE(TAG, "MQTT connection failed: %s", esp_err_to_name(ret));
        vTaskDelete(NULL);
    }

    mqtt_connection_task_running = true;

    // Publish firmware version on successful connection
    mqtt_connection_publish_version();

    mqtt_connection_publish_loop();

    ESP_LOGI(TAG, "MQTT publish loop exited");
    mqtt_connection_stop_task();
}

void mqtt_connection_stop_task(void){

    mqtt_connection_stop();

    if(mqtt_connection_message_queue != NULL){
        vQueueDelete(mqtt_connection_message_queue);
        mqtt_connection_message_queue = NULL;
    }
    if(mqtt_connection_event_group != NULL){
        vEventGroupDelete(mqtt_connection_event_group);
        mqtt_connection_event_group = NULL;
    }
    if(mqtt_connection_task_handle != NULL){
        vTaskDelete(mqtt_connection_task_handle);
        mqtt_connection_task_handle = NULL;
    }
}

void mqtt_connection_start_task(void){
    if (mqtt_connection_task_handle != NULL) {
        ESP_LOGW(TAG, "MQTT connection task already running, stopping it first");
        mqtt_connection_stop_task();
    }
    xTaskCreate(mqtt_connection_task, MQTT_CONNECTION_TASK_NAME, MQTT_CONNECTION_STACK_SIZE, NULL, MQTT_CONNECTION_TASK_PRIORITY, &mqtt_connection_task_handle);
}

esp_err_t mqtt_connection_put_publish_queue(struct mqtt_connection_message_t *msg){
    BaseType_t ret = pdFALSE;
    if(mqtt_connection_message_queue != NULL){
        ret = xQueueSend(mqtt_connection_message_queue, msg, 0);
    } else {
        ESP_LOGE(TAG, "MQTT connection message queue is NULL");
    }

    return ret == pdTRUE ? ESP_OK : ESP_FAIL;
}

esp_err_t mqtt_connection_get_base_topic(char *topic_out, size_t topic_out_size){
    if (topic_out == NULL || topic_out_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    if (internal_storage_check_mqtt_topic_preserved()) {
        return internal_storage_get_mqtt_topic(topic_out);
    }

    // Fallback to compile-time default
    size_t default_len = strlen(CONFIG_HOMEPOST_MQTT_TOPIC);
    if (default_len >= topic_out_size) {
        return ESP_ERR_INVALID_SIZE;
    }
    strncpy(topic_out, CONFIG_HOMEPOST_MQTT_TOPIC, topic_out_size);
    topic_out[topic_out_size - 1] = '\0';
    return ESP_OK;
}