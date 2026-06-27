#include "lvgl.h"
#include "lv_drivers/display/monitor.h"
#include "lv_drivers/indev/mouse.h"

#include "mock/mock_storage.h"
#include "mock/mock_system.h"
#include "mock/mock_ha.h"

#include "app/app_state.h"
#include "app/app_entities.h"
#include "app/app_events.h"
#include "ui/ui_manager.h"

#include <stdio.h>
#include <SDL2/SDL.h>

/* Gedeeld met firmware/src — zelfde type, gesimuleerde implementatie */
#include "app/app_events.h"

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

    /* SDL2 display driver (480x480 venster) */
    monitor_init();
    lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.flush_cb = monitor_flush;
    disp_drv.hor_res  = 480;
    disp_drv.ver_res  = 480;
    static lv_disp_draw_buf_t draw_buf;
    static lv_color_t buf[480 * 10];
    lv_disp_draw_buf_init(&draw_buf, buf, NULL, 480 * 10);
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    /* Muis als touch-invoer */
    mouse_init();
    lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type    = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = mouse_read;
    lv_indev_drv_register(&indev_drv);

    /* Applicatielaag opstarten */
    app_state_init();
    ui_manager_init();

    /* Mock HA: start achtergrond event-generator */
    mock_ha_start(sim_queue_send);

    /* Hoofdlus */
    while (1) {
        SDL_Event sdl_evt;
        while (SDL_PollEvent(&sdl_evt)) {
            if (sdl_evt.type == SDL_QUIT) {
                printf("Simulator afgesloten.\n");
                return 0;
            }
        }

        /* HA-events doorgeven aan UI */
        ha_event_t evt;
        while (sim_queue_recv(&evt)) {
            app_events_handle(&evt);
        }

        lv_timer_handler();
        SDL_Delay(5);
    }
    return 0;
}
