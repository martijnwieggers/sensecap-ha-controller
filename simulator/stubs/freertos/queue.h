#pragma once
#include "FreeRTOS.h"
#include <stdlib.h>

static inline QueueHandle_t xQueueCreate(int length, int item_size) {
    (void)length; (void)item_size;
    return (QueueHandle_t)malloc(1);
}
static inline int xQueueSend(QueueHandle_t q, const void *item, TickType_t wait) {
    (void)q; (void)item; (void)wait;
    return pdTRUE;
}
static inline int xQueueReceive(QueueHandle_t q, void *item, TickType_t wait) {
    (void)q; (void)item; (void)wait;
    return pdFALSE;
}
