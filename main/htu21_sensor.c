#include "htu21_sensor.h"
#include "mqtt_connection.h"
#include <driver/i2c_master.h>
#include <esp_timer.h>
#include <esp_log.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define HTU21_I2C_ADDR                  0x40
#define HTU21_CMD_TEMP_HOLD             0xE3
#define HTU21_CMD_HUMIDITY_HOLD         0xE5
#define HTU21_CMD_TEMP_NOHOLD           0xF3
#define HTU21_CMD_HUMIDITY_NOHOLD       0xF5
#define HTU21_CMD_SOFT_RESET            0xFE

static const char *TAG = __FILE__;

static i2c_master_bus_handle_t i2c_bus_handle = NULL;
static i2c_master_dev_handle_t htu21_dev_handle = NULL;

static esp_timer_handle_t htu21_timer;

static char temperature_payload[32];
static char humidity_payload[32];

static struct mqtt_connection_message_t temperature_message = {
    .topic = CONFIG_HOMEPOST_MQTT_TOPIC "/temperature",
    .payload = temperature_payload,
    .qos = 0
};

static struct mqtt_connection_message_t humidity_message = {
    .topic = CONFIG_HOMEPOST_MQTT_TOPIC "/humidity",
    .payload = humidity_payload,
    .qos = 0
};

static esp_err_t htu21_read_temperature(float *temperature)
{
    uint8_t cmd = HTU21_CMD_TEMP_NOHOLD;
    uint8_t data[2];
    esp_err_t ret;

    ret = i2c_master_transmit(htu21_dev_handle, &cmd, 1, 1000);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send temperature command: %s", esp_err_to_name(ret));
        return ret;
    }

    vTaskDelay(pdMS_TO_TICKS(85));

    ret = i2c_master_receive(htu21_dev_handle, data, sizeof(data), 1000);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to receive temperature data: %s", esp_err_to_name(ret));
        return ret;
    }

    uint16_t raw_temp = (data[0] << 8) | data[1];
    *temperature = -46.85 + 175.72 * (raw_temp / 65536.0);

    return ESP_OK;
}

static esp_err_t htu21_read_humidity(float *humidity)
{
    uint8_t cmd = HTU21_CMD_HUMIDITY_NOHOLD;
    uint8_t data[2];
    esp_err_t ret;

    ret = i2c_master_transmit(htu21_dev_handle, &cmd, 1, 1000);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send humidity command: %s", esp_err_to_name(ret));
        return ret;
    }

    vTaskDelay(pdMS_TO_TICKS(50));

    ret = i2c_master_receive(htu21_dev_handle, data, sizeof(data), 1000);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to receive humidity data: %s", esp_err_to_name(ret));
        return ret;
    }

    uint16_t raw_humidity = (data[0] << 8) | data[1];
    *humidity = -6.0 + 125.0 * (raw_humidity / 65536.0);

    return ESP_OK;
}

static void htu21_timer_cb(void *arg)
{
    float temperature, humidity;
    esp_err_t ret;

    ret = htu21_read_temperature(&temperature);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Temperature: %.2f C", temperature);
        memset(temperature_payload, 0, sizeof(temperature_payload));
        int len = snprintf(temperature_payload, sizeof(temperature_payload), 
                          "{\"temperature\": %.2f}", temperature);
        if (len > 0 && len < sizeof(temperature_payload)) {
            if (mqtt_connection_put_publish_queue(&temperature_message) != ESP_OK) {
                ESP_LOGE(TAG, "Failed to enqueue temperature message");
            }
        } else {
            ESP_LOGE(TAG, "Failed to format temperature payload");
        }
    } else {
        ESP_LOGE(TAG, "Failed to read temperature, skipping publish");
    }

    ret = htu21_read_humidity(&humidity);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Humidity: %.2f %%", humidity);
        memset(humidity_payload, 0, sizeof(humidity_payload));
        int len = snprintf(humidity_payload, sizeof(humidity_payload), 
                          "{\"humidity\": %.2f}", humidity);
        if (len > 0 && len < sizeof(humidity_payload)) {
            if (mqtt_connection_put_publish_queue(&humidity_message) != ESP_OK) {
                ESP_LOGE(TAG, "Failed to enqueue humidity message");
            }
        } else {
            ESP_LOGE(TAG, "Failed to format humidity payload");
        }
    } else {
        ESP_LOGE(TAG, "Failed to read humidity, skipping publish");
    }
}

static const esp_timer_create_args_t htu21_timer_args = {
    .callback = &htu21_timer_cb,
};

static esp_err_t htu21_init(void)
{
    uint8_t cmd = HTU21_CMD_SOFT_RESET;
    esp_err_t ret;

    ret = i2c_master_transmit(htu21_dev_handle, &cmd, 1, 1000);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to reset HTU21: %s", esp_err_to_name(ret));
        return ret;
    }

    vTaskDelay(pdMS_TO_TICKS(20));
    ESP_LOGI(TAG, "HTU21 sensor initialized successfully");

    return ESP_OK;
}

void htu21_sensor_start(void)
{
    esp_err_t ret;

    ESP_LOGI(TAG, "Starting HTU21 sensor on I2C bus (SDA: %d, SCL: %d, freq: %d Hz)",
             CONFIG_HOMEPOST_HTU21_I2C_SDA_GPIO, CONFIG_HOMEPOST_HTU21_I2C_SCL_GPIO,
             CONFIG_HOMEPOST_HTU21_I2C_FREQ_HZ);

    i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = CONFIG_HOMEPOST_HTU21_I2C_SDA_GPIO,
        .scl_io_num = CONFIG_HOMEPOST_HTU21_I2C_SCL_GPIO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    ret = i2c_new_master_bus(&bus_config, &i2c_bus_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create I2C bus: %s", esp_err_to_name(ret));
        return;
    }

    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = HTU21_I2C_ADDR,
        .scl_speed_hz = CONFIG_HOMEPOST_HTU21_I2C_FREQ_HZ,
    };

    ret = i2c_master_bus_add_device(i2c_bus_handle, &dev_config, &htu21_dev_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add HTU21 device: %s", esp_err_to_name(ret));
        i2c_del_master_bus(i2c_bus_handle);
        return;
    }
    ESP_LOGI(TAG, "HTU21 device added to I2C bus at address 0x%02X", HTU21_I2C_ADDR);

    ret = htu21_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "HTU21 initialization failed, sensor readings may be unreliable");
    }

    ret = esp_timer_create(&htu21_timer_args, &htu21_timer);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create timer: %s", esp_err_to_name(ret));
        i2c_master_bus_rm_device(htu21_dev_handle);
        i2c_del_master_bus(i2c_bus_handle);
        return;
    }

    ret = esp_timer_start_periodic(htu21_timer, CONFIG_HOMEPOST_HTU21_TIMER_PERIOD_MS * 1000);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start timer: %s", esp_err_to_name(ret));
        esp_timer_delete(htu21_timer);
        i2c_master_bus_rm_device(htu21_dev_handle);
        i2c_del_master_bus(i2c_bus_handle);
        return;
    }

    ESP_LOGI(TAG, "HTU21 sensor started successfully (polling interval: %d ms)", CONFIG_HOMEPOST_HTU21_TIMER_PERIOD_MS);
}
