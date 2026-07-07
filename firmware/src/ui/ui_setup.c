#include "ui_setup.h"
#include "ui_manager.h"
#include "../platform/storage.h"
#include "../platform/wifi.h"
#include "../platform/web_config.h"
#include "../app/app_state.h"
#include "lvgl.h"
#include "esp_system.h"
#include <string.h>
#include <stdio.h>

/* ---- Kleurpalet ---- */
#define CLR_BG      0x1A1A2E
#define CLR_PANEL   0x16213E
#define CLR_ACCENT  0x4FC3F7
#define CLR_TEXT    0xE0E0E0
#define CLR_SUBTEXT 0x9E9E9E
#define CLR_ERROR   0xEF5350
#define CLR_SEP     0x2A2A4A

/* ---- State ---- */
static lv_obj_t *s_screen      = NULL;
static lv_obj_t *s_step1       = NULL;
static lv_obj_t *s_step2       = NULL;
static lv_obj_t *s_step3       = NULL;
static lv_obj_t *s_keyboard    = NULL;
static lv_obj_t *s_active_ta   = NULL;

/* Step 1 */
static lv_obj_t *s_wifi_list   = NULL;
static lv_obj_t *s_ssid_lbl    = NULL;
static lv_obj_t *s_pwd_ta      = NULL;
static lv_obj_t *s_conn_btn    = NULL;
static lv_obj_t *s_status1_lbl = NULL;
static char      s_sel_ssid[33] = {0};
static wifi_ap_t s_scan_results[WIFI_SCAN_MAX];
static int       s_scan_count = 0;

/* Step 2 — web config */
static lv_obj_t   *s_url_lbl        = NULL;  /* toont IP-adres */
static lv_obj_t   *s_status2_lbl    = NULL;
static lv_obj_t   *s_qr             = NULL;  /* QR-code widget */
static lv_obj_t   *s_manual_panel   = NULL;  /* fallback handmatig */
static lv_obj_t   *s_url_ta         = NULL;
static lv_obj_t   *s_token_ta       = NULL;
static char        s_device_ip[20]  = {0};

/* Timers */
static lv_timer_t *s_scan_timer     = NULL;
static lv_timer_t *s_connect_timer  = NULL;
static lv_timer_t *s_webconf_timer  = NULL;
static int         s_connect_ticks  = 0;

/* Terug-knop alleen tonen als het apparaat al geconfigureerd is:
   dan is de wizard geopend via het tandwiel en moet je terug kunnen. */
static bool        s_can_exit       = false;

/* ================================================================
   Hulpfuncties
   ================================================================ */

static void set_status(lv_obj_t *lbl, const char *msg, bool is_error) {
    if (!lbl) return;
    lv_label_set_text(lbl, msg);
    lv_obj_set_style_text_color(lbl,
        is_error ? lv_color_hex(CLR_ERROR) : lv_color_hex(CLR_SUBTEXT), 0);
}

static void show_step(int step) {
    if (s_step1) lv_obj_add_flag(s_step1, LV_OBJ_FLAG_HIDDEN);
    if (s_step2) lv_obj_add_flag(s_step2, LV_OBJ_FLAG_HIDDEN);
    if (s_step3) lv_obj_add_flag(s_step3, LV_OBJ_FLAG_HIDDEN);
    lv_obj_t *target = (step == 1) ? s_step1 : (step == 2) ? s_step2 : s_step3;
    if (target) lv_obj_clear_flag(target, LV_OBJ_FLAG_HIDDEN);
    if (s_keyboard) lv_obj_add_flag(s_keyboard, LV_OBJ_FLAG_HIDDEN);
}

static void hide_keyboard(void) {
    if (s_keyboard) lv_obj_add_flag(s_keyboard, LV_OBJ_FLAG_HIDDEN);
    s_active_ta = NULL;
}

static void show_keyboard(lv_obj_t *ta) {
    if (!s_keyboard) return;
    lv_keyboard_set_textarea(s_keyboard, ta);
    lv_obj_clear_flag(s_keyboard, LV_OBJ_FLAG_HIDDEN);
    s_active_ta = ta;
    lv_obj_scroll_to_view(ta, LV_ANIM_ON);
}

