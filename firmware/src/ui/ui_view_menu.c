#include "ui_view_menu.h"
#include "ui_manager.h"
#include "../ha/ha_client.h"
#include "../ha/ha_lovelace.h"
#include "../platform/storage.h"
#include "lvgl.h"
#include <string.h>
#include <stdio.h>

#define CLR_BG      0x1A1A2E
#define CLR_PANEL   0x16213E
#define CLR_ACCENT  0x4FC3F7
#define CLR_TEXT    0xE0E0E0
#define CLR_SUBTEXT 0x9E9E9E
#define CLR_SEP     0x2A2A4A

static lv_obj_t *s_screen       = NULL;
static lv_obj_t *s_list         = NULL;
static lv_obj_t *s_status_lbl   = NULL;

/* ---- View-selectie ---- */

static void select_view(const ha_view_info_t *view) {
    storage_set_string("selected_view", view->path);
    ha_client_load_view(view->path);
    /* Scherm wordt gewisseld via HA_EVT_ENTITIES_LOADED → app_events_handle */
}

static void view_btn_cb(lv_event_t *e) {
    const ha_view_info_t *view =
        (const ha_view_info_t *)lv_event_get_user_data(e);
    if (!view) return;

    /* Markeer geselecteerde knop visueel */
    lv_obj_t *btn  = lv_event_get_target(e);
    lv_obj_t *list = lv_obj_get_parent(btn);
    uint32_t child_cnt = lv_obj_get_child_cnt(list);
    for (uint32_t i = 0; i < child_cnt; i++) {
        lv_obj_t *c = lv_obj_get_child(list, i);
        lv_obj_set_style_bg_color(c, lv_color_hex(CLR_PANEL), 0);
    }
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x1E3A5F), 0);

    lv_label_set_text(s_status_lbl, "View laden...");
    select_view(view);
}

/* ---- Lijst opbouwen ---- */

static void populate_list(void) {
    lv_obj_clean(s_list);

    int count = ha_lovelace_get_view_count();
    if (count == 0) {
        lv_list_add_text(s_list, "Geen views beschikbaar");
        lv_label_set_text(s_status_lbl, "Geen Lovelace views gevonden in Home Assistant.");
        return;
    }

    lv_label_set_text(s_status_lbl, "");

    /* Geselecteerde view uit NVS */
    char saved_path[64] = {0};
    storage_get_string("selected_view", saved_path, sizeof(saved_path), "");

    for (int i = 0; i < count; i++) {
        const ha_view_info_t *view = ha_lovelace_get_view(i);
        if (!view) continue;

        char label[80];
        snprintf(label, sizeof(label), "  %s", view->title);

        lv_obj_t *btn = lv_list_add_btn(s_list, LV_SYMBOL_HOME, label);
        lv_obj_set_style_bg_color(btn, lv_color_hex(CLR_PANEL), 0);
        lv_obj_set_style_text_color(btn, lv_color_hex(CLR_TEXT), 0);
        lv_obj_set_style_border_color(btn, lv_color_hex(CLR_SEP), 0);
        lv_obj_set_style_border_width(btn, 0, LV_PART_MAIN);
        lv_obj_set_height(btn, 60);

        /* Markeer opgeslagen/vorige selectie */
        if (saved_path[0] && strcmp(view->path, saved_path) == 0) {
            lv_obj_set_style_bg_color(btn, lv_color_hex(0x1E3A5F), 0);
        }

        lv_obj_add_event_cb(btn, view_btn_cb, LV_EVENT_CLICKED,
                            (void *)view);
    }
}

/* ---- Refresh-knop ---- */

static void refresh_btn_cb(lv_event_t *e) {
    lv_label_set_text(s_status_lbl, "Ophalen...");
    lv_obj_clean(s_list);
    lv_list_add_text(s_list, "Verbinden met Home Assistant...");
    ha_client_get_views();
}

/* ---- Instellingen-knop ---- */

static void settings_btn_cb(lv_event_t *e) {
    ui_manager_show_setup();
}

/* ---- Publieke API ---- */

lv_obj_t *ui_view_menu_create(void) {
    s_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_screen, lv_color_hex(CLR_BG), 0);

    /* Titelbalk */
    lv_obj_t *title_bar = lv_obj_create(s_screen);
    lv_obj_set_size(title_bar, LV_HOR_RES, 48);
    lv_obj_set_pos(title_bar, 0, 0);
    lv_obj_set_style_bg_color(title_bar, lv_color_hex(CLR_PANEL), 0);
    lv_obj_set_style_border_width(title_bar, 0, 0);
    lv_obj_set_style_pad_all(title_bar, 0, 0);

    lv_obj_t *title = lv_label_create(title_bar);
    lv_label_set_text(title, "Kies een view");
    lv_obj_set_style_text_color(title, lv_color_hex(CLR_TEXT), 0);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 12, 0);

    /* Instellingen-knop (gear) */
    lv_obj_t *set_btn = lv_btn_create(title_bar);
    lv_obj_set_size(set_btn, 44, 36);
    lv_obj_align(set_btn, LV_ALIGN_RIGHT_MID, -56, 0);
    lv_obj_set_style_bg_color(set_btn, lv_color_hex(CLR_SEP), 0);
    lv_obj_t *set_lbl = lv_label_create(set_btn);
    lv_label_set_text(set_lbl, LV_SYMBOL_SETTINGS);
    lv_obj_set_style_text_color(set_lbl, lv_color_hex(CLR_TEXT), 0);
    lv_obj_center(set_lbl);
    lv_obj_add_event_cb(set_btn, settings_btn_cb, LV_EVENT_CLICKED, NULL);

    /* Vernieuwen-knop */
    lv_obj_t *ref_btn = lv_btn_create(title_bar);
    lv_obj_set_size(ref_btn, 44, 36);
    lv_obj_align(ref_btn, LV_ALIGN_RIGHT_MID, -6, 0);
    lv_obj_set_style_bg_color(ref_btn, lv_color_hex(CLR_SEP), 0);
    lv_obj_t *ref_lbl = lv_label_create(ref_btn);
    lv_label_set_text(ref_lbl, LV_SYMBOL_REFRESH);
    lv_obj_set_style_text_color(ref_lbl, lv_color_hex(CLR_TEXT), 0);
    lv_obj_center(ref_lbl);
    lv_obj_add_event_cb(ref_btn, refresh_btn_cb, LV_EVENT_CLICKED, NULL);

    /* Scrollbare view-lijst */
    s_list = lv_list_create(s_screen);
    lv_obj_set_pos(s_list, 0, 48);
    lv_obj_set_size(s_list, LV_HOR_RES, LV_VER_RES - 48 - 48);
    lv_obj_set_style_bg_color(s_list, lv_color_hex(CLR_BG), 0);
    lv_obj_set_style_border_width(s_list, 0, 0);
    lv_list_add_text(s_list, "Verbinden met Home Assistant...");

    /* Status / instructielabel */
    s_status_lbl = lv_label_create(s_screen);
    lv_obj_set_size(s_status_lbl, LV_HOR_RES - 24, 40);
    lv_obj_align(s_status_lbl, LV_ALIGN_BOTTOM_MID, 0, -4);
    lv_label_set_text(s_status_lbl, "Wachten op Home Assistant...");
    lv_obj_set_style_text_color(s_status_lbl, lv_color_hex(CLR_SUBTEXT), 0);
    lv_label_set_long_mode(s_status_lbl, LV_LABEL_LONG_DOT);

    return s_screen;
}

void ui_view_menu_refresh(void) {
    if (!s_list) return;
    populate_list();
}
