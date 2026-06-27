#include "ui_entities.h"
#include "ui_widgets.h"
#include "../app/app_entities.h"
#include "lvgl.h"
#include <string.h>

#define TITLE_H   40
#define DOTS_H    20
#define ROW_H     ((LV_VER_RES - TITLE_H - DOTS_H) / ENTITIES_PER_PAGE)  /* 70 px */

static view_model_t s_vm;
static entity_t     s_entities[MAX_ENTITIES];
static lv_obj_t    *s_tileview  = NULL;
static lv_obj_t    *s_dots      = NULL;

/* Paginering-dots bijwerken */
static void on_tile_changed(lv_event_t *e) {
    lv_obj_t *tv = lv_event_get_target(e);
    int page = lv_tileview_get_tile_act(tv)->user_data
               ? (int)(intptr_t)lv_tileview_get_tile_act(tv)->user_data : 0;
    /* TODO: dots bijwerken op basis van actieve pagina */
    (void)page;
}

static lv_obj_t *build_dots(lv_obj_t *parent, int page_count) {
    lv_obj_t *cont = lv_obj_create(parent);
    lv_obj_set_size(cont, LV_HOR_RES, DOTS_H);
    lv_obj_align(cont, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(cont, lv_color_hex(0x16213E), 0);
    lv_obj_set_style_border_width(cont, 0, 0);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    for (int i = 0; i < page_count; i++) {
        lv_obj_t *dot = lv_obj_create(cont);
        lv_obj_set_size(dot, 8, 8);
        lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(dot,
            i == 0 ? lv_color_hex(0x4FC3F7) : lv_color_hex(0x424242), 0);
        lv_obj_set_style_border_width(dot, 0, 0);
    }
    return cont;
}

lv_obj_t *ui_entities_create(void) {
    /* TODO: s_vm vullen vanuit ha_lovelace resultaat.
       Tijdelijk: fake entiteiten voor layout-test. */
    s_vm.page_count     = 1;
    s_vm.total_entities = 0;

    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x1A1A2E), 0);

    /* Titelbar */
    lv_obj_t *title = lv_label_create(screen);
    lv_label_set_text(title, s_vm.view_title[0] ? s_vm.view_title : "Home");
    lv_obj_set_style_text_color(title, lv_color_hex(0xE0E0E0), 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 8, 10);

    /* Menu-knop (☰) */
    lv_obj_t *btn_menu = lv_btn_create(screen);
    lv_obj_set_size(btn_menu, 40, TITLE_H);
    lv_obj_align(btn_menu, LV_ALIGN_TOP_RIGHT, -40, 0);
    lv_obj_t *lbl_menu = lv_label_create(btn_menu);
    lv_label_set_text(lbl_menu, LV_SYMBOL_LIST);

    /* Status-knop (ℹ) */
    lv_obj_t *btn_info = lv_btn_create(screen);
    lv_obj_set_size(btn_info, 40, TITLE_H);
    lv_obj_align(btn_info, LV_ALIGN_TOP_RIGHT, 0, 0);
    lv_obj_t *lbl_info = lv_label_create(btn_info);
    lv_label_set_text(lbl_info, LV_SYMBOL_SETTINGS);

    /* Tileview voor swipe-paginering */
    s_tileview = lv_tileview_create(screen);
    lv_obj_set_pos(s_tileview, 0, TITLE_H);
    lv_obj_set_size(s_tileview, LV_HOR_RES, LV_VER_RES - TITLE_H - DOTS_H);
    lv_obj_set_style_bg_color(s_tileview, lv_color_hex(0x1A1A2E), 0);
    lv_obj_add_event_cb(s_tileview, on_tile_changed, LV_EVENT_VALUE_CHANGED, NULL);

    for (int p = 0; p < s_vm.page_count; p++) {
        lv_obj_t *tile = lv_tileview_add_tile(s_tileview, p, 0, LV_DIR_HOR);
        lv_obj_set_style_bg_color(tile, lv_color_hex(0x1A1A2E), 0);
        for (int s = 0; s < s_vm.pages[p].count; s++) {
            entity_t *e = s_vm.pages[p].entities[s];
            lv_obj_t *row = lv_obj_create(tile);
            lv_obj_set_pos(row, 0, s * ROW_H);
            lv_obj_set_size(row, LV_HOR_RES, ROW_H);
            lv_obj_set_style_bg_color(row, lv_color_hex(0x1A1A2E), 0);
            lv_obj_set_style_border_color(row, lv_color_hex(0x2A2A4A), 0);
            lv_obj_set_style_border_width(row, 0, 0);
            lv_obj_set_style_border_side(row, LV_BORDER_SIDE_BOTTOM, 0);
            ui_widgets_render_row(row, e);
        }
    }

    s_dots = build_dots(screen, s_vm.page_count);
    return screen;
}

void ui_entities_update(const char *entity_id, const char *state,
                         float brightness_pct, float temperature) {
    entity_t *e = entities_find(&s_vm, entity_id);
    if (!e) return;

    strncpy(e->state, state, sizeof(e->state) - 1);
    if (brightness_pct >= 0) e->brightness_pct = brightness_pct;
    if (temperature >= 0)    e->temperature    = temperature;

    /* TODO: gerichte widget refresh zonder volledige herrender */
}
