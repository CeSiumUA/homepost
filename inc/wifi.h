#ifndef WIFI_H
#define WIFI_H

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "internal_storage.h"

void wifi_init(void);
void wifi_start_softap(void);
bool wifi_connect_sta(bool is_softap_mode);
bool wifi_is_connected_to_internet(void);

#endif // WIFI_H