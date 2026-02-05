#include "geiger_counter.h"
#include <driver/gpio.h>
#include <esp_attr.h>
#include <esp_timer.h>

#define GPIO_CPM_PIN_SEL                                GPIO_NUM_4
#define GPIO_CPM_INPUT_PIN                              (1ULL<<GPIO_CPM_PIN_SEL)
#define GPIO_INTR_FLAG_DEFAULT                          (0)
#define GEIGER_COUNTER_CONVERSION_FACTOR                ((CONFIG_HOMEPOST_GEIGER_COUNTER_CONVERSION_FACTOR) / 1000000.0f)

static portMUX_TYPE gpio_spinlock = portMUX_INITIALIZER_UNLOCKED;

static void geiger_counter_timer_cb(void *arg);

static char radiation_payload[32];
static char radiation_topic[100];
static struct mqtt_connection_message_t radiation_message = {
    .topic = radiation_topic,
    .payload = radiation_payload,
    .qos = 0
};

static const char *TAG = __FILE__;

static gpio_config_t io_config = {
    .intr_type = GPIO_INTR_NEGEDGE,
    .mode = GPIO_MODE_INPUT,
    .pin_bit_mask = GPIO_CPM_INPUT_PIN,
    .pull_down_en = GPIO_PULLDOWN_DISABLE,
    .pull_up_en = GPIO_PULLUP_DISABLE
};

static esp_timer_handle_t geiger_counter_timer;
static const esp_timer_create_args_t geiger_counter_timer_args = {
    .callback = &geiger_counter_timer_cb,
};

static uint32_t geiger_counts = 0;

static uint32_t cpm_history[CONFIG_HOMEPOST_GEIGER_COUNTER_CPM_HISTORY_DEPTH] = {0};
static uint32_t cpm_index = 0;
static bool cpm_history_full = false;

static void IRAM_ATTR geiger_counter_gpio_isr_handler(void *arg) {
    gpio_num_t gpio_num = (gpio_num_t)arg;
    if (gpio_num != GPIO_CPM_PIN_SEL) {
        return;
    }

    geiger_counts++;
}

static void geiger_counter_timer_cb(void *arg)
{
    uint32_t cpm = 0;
    uint32_t cpm_sum = 0;
    float average_cpm = 0;
    float average_usvh = 0;
    int ret;

    taskENTER_CRITICAL(&gpio_spinlock);
    cpm = geiger_counts;
    geiger_counts = 0;
    taskEXIT_CRITICAL(&gpio_spinlock);

    ESP_LOGI(TAG, "Latest CPM: %lu", cpm);

    cpm_history[cpm_index] = cpm;
    cpm_index = (cpm_index + 1) % CONFIG_HOMEPOST_GEIGER_COUNTER_CPM_HISTORY_DEPTH;
    cpm_history_full = cpm_history_full || (cpm_index == 0);

    for (int i = 0; i < (cpm_history_full ? CONFIG_HOMEPOST_GEIGER_COUNTER_CPM_HISTORY_DEPTH : cpm_index); i++) {
        cpm_sum += cpm_history[i];
    }

    average_cpm = (float)cpm_sum / (float)(cpm_history_full ? CONFIG_HOMEPOST_GEIGER_COUNTER_CPM_HISTORY_DEPTH : cpm_index);
    average_usvh = average_cpm * GEIGER_COUNTER_CONVERSION_FACTOR;

    ESP_LOGI(TAG, "Average CPM: %f, Average uSv/h: %f", average_cpm, average_usvh);

    memset(radiation_payload, 0, sizeof(radiation_payload));
    ret = snprintf(radiation_payload, sizeof(radiation_payload), "{\"radiation\": %.3f}", average_usvh);
    if (ret < 0 || ret >= sizeof(radiation_payload)) {
        ESP_LOGE(TAG, "Failed to create radiation payload");
        return;
    }

    if(mqtt_connection_put_publish_queue(&radiation_message) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enqueue radiation message");
    }
}

void geiger_counter_start(void){
    // Build MQTT topic from base topic
    char base_topic[64];
    if (mqtt_connection_get_base_topic(base_topic, sizeof(base_topic)) == ESP_OK) {
        snprintf(radiation_topic, sizeof(radiation_topic), "%s/radiation", base_topic);
    } else {
        ESP_LOGE(TAG, "Failed to get base topic, using default");
        snprintf(radiation_topic, sizeof(radiation_topic), "%s/radiation", CONFIG_HOMEPOST_MQTT_TOPIC);
    }

    ESP_ERROR_CHECK(gpio_config(&io_config));
    ESP_ERROR_CHECK(gpio_install_isr_service(GPIO_INTR_FLAG_DEFAULT));
    ESP_ERROR_CHECK(gpio_isr_handler_add(GPIO_CPM_PIN_SEL, geiger_counter_gpio_isr_handler, (void *)GPIO_CPM_PIN_SEL));
    ESP_ERROR_CHECK(esp_timer_create(&geiger_counter_timer_args, &geiger_counter_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(geiger_counter_timer, CONFIG_HOMEPOST_GEIGER_COUNTER_TIMER_PERIOD_MS * 1000));
}