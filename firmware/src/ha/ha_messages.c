#include "ha_messages.h"
#include "../app/app_events.h"
#include "../platform/storage.h"
#include "ArduinoJson.h"
#include "esp_log.h"
#include <string.h>

extern QueueHandle_t ha_event_queue;
static const char *TAG = "ha_messages";

static void send_json(esp_websocket_client_handle_t client, const char *json) {
    esp_websocket_client_send_text(client, json, strlen(json), pdMS_TO_TICKS(2000));
}

void ha_messages_send_auth(esp_websocket_client_handle_t client,
                           const char *token) {
    char buf[512];
    snprintf(buf, sizeof(buf),
             "{\"type\":\"auth\",\"access_token\":\"%s\"}", token);
    send_json(client, buf);
}

void ha_messages_subscribe_events(esp_websocket_client_handle_t client,
                                  int *msg_id) {
    char buf[128];
    snprintf(buf, sizeof(buf),
             "{\"id\":%d,\"type\":\"subscribe_events\","
             "\"event_type\":\"state_changed\"}", (*msg_id)++);
    send_json(client, buf);
}

void ha_messages_get_states(esp_websocket_client_handle_t client,
                            int *msg_id) {
    char buf[64];
    snprintf(buf, sizeof(buf), "{\"id\":%d,\"type\":\"get_states\"}", (*msg_id)++);
    send_json(client, buf);
}

void ha_messages_call_service(esp_websocket_client_handle_t client,
                              int *msg_id,
                              const char *entity_id,
                              const char *service,
                              int turn_on,
                              float brightness_pct,
                              float temperature) {
    char buf[256];
    char domain[32];

    /* Bepaal domein uit entity_id (voor het punt) */
    const char *dot = strchr(entity_id, '.');
    size_t dlen = dot ? (size_t)(dot - entity_id) : sizeof(domain) - 1;
    strncpy(domain, entity_id, dlen);
    domain[dlen] = '\0';

    if (temperature >= 0) {
        snprintf(buf, sizeof(buf),
                 "{\"id\":%d,\"type\":\"call_service\",\"domain\":\"%s\","
                 "\"service\":\"%s\",\"target\":{\"entity_id\":\"%s\"},"
                 "\"service_data\":{\"temperature\":%.1f}}",
                 (*msg_id)++, domain, service, entity_id, temperature);
    } else if (brightness_pct >= 0) {
        snprintf(buf, sizeof(buf),
                 "{\"id\":%d,\"type\":\"call_service\",\"domain\":\"%s\","
                 "\"service\":\"%s\",\"target\":{\"entity_id\":\"%s\"},"
                 "\"service_data\":{\"brightness_pct\":%.0f}}",
                 (*msg_id)++, domain, service, entity_id, brightness_pct);
    } else {
        snprintf(buf, sizeof(buf),
                 "{\"id\":%d,\"type\":\"call_service\",\"domain\":\"%s\","
                 "\"service\":\"%s\",\"target\":{\"entity_id\":\"%s\"}}",
                 (*msg_id)++, domain, service, entity_id);
    }
    send_json(client, buf);
}

void ha_messages_handle(esp_websocket_client_handle_t client,
                        const char *data, int len, int *msg_id) {
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, data, len);
    if (err) {
        ESP_LOGW(TAG, "JSON parse fout: %s", err.c_str());
        return;
    }

    const char *type = doc["type"];
    if (!type) return;

    if (strcmp(type, "auth_required") == 0) {
        char token[256];
        storage_get_string("ha_token", token, sizeof(token), "");
        ha_messages_send_auth(client, token);

    } else if (strcmp(type, "auth_ok") == 0) {
        ESP_LOGI(TAG, "HA authenticatie geslaagd");
        ha_messages_get_states(client, msg_id);
        ha_messages_subscribe_events(client, msg_id);
        ha_event_t evt = {.type = HA_EVT_CONNECTED};
        xQueueSend(ha_event_queue, &evt, 0);

    } else if (strcmp(type, "auth_invalid") == 0) {
        ESP_LOGE(TAG, "HA authenticatie mislukt — controleer token");

    } else if (strcmp(type, "event") == 0) {
        const char *entity_id = doc["event"]["data"]["entity_id"];
        const char *new_state = doc["event"]["data"]["new_state"]["state"];
        if (!entity_id || !new_state) return;

        ha_event_t evt = {.type = HA_EVT_STATE_CHANGED,
                          .brightness_pct = -1, .temperature = -1};
        strncpy(evt.entity_id, entity_id, sizeof(evt.entity_id) - 1);
        strncpy(evt.state,     new_state, sizeof(evt.state) - 1);

        JsonVariant attrs = doc["event"]["data"]["new_state"]["attributes"];
        if (attrs.containsKey("brightness")) {
            evt.brightness_pct = attrs["brightness"].as<float>() / 2.55f;
        }
        if (attrs.containsKey("temperature")) {
            evt.temperature = attrs["temperature"].as<float>();
        }
        xQueueSend(ha_event_queue, &evt, 0);
    }
}
