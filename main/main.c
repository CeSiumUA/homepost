#include <stdio.h>
#include "internal_storage.h"


void app_main(void)
{
    internal_storage_init();

    printf("Hello world!\n");
}