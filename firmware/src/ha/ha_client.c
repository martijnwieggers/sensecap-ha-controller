#include "ha_client.h"
#include "ha_messages.h"
#include "ha_lovelace.h"
#include "../app/app_state.h"
#include "../app/app_events.h"
#include "../platform/storage.h"
#include "../platform/wifi.h"
#include "esp_websocket_client.h"
#include "esp_crt_bundle.h"
#include "esp_netif_sntp.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "ha_client";

static const uint32_t BACKOFF_MS[] = {5000, 10000, 20000, 40000, 60000};
#define BACKOFF_COUNT (sizeof(BACKOFF_MS) / sizeof(BACKOFF_MS[0]))

static esp_websocket_client_handle_t s_client = NULL;
static int s_msg_id = 1;

/* Verzamelbuffer voor gefragmenteerde frames: grote HA-antwoorden
   (lovelace-config, get_states) komen binnen in chunks van ~1 KB */
static char  *s_rx_buf = NULL;
static size_t s_rx_cap = 0;

static void websocket_event_handler(void *arg,
                                    esp_event_base_t base,
                                    int32_t event_id,
                                    void *event_data) {
    esp_websocket_event_data_t *data = (esp_websocket_event_data_t *)event_data;

    switch (event_id) {
        case WEBSOCKET_EVENT_CONNECTED:
            ESP_LOGI(TAG, "WebSocket verbonden");
            app_state_set(STATE_HA_AUTH);
            break;

        case WEBSOCKET_EVENT_DATA: {
            /* Alleen tekstframes (0x1); ping/pong/close-frames overslaan */
            if (data->op_code != 0x01 && data->op_code != 0x00) break;
            if (data->payload_len <= 0 || data->data_len <= 0) break;

            if (data->payload_offset == 0 &&
                data->data_len == data->payload_len) {
                /* Compleet in één chunk: direct parsen */
                ha_messages_handle(s_client, data->data_ptr,
                                   data->data_len, &s_msg_id);
                break;
            }

            /* Gefragmenteerd: chunks samenvoegen tot het frame compleet is */
            if (s_rx_cap < (size_t)data->payload_len + 1) {
                char *nb = heap_caps_realloc(s_rx_buf, data->payload_len + 1,
                                             MALLOC_CAP_SPIRAM);
                if (!nb) {
                    ESP_LOGE(TAG, "Geen PSRAM voor RX-buffer (%d B)",
                             data->payload_len);
                    break;
                }
                s_rx_buf = nb;
                s_rx_cap = data->payload_len + 1;
            }
            memcpy(s_rx_buf + data->payload_offset, data->data_ptr,
                   data->data_len);
            if (data->payload_offset + data->data_len >=
                data->payload_len) {
                s_rx_buf[data->payload_len] = '\0';
                ha_messages_handle(s_client, s_rx_buf,
                                   data->payload_len, &s_msg_id);
            }
            break;
        }

        case WEBSOCKET_EVENT_DISCONNECTED:
        case WEBSOCKET_EVENT_ERROR: {
            ESP_LOGW(TAG, "WebSocket verbroken");
            ha_event_t evt = {.type = HA_EVT_DISCONNECTED};
            extern QueueHandle_t ha_event_queue;
            xQueueSend(ha_event_queue, &evt, 0);
            break;
        }

        default:
            break;
    }
}

static void build_ws_url(const char *ha_url, char *url, size_t url_len) {
    const char *host;
    const char *scheme;

    if (strncmp(ha_url, "https://", 8) == 0) {
        scheme = "wss://";
        host   = ha_url + 8;
    } else if (strncmp(ha_url, "http://", 7) == 0) {
        scheme = "ws://";
        host   = ha_url + 7;
    } else {
        scheme = "ws://";
        host   = ha_url;
    }

    char host_buf[96];
    strncpy(host_buf, host, sizeof(host_buf) - 1);
    host_buf[sizeof(host_buf) - 1] = '\0';
    size_t hlen = strlen(host_buf);
    if (hlen > 0 && host_buf[hlen - 1] == '/') {
        host_buf[hlen - 1] = '\0';
    }

    snprintf(url, url_len, "%s%s/api/websocket", scheme, host_buf);
}

static void connect_once(const char *url) {
    esp_websocket_client_config_t cfg = {
        .uri                    = url,
        /* Herverbinden regelt ha_client_run zelf (backoff-lus) */
        .disable_auto_reconnect = true,
        .network_timeout_ms     = 10000,
        /* JSON-parsen (recursie tot nesting 20) draait op deze taak;
           de standaard 4 KB stack loopt daarbij over */
        .task_stack             = 12288,
        /* CA-bundel voor wss:// (https-URL's, bv. DuckDNS + Let's Encrypt);
           bij ws:// wordt dit genegeerd */
        .crt_bundle_attach      = esp_crt_bundle_attach,
    };
    s_client = esp_websocket_client_init(&cfg);
    esp_websocket_register_events(s_client, WEBSOCKET_EVENT_ANY,
                                  websocket_event_handler, NULL);
    esp_websocket_client_start(s_client);
}

