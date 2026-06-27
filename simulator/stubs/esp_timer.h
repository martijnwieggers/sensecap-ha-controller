#pragma once
#include <stdint.h>
#include <SDL2/SDL.h>

/* Simulator: gebruik SDL_GetTicks() (ms) → converteer naar µs */
static inline uint64_t esp_timer_get_time(void) {
    return (uint64_t)SDL_GetTicks() * 1000ULL;
}
