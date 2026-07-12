#include "ui_entities.h"
#include "ui_widgets.h"
#include "ui_manager.h"
#include "../app/app_entities.h"
#include "../ha/ha_lovelace.h"
#include "lvgl.h"
#include <string.h>

#define CLR_BG    0x1A1A2E
#define CLR_PANEL 0x16213E
#define CLR_TEXT  0xE0E0E0
#define CLR_ACCENT 0x4FC3F7
#define CLR_SEP   0x2A2A4A

#define TITLE_H   40
#define DOTS_H    20
#define ROW_H     ((LV_VER_RES - TITLE_H - DOTS_H) / ENTITIES_PER_PAGE)

static view_model_t *s_vm       = NULL;
static lv_obj_t     *s_tileview = NULL;
static lv_obj_t     *s_dots     = NULL;

/* ---- Dot-indicator bijwerken ---- */

static void update_dots(int active_page) {
    if (!s_dots) return;
    int total = lv_obj_get_child_cnt(s_dots);
    for (int i = 0; i < total; i++) {
        lv_obj_t *dot = lv_obj_get_child(s_dots, i);
        lv_obj_set_style_bg_color(dot,
            i == active_page ? lv_color_hex(CLR_ACCENT)
                             : lv_color_hex(0x424242), 0);
    }
}

static void on_tile_changed(lv_event_t *e) {
    lv_obj_t *tv   = lv_event_get_target(e);
    lv_obj_t *tile = lv_tileview_get_tile_act(tv);
    int page = tile ? (int)(intptr_t)lv_obj_get_user_data(tile) : 0;
    update_dots(page);
}

/* ---- Dot-balk aanmaken ---- */

