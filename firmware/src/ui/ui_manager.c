#include "ui_manager.h"
#include "ui_setup.h"
#include "ui_view_menu.h"
#include "ui_entities.h"
#include "ui_status.h"
#include "../platform/storage.h"
#include "lvgl.h"

/* Slide-animatie: 200 ms conform ontwerp */
#define ANIM_MS     200
#define ANIM_FWD    LV_SCR_LOAD_ANIM_MOVE_LEFT
#define ANIM_BACK   LV_SCR_LOAD_ANIM_MOVE_RIGHT

static lv_obj_t *s_screen_setup      = NULL;
static lv_obj_t *s_screen_view_menu  = NULL;
static lv_obj_t *s_screen_entities   = NULL;
static lv_obj_t *s_screen_status     = NULL;

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
        ui_manager_show_view_menu();
    } else {
        ui_manager_show_setup();
    }
}

void ui_manager_show_setup(void) {
    if (!s_screen_setup) s_screen_setup = ui_setup_create();
    lv_scr_load_anim(s_screen_setup, ANIM_FWD, ANIM_MS, 0, false);
}

void ui_manager_show_view_menu(void) {
    if (!s_screen_view_menu) s_screen_view_menu = ui_view_menu_create();
    lv_scr_load_anim(s_screen_view_menu, ANIM_FWD, ANIM_MS, 0, false);
}

void ui_manager_show_entities(void) {
    if (s_screen_entities) lv_obj_del(s_screen_entities);
    s_screen_entities = ui_entities_create();
    lv_scr_load_anim(s_screen_entities, ANIM_FWD, ANIM_MS, 0, false);
}

void ui_manager_show_status(void) {
    if (!s_screen_status) s_screen_status = ui_status_create();
    lv_scr_load_anim(s_screen_status, ANIM_FWD, ANIM_MS, 0, false);
}

void ui_manager_show_disconnected(void) {
    /* TODO: toon een overlay over het huidige scherm */
}

void ui_manager_refresh_view_list(void) {
    ui_view_menu_refresh();
}

void ui_manager_update_entity(const char *entity_id, const char *state,
                               float brightness_pct, float temperature) {
    if (s_screen_entities) {
        ui_entities_update(entity_id, state, brightness_pct, temperature);
    }
}
