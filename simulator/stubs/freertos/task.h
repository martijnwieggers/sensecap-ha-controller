#pragma once
#include "FreeRTOS.h"
#include <SDL2/SDL.h>

static inline void vTaskDelay(TickType_t ticks) {
    SDL_Delay((Uint32)ticks);
}
