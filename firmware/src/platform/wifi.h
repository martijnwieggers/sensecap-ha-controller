#pragma once
#include <stdbool.h>
#include <stddef.h>

#define WIFI_SCAN_MAX 20

typedef struct {
    char ssid[33];
    int  rssi;
} wifi_ap_t;

void wifi_init(void);
void wifi_scan_start(void);
bool wifi_scan_done(void);
int  wifi_scan_get_results(wifi_ap_t *out, int max);
void wifi_connect(const char *ssid, const char *password);
bool wifi_connect_done(void);
bool wifi_is_connected(void);
void wifi_get_ip(char *out, size_t len);
void wifi_wait_connected(void);

/* Lage-latentiemodus: zet de modem-slaapstand uit (WIFI_PS_NONE) zodat
   HA-commando's direct beantwoord worden. Gekoppeld aan de backlight:
   scherm aan = responsief, scherm uit = radio mag slapen (energie). */
void wifi_set_low_latency(bool on);
