#include "lvgl.h"
#include "sdl/sdl.h"

#include "mock/mock_storage.h"
#include "mock/mock_system.h"
#include "mock/mock_ha.h"

#include "app/app_state.h"
#include "app/app_entities.h"
#include "app/app_events.h"
#include "ui/ui_manager.h"

#include <stdio.h>
#include <SDL2/SDL.h>

/* Eenvoudige queue-simulatie voor de pc (geen FreeRTOS) */
#define EVT_QUEUE_SIZE 32
static ha_event_t s_evt_queue[EVT_QUEUE_SIZE];
static int        s_evt_head = 0, s_evt_tail = 0;

void sim_queue_send(const ha_event_t *evt) {
    int next = (s_evt_tail + 1) % EVT_QUEUE_SIZE;
    if (next != s_evt_head) {
        s_evt_queue[s_evt_tail] = *evt;
        s_evt_tail = next;
    }
}

static int sim_queue_recv(ha_event_t *evt) {
    if (s_evt_head == s_evt_tail) return 0;
    *evt = s_evt_queue[s_evt_head];
    s_evt_head = (s_evt_head + 1) % EVT_QUEUE_SIZE;
    return 1;
}

/* Stubs die in de firmware FreeRTOS-queues gebruiken — hier gesimuleerd */
void xQueueSend_stub(ha_event_t *evt) { sim_queue_send(evt); }

int main(int argc, char *argv[]) {
    printf("SenseCap HA Controller — Simulator\n");
    printf("Schermresolutie: 480x480  |  Thema: Donker\n\n");

    /* Platform mocks initialiseren */
    mock_storage_init("config.ini");
    mock_system_init();

    /* LVGL initialiseren */
    lv_init();

    /* SDL2 display + muis via lv_drivers SDL-driver */
    sdl_init();

    static lv_disp_draw_buf_t draw_buf;
    static lv_color_t buf1[480 * 20];
    static lv_color_t buf2[480 * 20];
    lv_disp_draw_buf_init(&draw_buf, buf1, buf2, 480 * 20);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.draw_buf   = &draw_buf;
    disp_drv.flush_cb   = sdl_display_flush;
    disp_drv.hor_res    = 480;
    disp_drv.ver_res    = 480;
    lv_disp_drv_register(&disp_drv);

    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type    = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = sdl_mouse_read;
    lv_indev_drv_register(&indev_drv);

    /* Applicatielaag opstarten */
    app_state_init();
    ui_manager_init();

    /* Mock HA: start achtergrond event-generator */
    mock_ha_start(sim_queue_send);

    /* Hoofdlus.
       SDL-events (muis, toetsenbord, quit) worden verwerkt door de LVGL-timer
       die sdl_init() registreert (sdl_event_handler, elke 10 ms).
       De hoofdlus mag SDL_PollEvent NIET aanroepen — dat leegt de queue
       voordat sdl_event_handler mouse_handler() kan aanroepen. */
    while (1) {
        /* HA-events doorgeven aan UI */
        ha_event_t evt;
        while (sim_queue_recv(&evt)) {
            app_events_handle(&evt);
        }

        lv_tick_inc(5);
        lv_timer_handler();  /* roept ook sdl_event_handler aan (muis + quit) */
        SDL_Delay(5);
    }
    return 0;
}
