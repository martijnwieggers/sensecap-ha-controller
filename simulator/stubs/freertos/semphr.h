#pragma once
#include "FreeRTOS.h"
#include <stdlib.h>

/* Simulator is single-threaded: semaphores zijn no-ops */
static inline SemaphoreHandle_t xSemaphoreCreateMutex(void) {
    return (SemaphoreHandle_t)malloc(1);
}
#define xSemaphoreTake(sem, ticks)  (pdTRUE)
#define xSemaphoreGive(sem)         (pdTRUE)
