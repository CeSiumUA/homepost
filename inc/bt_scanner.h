#ifndef BT_SCANNER_H
#define BT_SCANNER_H

#include "esp_system.h"
#include "esp_log.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_gap_bt_api.h"
#include "esp_err.h"

esp_err_t bt_scanner_init(void);
esp_err_t bt_scanner_start(void);

#endif