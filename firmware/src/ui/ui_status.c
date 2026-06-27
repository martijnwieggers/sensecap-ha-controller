#include "ui_status.h"
#include "../platform/storage.h"
#include "../app/app_state.h"
#include "lvgl.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include <stdio.h>

static lv_obj_t *s_lbl_wifi   = NULL;
static lv_obj_t *s_lbl_ip     = NULL;
static lv_obj_t *s_lbl_ha     = NULL;
static lv_obj_t *s_lbl_uptime = NULL;

static void refresh_cb(lv_timer_t *timer) {
    char buf[64];

    /* WiFi RSSI */
    wifi_ap_record_t ap;
    if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) {
        snprintf(buf, sizeof(buf), "WiFi: %s (%d dBm)", ap.ssid, ap.rssi);
    } else {
        snprintf(buf, sizeof(buf), "WiFi: niet verbonden");
    }
    lv_label_set_text(s_lbl_wifi, buf);

    /* Uptime */
    uint64_t us = esp_timer_get_time();
    uint32_t s_total = (uint32_t)(us / 1000000ULL);
    uint32_t d = s_total / 86400, h = (s_total % 86400) / 3600;
    uint32_t m = (s_total % 3600) / 60, s2 = s_total % 60;
    snprintf(buf, sizeof(buf), "Uptime: %ud %uh %um %us", d, h, m, s2);
    lv_label_set_text(s_lbl_uptime, buf);

    /* HA status */
    const char *ha_status = app_state_is(STATE_VIEW_READY) ?
                            "HA: Verbonden" : "HA: Verbroken";
    lv_obj_set_style_text_color(s_lbl_ha,
        app_state_is(STATE_VIEW_READY) ?
        lv_color_hex(0x4FC3F7) : lv_color_hex(0xEF5350), 0);
    lv_label_set_text(s_lbl_ha, ha_status);
}

lv_obj_t *ui_status_create(void) {
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x1A1A2E), 0);

    int y = 20;
    lv_obj_t *title = lv_label_create(screen);
    lv_label_set_text(title, "Status");
    lv_obj_set_style_text_color(title, lv_color_hex(0xE0E0E0), 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 16, y); y += 40;

    /* Versie */
    lv_obj_t *lbl_ver = lv_label_create(screen);
    lv_label_set_text(lbl_ver, "Versie: " APP_VERSION);
    lv_obj_set_style_text_color(lbl_ver, lv_color_hex(0x9E9E9E), 0);
    lv_obj_align(lbl_ver, LV_ALIGN_TOP_LEFT, 16, y); y += 36;

    s_lbl_wifi = lv_label_create(screen);
    lv_obj_set_style_text_color(s_lbl_wifi, lv_color_hex(0xE0E0E0), 0);
    lv_obj_align(s_lbl_wifi, LV_ALIGN_TOP_LEFT, 16, y); y += 36;

    s_lbl_ip = lv_label_create(screen);
    lv_obj_set_style_text_color(s_lbl_ip, lv_color_hex(0xE0E0E0), 0);
    lv_obj_align(s_lbl_ip, LV_ALIGN_TOP_LEFT, 16, y); y += 36;

    s_lbl_ha = lv_label_create(screen);
    lv_obj_align(s_lbl_ha, LV_ALIGN_TOP_LEFT, 16, y); y += 36;

    s_lbl_uptime = lv_label_create(screen);
    lv_obj_set_style_text_color(s_lbl_uptime, lv_color_hex(0x9E9E9E), 0);
    lv_obj_align(s_lbl_uptime, LV_ALIGN_TOP_LEFT, 16, y);

    refresh_cb(NULL);
    lv_timer_create(refresh_cb, 5000, NULL);
    return screen;
}
