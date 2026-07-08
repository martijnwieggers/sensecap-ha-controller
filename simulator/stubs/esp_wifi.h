#pragma once
#include "esp_err.h"
#include <stdint.h>
#include <string.h>

typedef struct {
    uint8_t ssid[33];
    uint8_t bssid[6];
    uint8_t channel;
    int8_t  rssi;
    uint8_t authmode;
} wifi_ap_record_t;

/* Simulator: doe alsof we verbonden zijn met een vast netwerk,
   zodat de statuspagina en het WiFi/HA-onderscheid testbaar zijn. */
static inline esp_err_t esp_wifi_sta_get_ap_info(wifi_ap_record_t *ap) {
    memset(ap, 0, sizeof(*ap));
    strcpy((char *)ap->ssid, "Buurman_Wifi");
    ap->rssi = -58;
    return ESP_OK;
}
