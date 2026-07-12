#include "ui_status.h"
#include "ui_manager.h"
#include "../platform/storage.h"
#include "../platform/wifi.h"
#include "../app/app_state.h"
#include "../ha/ha_lovelace.h"
#include "lvgl.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#define CLR_BG      0x1A1A2E
#define CLR_PANEL   0x16213E
#define CLR_ACCENT  0x4FC3F7
#define CLR_TEXT    0xE0E0E0
#define CLR_SUBTEXT 0x9E9E9E
#define CLR_ERROR   0xEF5350
#define CLR_SEP     0x2A2A4A
#define CLR_OK      0x66BB6A

#define N_BARS 4

static lv_obj_t *s_lbl_wifi   = NULL;
static lv_obj_t *s_bars[N_BARS] = {NULL};
static lv_obj_t *s_lbl_ip     = NULL;
static lv_obj_t *s_lbl_ha_url = NULL;   /* geconfigureerd HA-adres */
static lv_obj_t *s_lbl_ha     = NULL;
static lv_obj_t *s_lbl_view   = NULL;
static lv_obj_t *s_lbl_uptime = NULL;
static lv_obj_t *s_lbl_msg    = NULL;   /* verbindingsmelding (US-009) */

static bool s_auth_failed = false;      /* HA wees het token af */

/* ---- Hulpfuncties ---- */

static int rssi_to_bars(int rssi) {
    if (rssi >= -55) return 4;
    if (rssi >= -67) return 3;
    if (rssi >= -78) return 2;
    if (rssi >= -88) return 1;
    return 0;
}

static bool ha_is_connected(void) {
    app_state_t st = app_state_get();
    return st == STATE_VIEW_SELECT || st == STATE_ENTITIES_LOADING ||
           st == STATE_VIEW_READY;
}

/* Zoek de titel van de opgeslagen view op in de lovelace-viewlijst */
static void active_view_name(char *out, size_t len) {
    char path[64] = {0};
    storage_get_string("selected_view", path, sizeof(path), "");
    if (!path[0]) {
        snprintf(out, len, "geen");
        return;
    }
    for (int i = 0; i < ha_lovelace_get_view_count(); i++) {
        const ha_view_info_t *v = ha_lovelace_get_view(i);
        if (v && strcmp(v->path, path) == 0) {
            snprintf(out, len, "%s", v->title);
            return;
        }
    }
    snprintf(out, len, "%s", path);   /* titel onbekend: toon het pad */
}

/* ---- Periodieke refresh ---- */