static void exit_btn_cb(lv_event_t *e) {
    (void)e;
    if (s_scan_timer)    { lv_timer_del(s_scan_timer);    s_scan_timer    = NULL; }
    if (s_connect_timer) { lv_timer_del(s_connect_timer); s_connect_timer = NULL; }
    if (s_webconf_timer) { lv_timer_del(s_webconf_timer); s_webconf_timer = NULL; }
    web_config_stop();
    hide_keyboard();
    app_state_set(STATE_VIEW_SELECT);
    ui_manager_show_view_menu();
}

static lv_obj_t *make_titlebar(lv_obj_t *parent, const char *title) {
    lv_obj_t *bar = lv_obj_create(parent);
    lv_obj_set_size(bar, LV_HOR_RES, 48);
    lv_obj_set_style_bg_color(bar, lv_color_hex(CLR_PANEL), 0);
    lv_obj_set_style_border_width(bar, 0, 0);
    lv_obj_set_style_pad_all(bar, 0, 0);

    int title_x = 12;
    if (s_can_exit) {
        lv_obj_t *back_btn = lv_btn_create(bar);
        lv_obj_set_size(back_btn, 44, 36);
        lv_obj_align(back_btn, LV_ALIGN_LEFT_MID, 6, 0);
        lv_obj_set_style_bg_color(back_btn, lv_color_hex(CLR_SEP), 0);
        lv_obj_t *back_lbl = lv_label_create(back_btn);
        lv_label_set_text(back_lbl, LV_SYMBOL_LEFT);
        lv_obj_set_style_text_color(back_lbl, lv_color_hex(CLR_TEXT), 0);
        lv_obj_center(back_lbl);
        lv_obj_add_event_cb(back_btn, exit_btn_cb, LV_EVENT_CLICKED, NULL);
        title_x = 60;
    }

    lv_obj_t *lbl = lv_label_create(bar);
    lv_label_set_text(lbl, title);
    lv_obj_set_style_text_color(lbl, lv_color_hex(CLR_TEXT), 0);
    lv_obj_align(lbl, LV_ALIGN_LEFT_MID, title_x, 0);
    return bar;
}

static lv_obj_t *make_label(lv_obj_t *parent, const char *txt,
                             uint32_t color) {
    lv_obj_t *lbl = lv_label_create(parent);
    lv_label_set_text(lbl, txt);
    lv_obj_set_style_text_color(lbl, lv_color_hex(color), 0);
    return lbl;
}

static lv_obj_t *make_button(lv_obj_t *parent, const char *txt,
                              lv_event_cb_t cb) {
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_width(btn, LV_HOR_RES - 32);
    lv_obj_set_height(btn, 52);
    lv_obj_set_style_bg_color(btn, lv_color_hex(CLR_ACCENT), 0);
    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, txt);
    lv_obj_set_style_text_color(lbl, lv_color_hex(CLR_BG), 0);
    lv_obj_center(lbl);
    if (cb) lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, NULL);
    return btn;
}

