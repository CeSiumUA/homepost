#ifndef INTERNAL_STORAGE_H
#define INTERNAL_STORAGE_H

#include <stdio.h>
#include <stdbool.h>
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_err.h"

void internal_storage_init(void);

#endif