static lv_obj_t *build_dots(lv_obj_t *parent, int page_count) {
    lv_obj_t *cont = lv_obj_create(parent);
    lv_obj_set_size(cont, LV_HOR_RES, DOTS_H);
    lv_obj_align(cont, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(cont, lv_color_hex(CLR_PANEL), 0);
    lv_obj_set_style_border_width(cont, 0, 0);
    lv_obj_set_style_pad_all(cont, 0, 0);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(cont, 8, 0);  /* ruimte tussen de dots */

    for (int i = 0; i < page_count; i++) {
        lv_obj_t *dot = lv_obj_create(cont);
        lv_obj_set_size(dot, 8, 8);
        lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(dot,
            i == 0 ? lv_color_hex(CLR_ACCENT)
                   : lv_color_hex(0x424242), 0);
        lv_obj_set_style_border_width(dot, 0, 0);
        lv_obj_set_style_pad_all(dot, 0, 0);
    }
    return cont;
}

/* ---- Knop-callbacks (geen C++ lambdas — compatibel met C-compiler) ---- */

static void menu_btn_cb(lv_event_t *e) {
    ui_manager_show_view_menu();
}

static void info_btn_cb(lv_event_t *e) {
    ui_manager_show_status();
}

/* ---- Scherm aanmaken ---- */

lv_obj_t *ui_entities_create(void) {
    s_vm = ha_lovelace_get_view_model();
    ui_widgets_reset_refs();  /* registry hoort bij dit (nieuwe) scherm */

    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, lv_color_hex(CLR_BG), 0);

    /* Titelbalk */
    lv_obj_t *title_bar = lv_obj_create(screen);
    lv_obj_set_size(title_bar, LV_HOR_RES, TITLE_H);
    lv_obj_set_pos(title_bar, 0, 0);
    lv_obj_set_style_bg_color(title_bar, lv_color_hex(CLR_PANEL), 0);
    lv_obj_set_style_border_width(title_bar, 0, 0);
    lv_obj_set_style_pad_all(title_bar, 0, 0);

    lv_obj_t *title_lbl = lv_label_create(title_bar);
    lv_label_set_text(title_lbl,
        (s_vm && s_vm->view_title[0]) ? s_vm->view_title : "Home");
    lv_obj_set_style_text_color(title_lbl, lv_color_hex(CLR_TEXT), 0);
    lv_label_set_long_mode(title_lbl, LV_LABEL_LONG_DOT);
    lv_obj_set_width(title_lbl, LV_HOR_RES - 90);
    lv_obj_align(title_lbl, LV_ALIGN_LEFT_MID, 8, 0);

    /* Terug naar view-menu knop (☰) */
    lv_obj_t *btn_menu = lv_btn_create(title_bar);
    lv_obj_set_size(btn_menu, 44, TITLE_H);
    lv_obj_align(btn_menu, LV_ALIGN_RIGHT_MID, -44, 0);
    lv_obj_set_style_bg_color(btn_menu, lv_color_hex(CLR_SEP), 0);
    lv_obj_t *lbl_menu = lv_label_create(btn_menu);
    lv_label_set_text(lbl_menu, LV_SYMBOL_LIST);
    lv_obj_set_style_text_color(lbl_menu, lv_color_hex(CLR_TEXT), 0);
    lv_obj_center(lbl_menu);
    lv_obj_add_event_cb(btn_menu, menu_btn_cb, LV_EVENT_CLICKED, NULL);

    /* Statuspagina knop (ℹ) */
    lv_obj_t *btn_info = lv_btn_create(title_bar);
    lv_obj_set_size(btn_info, 44, TITLE_H);
    lv_obj_align(btn_info, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_bg_color(btn_info, lv_color_hex(CLR_SEP), 0);
    lv_obj_t *lbl_info = lv_label_create(btn_info);
    lv_label_set_text(lbl_info, LV_SYMBOL_SETTINGS);
    lv_obj_set_style_text_color(lbl_info, lv_color_hex(CLR_TEXT), 0);
    lv_obj_center(lbl_info);
    lv_obj_add_event_cb(btn_info, info_btn_cb, LV_EVENT_CLICKED, NULL);

    /* Tileview voor swipe-paginering */
    s_tileview = lv_tileview_create(screen);
    lv_obj_set_pos(s_tileview, 0, TITLE_H);
    lv_obj_set_size(s_tileview, LV_HOR_RES,
                    LV_VER_RES - TITLE_H - DOTS_H);
    lv_obj_set_style_bg_color(s_tileview, lv_color_hex(CLR_BG), 0);
    lv_obj_add_event_cb(s_tileview, on_tile_changed,
                        LV_EVENT_VALUE_CHANGED, NULL);

    int page_count = (s_vm && s_vm->page_count > 0) ? s_vm->page_count : 1;

    for (int p = 0; p < page_count; p++) {
        lv_obj_t *tile = lv_tileview_add_tile(s_tileview, p, 0, LV_DIR_HOR);
        lv_obj_set_style_bg_color(tile, lv_color_hex(CLR_BG), 0);
        lv_obj_set_user_data(tile, (void *)(intptr_t)p);

        int row_count = (s_vm && p < s_vm->page_count)
                        ? s_vm->pages[p].count : 0;

        if (row_count == 0) {
            lv_obj_t *empty = lv_label_create(tile);
            lv_label_set_text(empty, "Geen entiteiten op deze pagina");
            lv_obj_set_style_text_color(empty, lv_color_hex(0x9E9E9E), 0);
            lv_obj_center(empty);
            continue;
        }

        for (int r = 0; r < row_count; r++) {
            entity_t *e = s_vm->pages[p].entities[r];
            if (!e) continue;

            lv_obj_t *row = lv_obj_create(tile);
            lv_obj_set_pos(row, 0, r * ROW_H);
            lv_obj_set_size(row, LV_HOR_RES, ROW_H);
            lv_obj_set_style_bg_color(row, lv_color_hex(CLR_BG), 0);
            lv_obj_set_style_border_color(row, lv_color_hex(CLR_SEP), 0);
            lv_obj_set_style_border_width(row, 1, 0);
            lv_obj_set_style_border_side(row, LV_BORDER_SIDE_BOTTOM, 0);
            lv_obj_set_style_pad_all(row, 0, 0);
            lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

            ui_widgets_render_row(row, e);
        }
    }

    /* Paginering-dots */
    s_dots = build_dots(screen, page_count);

    return screen;
}

/* ---- Realtime update van entiteitswidgets ---- */

void ui_entities_update(const ha_event_t *evt) {
    if (!s_vm) return;

    entity_t *e = entities_find(s_vm, evt->entity_id);
    if (!e) return;

    strncpy(e->state, evt->state, sizeof(e->state) - 1);
    if (evt->brightness_pct >= 0.0f) e->brightness_pct = evt->brightness_pct;
    if (evt->temperature    >= 0.0f) e->temperature    = evt->temperature;
    e->available = (strcmp(evt->state, "unavailable") != 0);

    /* Naamgeving als Lovelace: kaartnaam (name:) heeft voorrang,
       anders de friendly_name van de entiteit */
    if (evt->friendly_name[0] && !e->name_custom) {
        strncpy(e->name, evt->friendly_name, sizeof(e->name) - 1);
        e->name[sizeof(e->name) - 1] = '\0';
    }

    /* Climate (US-011) — alleen overnemen wat het event meelevert */
    if (evt->fan_mode[0]) {
        strncpy(e->fan_mode, evt->fan_mode, sizeof(e->fan_mode) - 1);
    }
    if (evt->fan_mode_count > 0) {
        memcpy(e->fan_modes, evt->fan_modes, sizeof(e->fan_modes));
        e->fan_mode_count = evt->fan_mode_count;
    }
    if (evt->hvac_mode_count > 0) {
        memcpy(e->hvac_modes, evt->hvac_modes, sizeof(e->hvac_modes));
        e->hvac_mode_count = evt->hvac_mode_count;
    }

    ui_widgets_update(e);
}
