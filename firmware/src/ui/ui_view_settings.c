#include "ui_view_settings.h"
#include "ui_manager.h"
#include "../ha/ha_client.h"
#include "../ha/ha_lovelace.h"
#include "../platform/storage.h"
#include <string.h>
#include <stdio.h>

#define CLR_BG      0x1A1A2E
#define CLR_PANEL   0x16213E
#define CLR_ACCENT  0x4FC3F7
#define CLR_TEXT    0xE0E0E0
#define CLR_SUBTEXT 0x9E9E9E
#define CLR_SEP     0x2A2A4A

/* Ruimte voor MAX_VIEWS paden (64) plus komma's */
#define ENABLED_VIEWS_MAX (MAX_VIEWS * 65)

static lv_obj_t *s_screen = NULL;
static lv_obj_t *s_list   = NULL;
static lv_obj_t *s_hint   = NULL;

/* ---- NVS-helpers (geen LVGL) ---- */

static bool list_contains(const char *csv, const char *path) {
    if (!path[0]) return false;
    size_t plen = strlen(path);
    const char *p = csv;
    while (*p) {
        const char *comma = strchr(p, ',');
        size_t len = comma ? (size_t)(comma - p) : strlen(p);
        if (len == plen && strncmp(p, path, plen) == 0) return true;
        p = comma ? comma + 1 : p + len;
    }
    return false;
}

bool view_settings_is_enabled(const char *path) {
    char csv[ENABLED_VIEWS_MAX];
    storage_get_string("enabled_views", csv, sizeof(csv), "");
    if (!csv[0]) return true;   /* geen filter ingesteld */
    return list_contains(csv, path);
}

int view_settings_enabled_count(char *first_out, size_t out_len) {
    char csv[ENABLED_VIEWS_MAX];
    storage_get_string("enabled_views", csv, sizeof(csv), "");
    if (first_out && out_len) first_out[0] = '\0';

    int count = 0;
    const char *p = csv;
    while (*p) {
        const char *comma = strchr(p, ',');
        size_t len = comma ? (size_t)(comma - p) : strlen(p);
        if (len) {
            if (count == 0 && first_out && len < out_len) {
                memcpy(first_out, p, len);
                first_out[len] = '\0';
            }
            count++;
        }
        p = comma ? comma + 1 : p + len;
    }
    return count;
}

/* ---- Opslaan en hint ---- */

static void update_hint(void) {
    if (!s_hint) return;
    int n = view_settings_enabled_count(NULL, 0);
    char buf[96];
    if (n == 0) {
        snprintf(buf, sizeof(buf),
                 "Niets aangevinkt: alle views blijven beschikbaar.");
    } else if (n == 1) {
        snprintf(buf, sizeof(buf),
                 "1 view actief: het keuzemenu wordt bij het\n"
                 "opstarten overgeslagen.");
    } else {
        snprintf(buf, sizeof(buf),
                 "%d views actief in het keuzemenu.", n);
    }
    lv_label_set_text(s_hint, buf);
}

static void save_from_checkboxes(void) {
    char csv[ENABLED_VIEWS_MAX] = {0};
    size_t pos = 0;

    uint32_t n = lv_obj_get_child_cnt(s_list);
    for (uint32_t i = 0; i < n; i++) {
        lv_obj_t *cb = lv_obj_get_child(s_list, i);
        if (!lv_obj_check_type(cb, &lv_checkbox_class)) continue;
        if (!lv_obj_has_state(cb, LV_STATE_CHECKED)) continue;

        const ha_view_info_t *v =
            (const ha_view_info_t *)lv_obj_get_user_data(cb);
        if (!v || !v->path[0]) continue;

        int written = snprintf(csv + pos, sizeof(csv) - pos, "%s%s",
                               pos ? "," : "", v->path);
        if (written < 0 || pos + (size_t)written >= sizeof(csv)) break;
        pos += (size_t)written;
    }
    storage_set_string("enabled_views", csv);
    update_hint();
}

static void checkbox_cb(lv_event_t *e) {
    (void)e;
    save_from_checkboxes();
}

/* ---- Lijst opbouwen ---- */

static void populate(void) {
    lv_obj_clean(s_list);

    int count = ha_lovelace_get_view_count();
    if (count == 0) {
        lv_obj_t *lbl = lv_label_create(s_list);
        lv_label_set_text(lbl, "Views ophalen uit Home Assistant...");
        lv_obj_set_style_text_color(lbl, lv_color_hex(CLR_SUBTEXT), 0);
        return;
    }

    for (int i = 0; i < count; i++) {
        const ha_view_info_t *view = ha_lovelace_get_view(i);
        /* Pad is altijd gevuld: views zonder pad in HA kregen bij het
           parsen een synthetisch "#<index>"-pad */
        if (!view || !view->path[0]) continue;

        lv_obj_t *cb = lv_checkbox_create(s_list);
        lv_checkbox_set_text(cb, view->title);
        lv_obj_set_width(cb, LV_HOR_RES - 48);
        lv_obj_set_style_text_color(cb, lv_color_hex(CLR_TEXT), 0);
        lv_obj_set_style_pad_ver(cb, 14, 0);
        lv_obj_set_style_bg_color(cb, lv_color_hex(CLR_ACCENT),
                                  LV_PART_INDICATOR | LV_STATE_CHECKED);
        lv_obj_set_user_data(cb, (void *)view);

        /* Zonder filter (lege lijst) telt elke view als aangevinkt */
        if (view_settings_is_enabled(view->path)) {
            lv_obj_add_state(cb, LV_STATE_CHECKED);
        }
        lv_obj_add_event_cb(cb, checkbox_cb, LV_EVENT_VALUE_CHANGED, NULL);
    }
    update_hint();
}

