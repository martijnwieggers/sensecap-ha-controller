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

#define TITLE_H     40
#define ROW_H       ((LV_VER_RES - TITLE_H) / ENTITIES_PER_PAGE)
#define SCROLLBAR_W 5

static view_model_t *s_vm       = NULL;
static lv_obj_t     *s_pages[MAX_PAGES] = {NULL};
static int           s_page_count = 0;
static int           s_cur_page   = 0;
static lv_obj_t     *s_scroll_thumb = NULL;

/* ---- Scrollbalk (US-016): positie-indicator rechts, springt per pagina ---- */

static void update_scrollbar(int active_page) {
    if (!s_scroll_thumb || s_page_count <= 0) return;
    lv_obj_set_y(s_scroll_thumb,
                 active_page * ((LV_VER_RES - TITLE_H) / s_page_count));
}

/* ---- Paginawissel zonder schuiven (veeg = direct vervangen) ---- */

static void show_page(int page) {
    if (page < 0 || page >= s_page_count || page == s_cur_page) return;
    lv_obj_add_flag(s_pages[s_cur_page], LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(s_pages[page], LV_OBJ_FLAG_HIDDEN);
    s_cur_page = page;
    update_scrollbar(page);
}

static void gesture_cb(lv_event_t *e) {
    lv_indev_t *indev = lv_indev_get_act();
    if (!indev) return;
    lv_dir_t dir = lv_indev_get_gesture_dir(indev);
    if (dir != LV_DIR_TOP && dir != LV_DIR_BOTTOM) return;

    /* Rest van deze aanraking negeren: het loslaten mag het widget waarop
       de veeg begon niet bedienen (slider zou LV_EVENT_RELEASED krijgen en
       een HA-commando sturen; het widget krijgt nu alleen PRESS_LOST) */
    lv_indev_wait_release(indev);

    if (dir == LV_DIR_TOP)    show_page(s_cur_page + 1);  /* omhoog = volgende */
    if (dir == LV_DIR_BOTTOM) show_page(s_cur_page - 1);  /* omlaag = vorige  */
}

/* ---- Scrollbalk aanmaken: permanent zichtbare indicator (geen bediening),
        zweeft rechts over de rijen — hoogte van het duimpje = 1/page_count ---- */

static lv_obj_t *build_scrollbar(lv_obj_t *parent, int page_count) {
    int track_h = LV_VER_RES - TITLE_H;

    lv_obj_t *track = lv_obj_create(parent);
    lv_obj_set_size(track, SCROLLBAR_W, track_h);
    lv_obj_set_pos(track, LV_HOR_RES - SCROLLBAR_W - 2, TITLE_H);
    lv_obj_set_style_bg_color(track, lv_color_hex(CLR_SEP), 0);
    lv_obj_set_style_radius(track, SCROLLBAR_W / 2, 0);
    lv_obj_set_style_border_width(track, 0, 0);
    lv_obj_set_style_pad_all(track, 0, 0);
    lv_obj_clear_flag(track, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *thumb = lv_obj_create(track);
    lv_obj_set_size(thumb, SCROLLBAR_W, track_h / page_count);
    lv_obj_set_pos(thumb, 0, 0);
    lv_obj_set_style_bg_color(thumb, lv_color_hex(CLR_ACCENT), 0);
    lv_obj_set_style_radius(thumb, SCROLLBAR_W / 2, 0);
    lv_obj_set_style_border_width(thumb, 0, 0);
    lv_obj_set_style_pad_all(thumb, 0, 0);
    lv_obj_clear_flag(thumb, LV_OBJ_FLAG_CLICKABLE);

    return thumb;
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

    /* Gestapelde pagina-containers: alleen de actieve is zichtbaar; een
       verticale veeg (gesture op het scherm) vervangt de pagina direct —
       geen scroll-animatie (oogt schokkerig op het RGB-panel, US-016) */
    s_page_count = (s_vm && s_vm->page_count > 0) ? s_vm->page_count : 1;
    s_cur_page   = 0;
    lv_obj_add_event_cb(screen, gesture_cb, LV_EVENT_GESTURE, NULL);

    for (int p = 0; p < s_page_count; p++) {
        lv_obj_t *page = lv_obj_create(screen);
        lv_obj_set_pos(page, 0, TITLE_H);
        lv_obj_set_size(page, LV_HOR_RES, LV_VER_RES - TITLE_H);
        lv_obj_set_style_bg_color(page, lv_color_hex(CLR_BG), 0);
        lv_obj_set_style_border_width(page, 0, 0);
        lv_obj_set_style_pad_all(page, 0, 0);
        lv_obj_clear_flag(page, LV_OBJ_FLAG_SCROLLABLE);
        if (p != 0) lv_obj_add_flag(page, LV_OBJ_FLAG_HIDDEN);
        s_pages[p] = page;

        int row_count = (s_vm && p < s_vm->page_count)
                        ? s_vm->pages[p].count : 0;

        if (row_count == 0) {
            lv_obj_t *empty = lv_label_create(page);
            lv_label_set_text(empty, "Geen entiteiten op deze pagina");
            lv_obj_set_style_text_color(empty, lv_color_hex(0x9E9E9E), 0);
            lv_obj_center(empty);
            continue;
        }

        for (int r = 0; r < row_count; r++) {
            entity_t *e = s_vm->pages[p].entities[r];
            if (!e) continue;

            lv_obj_t *row = lv_obj_create(page);
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

    /* Scrollbalk rechts (na de pagina's aangemaakt: tekent eroverheen) */
    s_scroll_thumb = build_scrollbar(screen, s_page_count);

    return screen;
}

/* ---- Realtime update van entiteitswidgets ---- */

void ui_entities_update(const ha_event_t *evt) {
    if (!s_vm) return;

    entity_t *e = entities_find(s_vm, evt->entity_id);
    if (!e) return;

    strncpy(e->state, evt->state, sizeof(e->state) - 1);
    e->state[sizeof(e->state) - 1] = '\0';
    if (evt->brightness_pct >= 0.0f) e->brightness_pct = evt->brightness_pct;
    if (evt->temperature    >= 0.0f) e->temperature    = evt->temperature;
    if (evt->dimmable) e->dimmable = (evt->dimmable == 1);   /* 0 = onbekend */
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
        e->fan_mode[sizeof(e->fan_mode) - 1] = '\0';
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
