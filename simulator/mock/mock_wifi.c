#include "../../firmware/src/platform/wifi.h"
#include <SDL2/SDL.h>
#include <string.h>
#include <stdio.h>

/* ---- Nep WiFi-netwerken voor de simulator ---- */
static const wifi_ap_t FAKE_APS[] = {
    {"Thuisnetwerk",        -45},
    {"Buurman_Wifi",        -72},
    {"AndroidHotspot",      -83},
};
#define N_FAKE_APS  (int)(sizeof(FAKE_APS) / sizeof(FAKE_APS[0]))

/* ---- Scan staat ---- */
static bool     s_scan_started  = false;
static uint32_t s_scan_start_ms = 0;
#define SCAN_DELAY_MS 1200   /* simuleer 1.2 s scan-duur */

/* ---- Verbindings staat ---- */
static bool     s_connect_started = false;
static bool     s_connected       = false;
static uint32_t s_connect_start_ms = 0;
#define CONNECT_DELAY_MS 800  /* simuleer 0.8 s verbindings-duur */

/* ---- Publieke API (stubs) ---- */

void wifi_init(void) {
    printf("[wifi] wifi_init() — simulator mock\n");
}

void wifi_scan_start(void) {
    s_scan_started  = true;
    s_scan_start_ms = SDL_GetTicks();
    printf("[wifi] Scan gestart (simuleer %d ms)\n", SCAN_DELAY_MS);
}

bool wifi_scan_done(void) {
    if (!s_scan_started) return false;
    return (SDL_GetTicks() - s_scan_start_ms) >= SCAN_DELAY_MS;
}

int wifi_scan_get_results(wifi_ap_t *out, int max) {
    int n = N_FAKE_APS < max ? N_FAKE_APS : max;
    for (int i = 0; i < n; i++) {
        out[i] = FAKE_APS[i];
    }
    printf("[wifi] %d netwerk(en) gevonden\n", n);
    return n;
}

void wifi_connect(const char *ssid, const char *password) {
    s_connect_started  = true;
    s_connected        = false;
    s_connect_start_ms = SDL_GetTicks();
    printf("[wifi] Verbinden met '%s' (simuleer %d ms)\n", ssid, CONNECT_DELAY_MS);
    (void)password;
}

bool wifi_connect_done(void) {
    if (!s_connect_started) return false;
    if (s_connected) return true;
    if ((SDL_GetTicks() - s_connect_start_ms) >= CONNECT_DELAY_MS) {
        s_connected = true;
        printf("[wifi] Verbonden (mock)\n");
        return true;
    }
    return false;
}

bool wifi_is_connected(void) {
    /* Eenmalig controleren zodat de timer de connected-bit kan zetten */
    if (s_connect_started && !s_connected) {
        wifi_connect_done();
    }
    return s_connected;
}

void wifi_get_ip(char *out, size_t len) {
    strncpy(out, "192.168.1.42", len - 1);
    out[len - 1] = '\0';
}

void wifi_wait_connected(void) {
    /* In de simulator is er geen echte WiFi-taak; dit is een no-op */
}
