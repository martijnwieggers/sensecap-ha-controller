#pragma once
#include "esp_err.h"
#include <stdint.h>

typedef struct {
    uint8_t ssid[33];
    uint8_t bssid[6];
    uint8_t channel;
    int8_t  rssi;
    uint8_t authmode;
} wifi_ap_record_t;

/* Simulator: altijd niet verbonden */
static inline esp_err_t esp_wifi_sta_get_ap_info(wifi_ap_record_t *ap) {
    (void)ap;
    return ESP_FAIL;
}
