#include "ui_manager.h"
#include "ui_setup.h"
#include "ui_view_menu.h"
#include "ui_entities.h"
#include "ui_status.h"
#include "../platform/storage.h"
#include "../app/app_state.h"
#include "lvgl.h"

/* Slide-animatie: 200 ms conform ontwerp */
#define ANIM_MS     200
#define ANIM_FWD    LV_SCR_LOAD_ANIM_MOVE_LEFT
#define ANIM_BACK   LV_SCR_LOAD_ANIM_MOVE_RIGHT

static lv_obj_t *s_screen_setup      = NULL;
static lv_obj_t *s_screen_view_menu  = NULL;
static lv_obj_t *s_screen_entities   = NULL;
static lv_obj_t *s_screen_status     = NULL;

/* lv_scr_load_anim() op het al-actieve scherm laat beide slide-animaties op
   hetzelfde object los; de uit-animatie wint en het scherm eindigt off-screen
   (leeg scherm). Daarom: nooit een scherm laden dat al actief is. */
static void load_screen(lv_obj_t *scr, lv_scr_load_anim_t anim) {
    if (scr == lv_scr_act()) return;
    lv_scr_load_anim(scr, anim, ANIM_MS, 0, false);
}

void ui_manager_init(void) {
    lv_obj_t *theme_obj = lv_scr_act();

    /* Donker kleurthema */
    lv_theme_t *theme = lv_theme_default_init(
        lv_disp_get_default(),
        lv_palette_main(LV_PALETTE_LIGHT_BLUE),
        lv_palette_main(LV_PALETTE_CYAN),
        true,   /* dark mode */
        LV_FONT_DEFAULT
    );
    lv_disp_set_theme(lv_disp_get_default(), theme);

    uint8_t configured = storage_get_u8("configured", 0);
    if (configured) {
        /* Geconfigureerd: toon view-menu totdat HA verbinding meldt */
        app_state_set(STATE_WIFI_CONNECTING);
        ui_manager_show_view_menu();
    } else {
        ui_manager_show_setup();
    }
}

bool ui_manager_setup_active(void) {
    return s_screen_setup && s_screen_setup == lv_scr_act();
}

void ui_manager_show_setup(void) {
    /* Altijd vers aanmaken: WiFi-scan start opnieuw, state wordt gereset */
    if (s_screen_setup) {
        if (s_screen_setup == lv_scr_act()) return;  /* al actief: niet verwijderen */
        lv_obj_del(s_screen_setup);
    }
    s_screen_setup = ui_setup_create();
    load_screen(s_screen_setup, ANIM_FWD);
}

void ui_manager_show_view_menu(void) {
    if (!s_screen_view_menu) s_screen_view_menu = ui_view_menu_create();
    load_screen(s_screen_view_menu, ANIM_FWD);
}

void ui_manager_show_entities(void) {
    if (s_screen_entities) {
        if (s_screen_entities == lv_scr_act()) {
            /* Actief scherm niet verwijderen; vers opbouwen zonder animatie */
            lv_obj_t *old = s_screen_entities;
            s_screen_entities = ui_entities_create();
            lv_scr_load_anim(s_screen_entities, LV_SCR_LOAD_ANIM_NONE, 0, 0, false);
            lv_obj_del(old);
            return;
        }
        lv_obj_del(s_screen_entities);
    }
    s_screen_entities = ui_entities_create();
    load_screen(s_screen_entities, ANIM_FWD);
}

void ui_manager_show_status(void) {
    if (!s_screen_status) s_screen_status = ui_status_create();
    ui_status_refresh();   /* direct actuele waarden + evt. foutmelding */
    load_screen(s_screen_status, ANIM_FWD);
}

void ui_manager_show_disconnected(void) {
    /* Geen HA-verbinding: toon de statuspagina met foutmelding en
       herstel-indicator (US-009). */
    ui_manager_show_status();
}

void ui_manager_refresh_view_list(void) {
    ui_view_menu_refresh();
}

void ui_manager_update_entity(const ha_event_t *evt) {
    if (s_screen_entities) {
        ui_entities_update(evt);
    }
}
