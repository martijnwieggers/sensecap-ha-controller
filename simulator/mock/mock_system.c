#include "mock_system.h"
#include <SDL2/SDL.h>
#include <stdarg.h>
#include <stdio.h>

static uint32_t s_start_ticks = 0;

void mock_system_init(void) {
    SDL_Init(SDL_INIT_TIMER | SDL_INIT_VIDEO);
    s_start_ticks = SDL_GetTicks();
}

uint32_t mock_tick_get(void) {
    return SDL_GetTicks() - s_start_ticks;
}
