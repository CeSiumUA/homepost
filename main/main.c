#include <stdio.h>
#include "internal_storage.h"
#include "ble_scanner.h"

void app_main(void)
{
    internal_storage_init();

    ble_scanner_init();
    ble_scanner_start();
}