void ha_client_run(void) {
    uint8_t configured = storage_get_u8("configured", 0);

    if (!configured) {
        /* Setup-wizard loopt in de UI-taak. Ha_client wacht totdat
           de wizard klaar is en het apparaat herstart. */
        vTaskDelay(portMAX_DELAY);
        return;
    }

    /* WiFi auto-verbinden met opgeslagen inloggegevens */
    char ssid[33] = {0};
    char pass[65] = {0};
    storage_get_string("wifi_ssid", ssid, sizeof(ssid), "");
    storage_get_string("wifi_pass", pass, sizeof(pass), "");

    if (ssid[0]) {
        app_state_set(STATE_WIFI_CONNECTING);
        wifi_connect(ssid, pass);
        wifi_wait_connected();   /* blokkeert tot IP verkregen */
        ESP_LOGI(TAG, "WiFi verbonden");
    } else {
        ESP_LOGE(TAG, "Geen WiFi-gegevens opgeslagen");
        vTaskDelay(portMAX_DELAY);
        return;
    }

    /* Kloksync via SNTP — zonder juiste tijd keurt mbedTLS elk certificaat
       af ("not yet valid", klok staat na boot op 1970) en faalt elke wss:// */
    esp_sntp_config_t sntp_cfg = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
    esp_netif_sntp_init(&sntp_cfg);
    if (esp_netif_sntp_sync_wait(pdMS_TO_TICKS(15000)) != ESP_OK) {
        ESP_LOGW(TAG, "SNTP-tijdsync niet gelukt — TLS-verbindingen kunnen falen");
    } else {
        ESP_LOGI(TAG, "Systeemtijd gesynchroniseerd");
    }

    /* Bouw WebSocket-URL */
    char ha_url[120] = {0};
    char url[128]    = {0};
    storage_get_string("ha_url", ha_url, sizeof(ha_url), "");
    build_ws_url(ha_url, url, sizeof(url));
    ESP_LOGI(TAG, "HA URL: %s", url);

    /* Reconnect-lus met exponential backoff. Een staande verbinding blijft
       staan; pas na een échte verbreking wordt opgeruimd en opnieuw
       geprobeerd. */
    int backoff_idx = 0;
    while (1) {
        app_state_set(STATE_HA_CONNECTING);
        ESP_LOGI(TAG, "Verbinden met %s", url);
        s_msg_id = 1;
        connect_once(url);

        /* Wacht tot de verbinding tot stand komt (max 15 s) */
        for (int i = 0; i < 150 && s_client &&
                        !esp_websocket_client_is_connected(s_client); i++) {
            vTaskDelay(pdMS_TO_TICKS(100));
        }

        if (s_client && esp_websocket_client_is_connected(s_client)) {
            backoff_idx = 0;
            while (esp_websocket_client_is_connected(s_client)) {
                vTaskDelay(pdMS_TO_TICKS(1000));
            }
            ESP_LOGW(TAG, "Verbinding verbroken — opnieuw proberen");
        }

        if (s_client) {
            esp_websocket_client_stop(s_client);
            esp_websocket_client_destroy(s_client);
            s_client = NULL;
        }

        vTaskDelay(pdMS_TO_TICKS(BACKOFF_MS[backoff_idx]));
        if (backoff_idx < (int)BACKOFF_COUNT - 1) backoff_idx++;
    }
}

void ha_client_toggle(const char *entity_id) {
    if (!s_client) return;
    ha_messages_call_service(s_client, &s_msg_id, entity_id, "toggle",
                             0, -1, -1);
}

void ha_client_set_brightness(const char *entity_id, float pct) {
    if (!s_client) return;
    ha_messages_call_service(s_client, &s_msg_id, entity_id, "turn_on",
                             0, pct, -1);
}

void ha_client_set_temperature(const char *entity_id, float temp) {
    if (!s_client) return;
    ha_messages_call_service(s_client, &s_msg_id, entity_id,
                             "set_temperature", 0, -1, temp);
}

void ha_client_set_hvac_mode(const char *entity_id, const char *mode) {
    if (!s_client) return;
    ha_messages_call_service_str(s_client, &s_msg_id, entity_id,
                                 "set_hvac_mode", "hvac_mode", mode);
}

void ha_client_set_fan_mode(const char *entity_id, const char *mode) {
    if (!s_client) return;
    ha_messages_call_service_str(s_client, &s_msg_id, entity_id,
                                 "set_fan_mode", "fan_mode", mode);
}

void ha_client_press_button(const char *entity_id) {
    if (!s_client) return;
    /* scene en script gebruiken turn_on; button-domein gebruikt press */
    const char *service = "press";
    if (strncmp(entity_id, "scene.",  6) == 0 ||
        strncmp(entity_id, "script.", 7) == 0) {
        service = "turn_on";
    }
    ha_messages_call_service(s_client, &s_msg_id, entity_id, service, 0, -1, -1);
}

void ha_client_load_view(const char *view_path) {
    if (!s_client) return;
    ha_lovelace_request(s_client, &s_msg_id, view_path);
}

void ha_client_get_views(void) {
    if (!s_client) return;
    ha_lovelace_request_all(s_client, &s_msg_id);
}

void ha_client_get_states(void) {
    if (!s_client) return;
    ha_messages_get_states(s_client, &s_msg_id);
}
