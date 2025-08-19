#ifndef INTERNAL_STORAGE_H
#define INTERNAL_STORAGE_H

#include <stdio.h>
#include <stdbool.h>
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_err.h"

void internal_storage_init(void);

bool internal_storage_check_wifi_credentials_preserved(void);
esp_err_t internal_storage_save_wifi_credentials(const char *ssid, const char *password);
esp_err_t internal_storage_get_wifi_credentials(char *ssid, char *password);
esp_err_t internal_storage_erase_wifi_credentials(void);

esp_err_t internal_storage_save_mqtt_client_id(const char *client_id);
esp_err_t internal_storage_get_mqtt_client_id(char *client_id);
bool internal_storage_check_mqtt_client_id_preserved(void);

esp_err_t internal_storage_save_mqtt_broker(const char *broker);
esp_err_t internal_storage_get_mqtt_broker(char *broker);
bool internal_storage_check_mqtt_broker_preserved(void);

esp_err_t internal_storage_save_mqtt_port(uint16_t port);
esp_err_t internal_storage_get_mqtt_port(uint16_t *port);
bool internal_storage_check_mqtt_port_preserved(void);

esp_err_t internal_storage_save_mqtt_username(const char *username);
esp_err_t internal_storage_get_mqtt_username(char *username);
bool internal_storage_check_mqtt_username_preserved(void);

esp_err_t internal_storage_save_mqtt_password(const char *password);
esp_err_t internal_storage_get_mqtt_password(char *password);
bool internal_storage_check_mqtt_password_preserved(void);

esp_err_t internal_storage_save_mqtt_topic(const char *topic);
esp_err_t internal_storage_get_mqtt_topic(char *topic);
bool internal_storage_check_mqtt_topic_preserved(void);

#endif