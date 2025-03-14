#ifndef BT_SCANNER_H
#define BT_SCANNER_H

#include <stdio.h>
#include <stdbool.h>
#include "esp_log.h"
#include "esp_err.h"
#include "esp_bt_main.h"
#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "string.h"

typedef void (* ble_scanned_device_cb_t)(esp_ble_gap_cb_param_t *param);

esp_err_t ble_scanner_init(void);
esp_err_t ble_scanner_start(ble_scanned_device_cb_t cb);

#endif