/* ---- Knop-callbacks ---- */

static void back_btn_cb(lv_event_t *e) {
    (void)e;
    ui_manager_show_status();
}

static void refresh_btn_cb(lv_event_t *e) {
    (void)e;
    lv_obj_clean(s_list);
    lv_obj_t *lbl = lv_label_create(s_list);
    lv_label_set_text(lbl, "Ophalen...");
    lv_obj_set_style_text_color(lbl, lv_color_hex(CLR_SUBTEXT), 0);
    ha_client_get_views();
    /* Lijst wordt opnieuw opgebouwd via HA_EVT_VIEWS_LOADED */
}

/* ---- Publieke API ---- */

lv_obj_t *ui_view_settings_create(void) {
    s_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_screen, lv_color_hex(CLR_BG), 0);

    /* Titelbalk met terug- en vernieuwen-knop */
    lv_obj_t *title_bar = lv_obj_create(s_screen);
    lv_obj_set_size(title_bar, LV_HOR_RES, 48);
    lv_obj_set_pos(title_bar, 0, 0);
    lv_obj_set_style_bg_color(title_bar, lv_color_hex(CLR_PANEL), 0);
    lv_obj_set_style_border_width(title_bar, 0, 0);
    lv_obj_set_style_pad_all(title_bar, 0, 0);

    lv_obj_t *back_btn = lv_btn_create(title_bar);
    lv_obj_set_size(back_btn, 44, 36);
    lv_obj_align(back_btn, LV_ALIGN_LEFT_MID, 6, 0);
    lv_obj_set_style_bg_color(back_btn, lv_color_hex(CLR_SEP), 0);
    lv_obj_t *back_lbl = lv_label_create(back_btn);
    lv_label_set_text(back_lbl, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_color(back_lbl, lv_color_hex(CLR_TEXT), 0);
    lv_obj_center(back_lbl);
    lv_obj_add_event_cb(back_btn, back_btn_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *title = lv_label_create(title_bar);
    lv_label_set_text(title, "Views");
    lv_obj_set_style_text_color(title, lv_color_hex(CLR_TEXT), 0);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 60, 0);

    lv_obj_t *ref_btn = lv_btn_create(title_bar);
    lv_obj_set_size(ref_btn, 44, 36);
    lv_obj_align(ref_btn, LV_ALIGN_RIGHT_MID, -6, 0);
    lv_obj_set_style_bg_color(ref_btn, lv_color_hex(CLR_SEP), 0);
    lv_obj_t *ref_lbl = lv_label_create(ref_btn);
    lv_label_set_text(ref_lbl, LV_SYMBOL_REFRESH);
    lv_obj_set_style_text_color(ref_lbl, lv_color_hex(CLR_TEXT), 0);
    lv_obj_center(ref_lbl);
    lv_obj_add_event_cb(ref_btn, refresh_btn_cb, LV_EVENT_CLICKED, NULL);

    /* Scrollbare checkbox-lijst */
    s_list = lv_obj_create(s_screen);
    lv_obj_set_pos(s_list, 0, 48);
    lv_obj_set_size(s_list, LV_HOR_RES, LV_VER_RES - 48 - 96);
    lv_obj_set_style_bg_color(s_list, lv_color_hex(CLR_BG), 0);
    lv_obj_set_style_border_width(s_list, 0, 0);
    lv_obj_set_style_pad_all(s_list, 16, 0);
    lv_obj_set_flex_flow(s_list, LV_FLEX_FLOW_COLUMN);

    /* Uitleg/teller onderaan */
    s_hint = lv_label_create(s_screen);
    lv_obj_set_width(s_hint, LV_HOR_RES - 32);
    lv_obj_align(s_hint, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_set_style_text_color(s_hint, lv_color_hex(CLR_SUBTEXT), 0);
    lv_label_set_long_mode(s_hint, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(s_hint, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(s_hint, "");

    populate();
    if (ha_lovelace_get_view_count() == 0) {
        /* Nog geen viewlijst in het geheugen: opvragen bij HA */
        ha_client_get_views();
    }
    return s_screen;
}

void ui_view_settings_refresh(void) {
    if (s_list) populate();
}
