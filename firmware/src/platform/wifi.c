#include "wifi.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "wifi";

#define SCAN_DONE_BIT    BIT0
#define CONNECTED_BIT    BIT1
#define CONNECT_DONE_BIT BIT2

static EventGroupHandle_t s_eg        = NULL;
static esp_netif_t       *s_sta_netif = NULL;
static bool               s_init_done = false;

static void event_handler(void *arg, esp_event_base_t base,
                           int32_t id, void *event_data) {
    if (base == WIFI_EVENT) {
        switch ((wifi_event_t)id) {
            case WIFI_EVENT_STA_START:
                ESP_LOGI(TAG, "STA gestart");
                break;
            case WIFI_EVENT_SCAN_DONE:
                xEventGroupSetBits(s_eg, SCAN_DONE_BIT);
                ESP_LOGI(TAG, "WiFi scan klaar");
                break;
            case WIFI_EVENT_STA_CONNECTED:
                ESP_LOGI(TAG, "Verbonden met AP");
                break;
            case WIFI_EVENT_STA_DISCONNECTED: {
                wifi_event_sta_disconnected_t *ev =
                    (wifi_event_sta_disconnected_t *)event_data;
                ESP_LOGW(TAG, "WiFi verbroken (reden %d)", ev->reason);
                xEventGroupClearBits(s_eg, CONNECTED_BIT);
                xEventGroupSetBits(s_eg, CONNECT_DONE_BIT);
                break;
            }
            default:
                break;
        }
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *ev = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "IP: " IPSTR, IP2STR(&ev->ip_info.ip));
        xEventGroupSetBits(s_eg, CONNECTED_BIT | CONNECT_DONE_BIT);
    }
}

void wifi_init(void) {
    if (s_init_done) return;

    s_eg = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    s_sta_netif = esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                        event_handler, NULL, NULL);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                        event_handler, NULL, NULL);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    s_init_done = true;
    ESP_LOGI(TAG, "WiFi geïnitialiseerd");
}

void wifi_scan_start(void) {
    xEventGroupClearBits(s_eg, SCAN_DONE_BIT);
    wifi_scan_config_t scan_cfg = {
        .ssid        = NULL,
        .bssid       = NULL,
        .channel     = 0,
        .show_hidden = false,
    };
    esp_wifi_scan_start(&scan_cfg, false);
}

bool wifi_scan_done(void) {
    return (xEventGroupGetBits(s_eg) & SCAN_DONE_BIT) != 0;
}

int wifi_scan_get_results(wifi_ap_t *out, int max) {
    uint16_t ap_count = 0;
    esp_wifi_scan_get_ap_num(&ap_count);
    if (ap_count == 0) return 0;
    if ((int)ap_count > max) ap_count = (uint16_t)max;

    wifi_ap_record_t *records =
        (wifi_ap_record_t *)malloc(ap_count * sizeof(wifi_ap_record_t));
    if (!records) return 0;

    esp_wifi_scan_get_ap_records(&ap_count, records);
    for (int i = 0; i < (int)ap_count; i++) {
        memcpy(out[i].ssid, records[i].ssid, 32);
        out[i].ssid[32] = '\0';
        out[i].rssi     = records[i].rssi;
    }
    free(records);
    return (int)ap_count;
}

void wifi_connect(const char *ssid, const char *password) {
    xEventGroupClearBits(s_eg, CONNECTED_BIT | CONNECT_DONE_BIT);
    esp_wifi_disconnect();

    wifi_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    strncpy((char *)cfg.sta.ssid,     ssid,     sizeof(cfg.sta.ssid) - 1);
    strncpy((char *)cfg.sta.password, password, sizeof(cfg.sta.password) - 1);
    cfg.sta.threshold.authmode =
        (strlen(password) == 0) ? WIFI_AUTH_OPEN : WIFI_AUTH_WPA2_PSK;

    esp_wifi_set_config(WIFI_IF_STA, &cfg);
    esp_wifi_connect();
    ESP_LOGI(TAG, "Verbinden met '%s'", ssid);
}

bool wifi_connect_done(void) {
    return (xEventGroupGetBits(s_eg) & CONNECT_DONE_BIT) != 0;
}

bool wifi_is_connected(void) {
    return (xEventGroupGetBits(s_eg) & CONNECTED_BIT) != 0;
}

void wifi_get_ip(char *out, size_t len) {
    esp_netif_ip_info_t info;
    if (s_sta_netif &&
        esp_netif_get_ip_info(s_sta_netif, &info) == ESP_OK &&
        info.ip.addr != 0) {
        snprintf(out, len, IPSTR, IP2STR(&info.ip));
    } else {
        strncpy(out, "0.0.0.0", len);
    }
}

void wifi_wait_connected(void) {
    xEventGroupWaitBits(s_eg, CONNECTED_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
}