static lv_obj_t *make_step_container(lv_obj_t *parent) {
    lv_obj_t *cont = lv_obj_create(parent);
    lv_obj_set_size(cont, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_pos(cont, 0, 0);
    lv_obj_set_style_bg_color(cont, lv_color_hex(CLR_BG), 0);
    lv_obj_set_style_border_width(cont, 0, 0);
    lv_obj_set_style_pad_all(cont, 0, 0);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    return cont;
}

/* ================================================================
   WiFi-lijst item callback
   ================================================================ */

static void wifi_item_cb(lv_event_t *e) {
    lv_obj_t *btn = lv_event_get_target(e);
    int idx = (int)(intptr_t)lv_obj_get_user_data(btn);
    if (idx < 0 || idx >= s_scan_count) return;
    strncpy(s_sel_ssid, s_scan_results[idx].ssid, sizeof(s_sel_ssid) - 1);
    s_sel_ssid[sizeof(s_sel_ssid) - 1] = '\0';
    char disp[56];
    snprintf(disp, sizeof(disp), "Geselecteerd: %s", s_sel_ssid);
    if (s_ssid_lbl) lv_label_set_text(s_ssid_lbl, disp);
}

/* ================================================================
   WiFi-scan polling
   ================================================================ */

static void scan_poll_cb(lv_timer_t *t) {
    if (!wifi_scan_done()) return;
    lv_timer_del(t);
    s_scan_timer = NULL;
    s_scan_count = wifi_scan_get_results(s_scan_results, WIFI_SCAN_MAX);
    lv_obj_clean(s_wifi_list);
    if (s_scan_count == 0) {
        lv_list_add_text(s_wifi_list, "Geen netwerken gevonden");
        return;
    }
    for (int i = 0; i < s_scan_count; i++) {
        char label[48];
        snprintf(label, sizeof(label), "%s  (%d dBm)",
                 s_scan_results[i].ssid, s_scan_results[i].rssi);
        lv_obj_t *btn = lv_list_add_btn(s_wifi_list, LV_SYMBOL_WIFI, label);
        lv_obj_set_style_bg_color(btn, lv_color_hex(CLR_PANEL), 0);
        lv_obj_set_style_text_color(btn, lv_color_hex(CLR_TEXT), 0);
        lv_obj_set_user_data(btn, (void *)(intptr_t)i);
        lv_obj_add_event_cb(btn, wifi_item_cb, LV_EVENT_CLICKED, NULL);
    }
}

/* ================================================================
   WiFi-verbinding polling → start web config server
   ================================================================ */

static void start_web_config(void) {
    wifi_get_ip(s_device_ip, sizeof(s_device_ip));

    /* URL in stap 2 tonen */
    char url_str[64];
#ifdef SIMULATOR
    snprintf(url_str, sizeof(url_str), "http://localhost:8080");
#else
    snprintf(url_str, sizeof(url_str), "http://%s", s_device_ip);
#endif
    if (s_url_lbl) lv_label_set_text(s_url_lbl, url_str);

    /* QR-code bijwerken */
    if (s_qr) {
        lv_qrcode_update(s_qr, url_str, strlen(url_str));
    }

    web_config_start(s_device_ip);
    set_status(s_status2_lbl, "Vul het formulier in en klik Opslaan...", false);
}

static void webconf_poll_cb(lv_timer_t *t) {
    if (!web_config_is_done()) return;
    lv_timer_del(t);
    s_webconf_timer = NULL;

    web_config_result_t res;
    web_config_get_result(&res);
    web_config_stop();

    /* Sla op in NVS */
    const char *pwd = s_pwd_ta ? lv_textarea_get_text(s_pwd_ta) : "";
    storage_set_string("wifi_ssid", s_sel_ssid);
    storage_set_string("wifi_pass", pwd ? pwd : "");
    storage_set_string("ha_url",    res.ha_url);
    storage_set_string("ha_token",  res.ha_token);
    storage_set_u8("configured", 1);

    show_step(3);
}

static void connect_poll_cb(lv_timer_t *t) {
    s_connect_ticks++;

    if (wifi_is_connected()) {
        lv_timer_del(t);
        s_connect_timer = NULL;
        set_status(s_status1_lbl, "WiFi verbonden!", false);

        /* Stap 2 activeren */
        show_step(2);
        start_web_config();
        s_webconf_timer = lv_timer_create(webconf_poll_cb, 500, NULL);
        lv_timer_set_repeat_count(s_webconf_timer, -1);
        return;
    }

    if (s_connect_ticks > 30) {
        lv_timer_del(t);
        s_connect_timer = NULL;
        set_status(s_status1_lbl,
                   "Verbinding mislukt. Controleer het wachtwoord.", true);
        lv_obj_add_flag(s_conn_btn, LV_OBJ_FLAG_CLICKABLE);
        return;
    }

    char msg[32];
    snprintf(msg, sizeof(msg), "Verbinden%s",
             (s_connect_ticks % 3 == 0) ? "." :
             (s_connect_ticks % 3 == 1) ? ".." : "...");
    set_status(s_status1_lbl, msg, false);
}

/* ================================================================
   Knop-callbacks stap 1
   ================================================================ */

static void scan_btn_cb(lv_event_t *e) {
    if (s_scan_timer) return;
    lv_obj_clean(s_wifi_list);
    lv_list_add_text(s_wifi_list, "Scannen...");
    wifi_scan_start();
    s_scan_timer = lv_timer_create(scan_poll_cb, 500, NULL);
    lv_timer_set_repeat_count(s_scan_timer, -1);
}

static void connect_btn_cb(lv_event_t *e) {
    if (s_sel_ssid[0] == '\0') {
        set_status(s_status1_lbl, "Selecteer eerst een netwerk.", true);
        return;
    }
    const char *pwd = lv_textarea_get_text(s_pwd_ta);
    hide_keyboard();
    s_connect_ticks = 0;
    set_status(s_status1_lbl, "Verbinden...", false);
    wifi_connect(s_sel_ssid, pwd);
    s_connect_timer = lv_timer_create(connect_poll_cb, 500, NULL);
    lv_timer_set_repeat_count(s_connect_timer, -1);
}

/* ================================================================
   Handmatige invoer (fallback voor stap 2)
   ================================================================ */

static void ta_clicked_cb(lv_event_t *e) {
    show_keyboard(lv_event_get_target(e));
}

static void manual_save_cb(lv_event_t *e) {
    if (!s_url_ta || !s_token_ta) return;
    const char *url   = lv_textarea_get_text(s_url_ta);
    const char *token = lv_textarea_get_text(s_token_ta);
    hide_keyboard();

    if (!url || strlen(url) < 7) {
        set_status(s_status2_lbl, "Voer een geldig HA-adres in.", true);
        return;
    }
    if (!token || strlen(token) < 10) {
        set_status(s_status2_lbl, "Token is te kort.", true);
        return;
    }

    web_config_stop();
    if (s_webconf_timer) { lv_timer_del(s_webconf_timer); s_webconf_timer = NULL; }

    const char *pwd = s_pwd_ta ? lv_textarea_get_text(s_pwd_ta) : "";
    storage_set_string("wifi_ssid", s_sel_ssid);
    storage_set_string("wifi_pass", pwd ? pwd : "");
    storage_set_string("ha_url",   url);
    storage_set_string("ha_token", token);
    storage_set_u8("configured", 1);
    show_step(3);
}

static void manual_btn_cb(lv_event_t *e) {
    /* Toon of verberg het handmatige invoerpaneel */
    if (!s_manual_panel) return;
    if (lv_obj_has_flag(s_manual_panel, LV_OBJ_FLAG_HIDDEN)) {
        lv_obj_clear_flag(s_manual_panel, LV_OBJ_FLAG_HIDDEN);
        lv_obj_t *btn = lv_event_get_target(e);
        lv_obj_t *lbl = lv_obj_get_child(btn, 0);
        if (lbl) lv_label_set_text(lbl, "Verberg handmatige invoer");
    } else {
        lv_obj_add_flag(s_manual_panel, LV_OBJ_FLAG_HIDDEN);
        lv_obj_t *btn = lv_event_get_target(e);
        lv_obj_t *lbl = lv_obj_get_child(btn, 0);
        if (lbl) lv_label_set_text(lbl, "Handmatig invullen");
    }
}

static void kb_ready_cb(lv_event_t *e) { hide_keyboard(); }

/* ================================================================
   Stap 3 done-knop
   ================================================================ */

static void done_btn_cb(lv_event_t *e) { esp_restart(); }

/* ================================================================
   Schermopbouw
   ================================================================ */

static void build_step1(lv_obj_t *parent) {
    s_step1 = make_step_container(parent);

    lv_obj_t *title_bar = make_titlebar(s_step1, "Stap 1 van 3: WiFi-netwerk");

    lv_obj_t *scan_btn = lv_btn_create(title_bar);
    lv_obj_set_size(scan_btn, 80, 36);
    lv_obj_align(scan_btn, LV_ALIGN_RIGHT_MID, -8, 0);
    lv_obj_set_style_bg_color(scan_btn, lv_color_hex(CLR_SEP), 0);
    lv_obj_t *scan_lbl = lv_label_create(scan_btn);
    lv_label_set_text(scan_lbl, LV_SYMBOL_REFRESH " Scan");
    lv_obj_set_style_text_color(scan_lbl, lv_color_hex(CLR_TEXT), 0);
    lv_obj_center(scan_lbl);
    lv_obj_add_event_cb(scan_btn, scan_btn_cb, LV_EVENT_CLICKED, NULL);

    s_wifi_list = lv_list_create(s_step1);
    lv_obj_set_size(s_wifi_list, LV_HOR_RES - 16, 160);
    lv_obj_set_style_bg_color(s_wifi_list, lv_color_hex(CLR_PANEL), 0);
    lv_obj_set_style_border_color(s_wifi_list, lv_color_hex(CLR_SEP), 0);
    lv_list_add_text(s_wifi_list, "Druk op Scan om netwerken te zoeken");

    s_ssid_lbl = make_label(s_step1, "Geselecteerd: \xe2\x80\x94", CLR_SUBTEXT);

    make_label(s_step1, "Wachtwoord:", CLR_TEXT);
    s_pwd_ta = lv_textarea_create(s_step1);
    lv_obj_set_width(s_pwd_ta, LV_HOR_RES - 32);
    lv_obj_set_height(s_pwd_ta, 48);
    lv_textarea_set_placeholder_text(s_pwd_ta, "WiFi-wachtwoord");
    lv_textarea_set_one_line(s_pwd_ta, true);
    lv_obj_set_style_bg_color(s_pwd_ta, lv_color_hex(CLR_PANEL), 0);
    lv_obj_set_style_text_color(s_pwd_ta, lv_color_hex(CLR_TEXT), 0);
    lv_textarea_set_password_mode(s_pwd_ta, true);
    lv_obj_add_event_cb(s_pwd_ta, ta_clicked_cb, LV_EVENT_CLICKED, NULL);

    s_conn_btn = make_button(s_step1, "Verbinden", connect_btn_cb);

    s_status1_lbl = make_label(s_step1, "", CLR_SUBTEXT);
    lv_label_set_long_mode(s_status1_lbl, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s_status1_lbl, LV_HOR_RES - 32);

    /* Auto-scan */
    lv_obj_clean(s_wifi_list);
    lv_list_add_text(s_wifi_list, "Scannen...");
    wifi_scan_start();
    s_scan_timer = lv_timer_create(scan_poll_cb, 500, NULL);
    lv_timer_set_repeat_count(s_scan_timer, -1);
}

static void build_step2(lv_obj_t *parent) {
    s_step2 = make_step_container(parent);

    make_titlebar(s_step2, "Stap 2 van 3: Home Assistant");

    /* QR-code */
    s_qr = lv_qrcode_create(s_step2, 160,
                             lv_color_hex(0x000000),
                             lv_color_hex(CLR_ACCENT));
    lv_qrcode_update(s_qr, "http://...", 10);

    make_label(s_step2, "Open in je browser:", CLR_SUBTEXT);

    s_url_lbl = lv_label_create(s_step2);
    lv_label_set_text(s_url_lbl, "Verbinden...");
    lv_obj_set_style_text_color(s_url_lbl, lv_color_hex(CLR_ACCENT), 0);
    lv_obj_set_style_text_font(s_url_lbl, &lv_font_montserrat_16, 0);

    s_status2_lbl = make_label(s_step2, "Wachten op WiFi...", CLR_SUBTEXT);
    lv_label_set_long_mode(s_status2_lbl, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s_status2_lbl, LV_HOR_RES - 32);

    /* Handmatig-invullen knop */
    make_button(s_step2, "Handmatig invullen", manual_btn_cb);

    /* Verborgen handmatig invoerpaneel */
    s_manual_panel = lv_obj_create(s_step2);
    lv_obj_set_width(s_manual_panel, LV_HOR_RES);
    lv_obj_set_height(s_manual_panel, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(s_manual_panel, lv_color_hex(CLR_BG), 0);
    lv_obj_set_style_border_width(s_manual_panel, 0, 0);
    lv_obj_set_style_pad_all(s_manual_panel, 0, 0);
    lv_obj_set_flex_flow(s_manual_panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_manual_panel, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(s_manual_panel, LV_OBJ_FLAG_HIDDEN);

    make_label(s_manual_panel, "HA Adres:", CLR_TEXT);
    s_url_ta = lv_textarea_create(s_manual_panel);
    lv_obj_set_width(s_url_ta, LV_HOR_RES - 32);
    lv_obj_set_height(s_url_ta, 48);
    lv_textarea_set_placeholder_text(s_url_ta, "http://192.168.1.10:8123");
    lv_textarea_set_one_line(s_url_ta, true);
    lv_obj_set_style_bg_color(s_url_ta, lv_color_hex(CLR_PANEL), 0);
    lv_obj_set_style_text_color(s_url_ta, lv_color_hex(CLR_TEXT), 0);
    lv_obj_add_event_cb(s_url_ta, ta_clicked_cb, LV_EVENT_CLICKED, NULL);

    make_label(s_manual_panel, "Access Token:", CLR_TEXT);
    s_token_ta = lv_textarea_create(s_manual_panel);
    lv_obj_set_width(s_token_ta, LV_HOR_RES - 32);
    lv_obj_set_height(s_token_ta, 48);
    lv_textarea_set_placeholder_text(s_token_ta, "eyJhbGci...");
    lv_textarea_set_one_line(s_token_ta, true);
    lv_obj_set_style_bg_color(s_token_ta, lv_color_hex(CLR_PANEL), 0);
    lv_obj_set_style_text_color(s_token_ta, lv_color_hex(CLR_TEXT), 0);
    lv_obj_add_event_cb(s_token_ta, ta_clicked_cb, LV_EVENT_CLICKED, NULL);

    make_button(s_manual_panel, "Opslaan", manual_save_cb);

    lv_obj_add_flag(s_step2, LV_OBJ_FLAG_HIDDEN);
}

static void build_step3(lv_obj_t *parent) {
    s_step3 = make_step_container(parent);
    lv_obj_set_flex_align(s_step3, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *icon = make_label(s_step3, LV_SYMBOL_OK, CLR_ACCENT);
    lv_obj_set_style_text_font(icon, &lv_font_montserrat_48, 0);

    lv_obj_t *h = make_label(s_step3, "Configuratie opgeslagen!", CLR_TEXT);
    lv_obj_set_style_text_font(h, &lv_font_montserrat_16, 0);

    lv_obj_t *sub = make_label(s_step3,
        "Het apparaat herstart nu en maakt verbinding\nmet Home Assistant.",
        CLR_SUBTEXT);
    lv_label_set_long_mode(sub, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(sub, LV_HOR_RES - 32);
    lv_obj_set_style_text_align(sub, LV_TEXT_ALIGN_CENTER, 0);

    make_button(s_step3, "Doorgaan (herstart)", done_btn_cb);

    lv_obj_add_flag(s_step3, LV_OBJ_FLAG_HIDDEN);
}

/* ================================================================
   Publieke API
   ================================================================ */

lv_obj_t *ui_setup_create(void) {
    app_state_set(STATE_SETUP_WIZARD);

    /* Reset statische timers van vorige sessie */
    s_scan_timer    = NULL;
    s_connect_timer = NULL;
    s_webconf_timer = NULL;

    s_can_exit = storage_get_u8("configured", 0) != 0;

    s_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_screen, lv_color_hex(CLR_BG), 0);

    s_keyboard = lv_keyboard_create(s_screen);
    lv_obj_set_size(s_keyboard, LV_HOR_RES, 200);
    lv_obj_align(s_keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(s_keyboard, lv_color_hex(CLR_PANEL), 0);
    lv_obj_add_flag(s_keyboard, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(s_keyboard, kb_ready_cb, LV_EVENT_READY,  NULL);
    lv_obj_add_event_cb(s_keyboard, kb_ready_cb, LV_EVENT_CANCEL, NULL);

    build_step1(s_screen);
    build_step2(s_screen);
    build_step3(s_screen);

    lv_obj_move_foreground(s_keyboard);
    return s_screen;
}
