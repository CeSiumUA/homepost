#ifndef MQTT_CONNECTION_H
#define MQTT_CONNECTION_H

#include <stdio.h>
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
    char topic[100];
    char payload[100];
    uint8_t qos;
};

#endif