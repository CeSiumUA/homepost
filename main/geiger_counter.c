#include "geiger_counter.h"
#include <driver/gpio.h>
#include <esp_attr.h>

#define GPIO_CPM_PIN_SEL        GPIO_NUM_4
#define GPIO_CPM_INPUT_PIN      (1 << GPIO_CPM_PIN_SEL)
#define GPIO_INTR_FLAG_DEFAULT  (0)

static gpio_config_t io_config = {
    .intr_type = GPIO_INTR_POSEDGE,
    .mode = GPIO_MODE_INPUT,
    .pin_bit_mask = GPIO_CPM_INPUT_PIN,
    .pull_up_en = GPIO_PULLUP_ENABLE
};

static SemaphoreHandle_t gpio_semaphore;

static void IRAM_ATTR geiger_counter_gpio_isr_handler(void *arg) {
    
}

void geiger_counter_start(void){
    gpio_semaphore = xSemaphoreCreateMutex();
    ESP_ERROR_CHECK(gpio_config(&io_config));
    ESP_ERROR_CHECK(gpio_install_isr_service(GPIO_INTR_FLAG_DEFAULT));
    ESP_ERROR_CHECK(gpio_isr_handler_add(GPIO_CPM_PIN_SEL, geiger_counter_gpio_isr_handler, NULL));
}