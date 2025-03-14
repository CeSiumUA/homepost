#include <stdio.h>
#include "internal_storage.h"
#include "tracker_scanner.h"

void app_main(void)
{
    internal_storage_init();

    tracker_scanner_start_task();
}