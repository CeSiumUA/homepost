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

#endif