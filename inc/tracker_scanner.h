#ifndef TRACKER_SCANNER_H
#define TRACKER_SCANNER_H

#include <stdio.h>
#include <stdbool.h>
#include "esp_log.h"
#include "esp_err.h"
#include "ble_scanner.h"
#include "ble_ibeacon.h"
#include "mqtt_connection.h"

void tracker_scanner_start_task(void);
void tracker_scanner_stop_task(void);

#endif // TRACKER_SCANNER_H