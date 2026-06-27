#pragma once
#include <stdint.h>
#include <stdlib.h>

typedef uint32_t TickType_t;
typedef void *   SemaphoreHandle_t;
typedef void *   QueueHandle_t;
typedef void *   TaskHandle_t;

#define portMAX_DELAY       ((TickType_t)0xFFFFFFFFU)
#define pdTRUE              (1)
#define pdFALSE             (0)
#define pdPASS              pdTRUE
#define pdFAIL              pdFALSE
#define pdMS_TO_TICKS(ms)   ((TickType_t)(ms))