static void refresh_cb(lv_timer_t *timer) {
    (void)timer;
    char buf[140];   /* moet ook "HA-adres: " + volledige URL (119) kunnen dragen */

    /* WiFi: SSID + dBm + balkjes */
    wifi_ap_record_t ap;
    bool wifi_ok = (esp_wifi_sta_get_ap_info(&ap) == ESP_OK);
    int bars = wifi_ok ? rssi_to_bars(ap.rssi) : 0;
    if (wifi_ok) {
        snprintf(buf, sizeof(buf), "WiFi: %s (%d dBm)", ap.ssid, ap.rssi);
    } else {
        snprintf(buf, sizeof(buf), "WiFi: niet verbonden");
    }
    lv_label_set_text(s_lbl_wifi, buf);
    for (int i = 0; i < N_BARS; i++) {
        lv_obj_set_style_bg_color(s_bars[i],
            i < bars ? lv_color_hex(CLR_ACCENT) : lv_color_hex(CLR_SEP), 0);
    }

    /* IP-adres */
    char ip[20] = {0};
    if (wifi_ok) wifi_get_ip(ip, sizeof(ip));
    snprintf(buf, sizeof(buf), "IP: %s", ip[0] ? ip : "-");
    lv_label_set_text(s_lbl_ip, buf);

    /* Geconfigureerd HA-adres */
    char ha_url[120] = {0};
    storage_get_string("ha_url", ha_url, sizeof(ha_url), "");
    snprintf(buf, sizeof(buf), "HA-adres: %s",
             ha_url[0] ? ha_url : "niet ingesteld");
    lv_label_set_text(s_lbl_ha_url, buf);

    /* HA-verbindingsstatus, met tussenfases zodat zichtbaar is wáár
       het verbinden blijft hangen (TCP/TLS vs. authenticatie vs. token) */
    const char *ha_txt;
    uint32_t    ha_clr;
    if (s_auth_failed) {
        ha_txt = "HA: Token ongeldig";       ha_clr = CLR_ERROR;
    } else if (ha_is_connected()) {
        ha_txt = "HA: Verbonden";            ha_clr = CLR_OK;
    } else if (app_state_is(STATE_HA_CONNECTING)) {
        ha_txt = "HA: Verbinden...";         ha_clr = CLR_SUBTEXT;
    } else if (app_state_is(STATE_HA_AUTH)) {
        ha_txt = "HA: Authenticeren...";     ha_clr = CLR_SUBTEXT;
    } else {
        ha_txt = "HA: Verbroken";            ha_clr = CLR_ERROR;
    }
    lv_label_set_text(s_lbl_ha, ha_txt);
    lv_obj_set_style_text_color(s_lbl_ha, lv_color_hex(ha_clr), 0);

    /* Actieve view */
    char view[64];
    active_view_name(view, sizeof(view));
    snprintf(buf, sizeof(buf), "Actieve view: %s", view);
    lv_label_set_text(s_lbl_view, buf);

    /* Uptime */
    uint64_t us = esp_timer_get_time();
    uint32_t s_total = (uint32_t)(us / 1000000ULL);
    uint32_t d = s_total / 86400, h = (s_total % 86400) / 3600;
    uint32_t m = (s_total % 3600) / 60, s2 = s_total % 60;
    snprintf(buf, sizeof(buf),
             "Uptime: %" PRIu32 "d %" PRIu32 "u %" PRIu32 "m %" PRIu32 "s",
             d, h, m, s2);
    lv_label_set_text(s_lbl_uptime, buf);

    /* Verbindingsmelding (US-009): onderscheid WiFi-, HA- en token-probleem */
    if (app_state_is(STATE_DISCONNECTED)) {
        lv_label_set_text(s_lbl_msg,
            s_auth_failed
            ? "Home Assistant weigert het token.\n"
              "Controleer het token via Instellingen."
            : wifi_ok
            ? "Verbinding verbroken \xE2\x80\x94 Home Assistant onbereikbaar.\n"
              "Opnieuw verbinden..."
            : "Verbinding verbroken \xE2\x80\x94 WiFi-netwerk weggevallen.\n"
              "Controleer het netwerk; opnieuw verbinden...");
        lv_obj_clear_flag(s_lbl_msg, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(s_lbl_msg, LV_OBJ_FLAG_HIDDEN);
    }
}

void ui_status_refresh(void) {
    if (s_lbl_wifi) refresh_cb(NULL);
}

void ui_status_set_auth_failed(bool failed) {
    s_auth_failed = failed;
}

/* ---- Knop-callbacks ---- */

static void back_btn_cb(lv_event_t *e) {
    (void)e;
    view_model_t *vm = ha_lovelace_get_view_model();
    if (vm && vm->total_entities > 0) {
        ui_manager_show_entities();
    } else {
        ui_manager_show_view_menu();
    }
}

static void settings_btn_cb(lv_event_t *e) {
    (void)e;
    ui_manager_show_setup();
}

static void views_btn_cb(lv_event_t *e) {
    (void)e;
    ui_manager_show_view_settings();
}

/* ---- Schermopbouw ---- */

lv_obj_t *ui_status_create(void) {
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, lv_color_hex(CLR_BG), 0);

    /* Titelbalk met terug-knop */
    lv_obj_t *title_bar = lv_obj_create(screen);
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
    lv_label_set_text(title, "Status");
    lv_obj_set_style_text_color(title, lv_color_hex(CLR_TEXT), 0);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 60, 0);

    /* Infovelden */
    int y = 64;
    lv_obj_t *lbl_ver = lv_label_create(screen);
    lv_label_set_text(lbl_ver, "Versie: " APP_VERSION);
    lv_obj_set_style_text_color(lbl_ver, lv_color_hex(CLR_SUBTEXT), 0);
    lv_obj_align(lbl_ver, LV_ALIGN_TOP_LEFT, 16, y); y += 36;

    s_lbl_wifi = lv_label_create(screen);
    lv_obj_set_style_text_color(s_lbl_wifi, lv_color_hex(CLR_TEXT), 0);
    lv_obj_align(s_lbl_wifi, LV_ALIGN_TOP_LEFT, 16, y);

    /* Signaalsterkte-balkjes rechts van het WiFi-label */
    for (int i = 0; i < N_BARS; i++) {
        s_bars[i] = lv_obj_create(screen);
        lv_obj_set_size(s_bars[i], 7, 6 + i * 5);
        lv_obj_align(s_bars[i], LV_ALIGN_TOP_RIGHT,
                     -16 - (N_BARS - 1 - i) * 10, y + 21 - (6 + i * 5));
        lv_obj_set_style_radius(s_bars[i], 1, 0);
        lv_obj_set_style_border_width(s_bars[i], 0, 0);
        lv_obj_set_style_bg_color(s_bars[i], lv_color_hex(CLR_SEP), 0);
        lv_obj_clear_flag(s_bars[i], LV_OBJ_FLAG_SCROLLABLE);
    }
    y += 36;

    s_lbl_ip = lv_label_create(screen);
    lv_obj_set_style_text_color(s_lbl_ip, lv_color_hex(CLR_TEXT), 0);
    lv_obj_align(s_lbl_ip, LV_ALIGN_TOP_LEFT, 16, y); y += 36;

    s_lbl_ha_url = lv_label_create(screen);
    lv_obj_set_style_text_color(s_lbl_ha_url, lv_color_hex(CLR_TEXT), 0);
    lv_label_set_long_mode(s_lbl_ha_url, LV_LABEL_LONG_DOT);
    lv_obj_set_width(s_lbl_ha_url, LV_HOR_RES - 32);
    lv_obj_align(s_lbl_ha_url, LV_ALIGN_TOP_LEFT, 16, y); y += 36;

    s_lbl_ha = lv_label_create(screen);
    lv_obj_align(s_lbl_ha, LV_ALIGN_TOP_LEFT, 16, y); y += 36;

    s_lbl_view = lv_label_create(screen);
    lv_obj_set_style_text_color(s_lbl_view, lv_color_hex(CLR_TEXT), 0);
    lv_label_set_long_mode(s_lbl_view, LV_LABEL_LONG_DOT);
    lv_obj_set_width(s_lbl_view, LV_HOR_RES - 32);
    lv_obj_align(s_lbl_view, LV_ALIGN_TOP_LEFT, 16, y); y += 36;

    s_lbl_uptime = lv_label_create(screen);
    lv_obj_set_style_text_color(s_lbl_uptime, lv_color_hex(CLR_SUBTEXT), 0);
    lv_obj_align(s_lbl_uptime, LV_ALIGN_TOP_LEFT, 16, y);

    /* Verbindingsmelding (US-009) — alleen zichtbaar bij STATE_DISCONNECTED */
    s_lbl_msg = lv_label_create(screen);
    lv_obj_set_style_text_color(s_lbl_msg, lv_color_hex(CLR_ERROR), 0);
    lv_label_set_long_mode(s_lbl_msg, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s_lbl_msg, LV_HOR_RES - 32);
    lv_obj_set_style_text_align(s_lbl_msg, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(s_lbl_msg, LV_ALIGN_BOTTOM_MID, 0, -84);
    lv_obj_add_flag(s_lbl_msg, LV_OBJ_FLAG_HIDDEN);

    /* Views- en Instellingen-knop naast elkaar onderaan */
    lv_obj_t *views_btn = lv_btn_create(screen);
    lv_obj_set_size(views_btn, (LV_HOR_RES - 48) / 2, 52);
    lv_obj_align(views_btn, LV_ALIGN_BOTTOM_LEFT, 16, -16);
    lv_obj_set_style_bg_color(views_btn, lv_color_hex(CLR_SEP), 0);
    lv_obj_t *views_lbl = lv_label_create(views_btn);
    lv_label_set_text(views_lbl, LV_SYMBOL_LIST "  Views");
    lv_obj_set_style_text_color(views_lbl, lv_color_hex(CLR_TEXT), 0);
    lv_obj_center(views_lbl);
    lv_obj_add_event_cb(views_btn, views_btn_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *set_btn = lv_btn_create(screen);
    lv_obj_set_size(set_btn, (LV_HOR_RES - 48) / 2, 52);
    lv_obj_align(set_btn, LV_ALIGN_BOTTOM_RIGHT, -16, -16);
    lv_obj_set_style_bg_color(set_btn, lv_color_hex(CLR_SEP), 0);
    lv_obj_t *set_lbl = lv_label_create(set_btn);
    lv_label_set_text(set_lbl, LV_SYMBOL_SETTINGS "  Instellingen");
    lv_obj_set_style_text_color(set_lbl, lv_color_hex(CLR_TEXT), 0);
    lv_obj_center(set_lbl);
    lv_obj_add_event_cb(set_btn, settings_btn_cb, LV_EVENT_CLICKED, NULL);

    refresh_cb(NULL);
    lv_timer_create(refresh_cb, 5000, NULL);
    return screen;
}
