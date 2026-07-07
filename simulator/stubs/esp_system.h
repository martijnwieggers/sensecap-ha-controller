#pragma once
#include <stdlib.h>
#include <stdio.h>

static inline void esp_restart(void) {
    printf("[system] esp_restart() — simulator stopt\n");
    exit(0);
}
