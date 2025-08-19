#ifndef MQTT_CONNECTION_H
#define MQTT_CONNECTION_H

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <esp_event.h>
#include <esp_err.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <freertos/queue.h>
#include <mqtt_client.h>

#include "internal_storage.h"

struct mqtt_connection_message_t {
    char *topic;
    char *payload;
    uint8_t qos;
};

void mqtt_connection_stop_task(void);
void mqtt_connection_start_task(void);
esp_err_t mqtt_connection_put_publish_queue(struct mqtt_connection_message_t *msg);

#endif