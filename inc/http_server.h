#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include <string.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_system.h>
#include <esp_err.h>
#include <sys/param.h>
#include <esp_netif.h>
#include <esp_http_server.h>
#include <esp_timer.h>
#include <esp_tls.h>
#include <esp_wifi_types_generic.h>
#include "sdkconfig.h"
#include "wifi.h"
#include "internal_storage.h"
#include "mqtt_connection.h"

void http_server_init(void);
void http_server_start(void);

#endif // HTTP_SERVER_H