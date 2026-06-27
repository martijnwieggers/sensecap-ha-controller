#include "ha_client.h"
#include "ha_messages.h"
#include "ha_lovelace.h"
#include "../app/app_state.h"
#include "../app/app_events.h"
#include "../platform/storage.h"
#include "esp_websocket_client.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "ha_client";

/* Exponential backoff vertragingen in ms */
static const uint32_t BACKOFF_MS[] = {5000, 10000, 20000, 40000, 60000};
#define BACKOFF_COUNT (sizeof(BACKOFF_MS) / sizeof(BACKOFF_MS[0]))

static esp_websocket_client_handle_t s_client = NULL;
static int s_msg_id = 1;

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

        case WEBSOCKET_EVENT_DATA:
            ha_messages_handle(s_client, data->data_ptr, data->data_len, &s_msg_id);
            break;

        case WEBSOCKET_EVENT_DISCONNECTED:
        case WEBSOCKET_EVENT_ERROR:
            ESP_LOGW(TAG, "WebSocket verbroken");
            {
                ha_event_t evt = {.type = HA_EVT_DISCONNECTED};
                extern QueueHandle_t ha_event_queue;
                xQueueSend(ha_event_queue, &evt, 0);
            }
            break;

        default:
            break;
    }
}

static void connect_once(const char *url) {
    esp_websocket_client_config_t cfg = {
        .uri               = url,
        .reconnect_timeout_ms = 0,  /* wij beheren reconnect zelf */
        .network_timeout_ms   = 10000,
    };
    s_client = esp_websocket_client_init(&cfg);
    esp_websocket_register_events(s_client, WEBSOCKET_EVENT_ANY,
                                  websocket_event_handler, NULL);
    esp_websocket_client_start(s_client);
}

void ha_client_run(void) {
    char url[128];
    char ha_url[120];
    storage_get_string("ha_url", ha_url, sizeof(ha_url), "");

    snprintf(url, sizeof(url), "%s/api/websocket",
             ha_url[strlen(ha_url) - 1] == '/' ?
             (ha_url[strlen(ha_url) - 1] = '\0', ha_url) : ha_url);

    int backoff_idx = 0;
    while (1) {
        app_state_set(STATE_HA_CONNECTING);
        ESP_LOGI(TAG, "Verbinden met %s", url);
        connect_once(url);

        /* Wacht tot verbinding verbroken is, dan exponential backoff */
        vTaskDelay(pdMS_TO_TICKS(BACKOFF_MS[backoff_idx]));
        if (backoff_idx < (int)BACKOFF_COUNT - 1) backoff_idx++;

        if (s_client) {
            esp_websocket_client_stop(s_client);
            esp_websocket_client_destroy(s_client);
            s_client = NULL;
        }
    }
}

void ha_client_toggle(const char *entity_id) {
    if (!s_client) return;
    ha_messages_call_service(s_client, &s_msg_id, entity_id, "toggle", 0, -1, -1);
}

void ha_client_set_brightness(const char *entity_id, float pct) {
    if (!s_client) return;
    ha_messages_call_service(s_client, &s_msg_id, entity_id, "turn_on", 0, pct, -1);
}

void ha_client_set_temperature(const char *entity_id, float temp) {
    if (!s_client) return;
    ha_messages_call_service(s_client, &s_msg_id, entity_id, "set_temperature", 0, -1, temp);
}

void ha_client_load_view(const char *view_path) {
    if (!s_client) return;
    ha_lovelace_request(s_client, &s_msg_id, view_path);
}

void ha_client_get_views(void) {
    if (!s_client) return;
    ha_lovelace_request_all(s_client, &s_msg_id);
}
