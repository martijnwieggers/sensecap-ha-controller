#include "ha_lovelace.h"
#include "../app/app_events.h"
#include "ArduinoJson.h"
#include "esp_log.h"
#include <string.h>

extern QueueHandle_t ha_event_queue;
static const char *TAG = "ha_lovelace";

static void send_json(esp_websocket_client_handle_t client, const char *json) {
    esp_websocket_client_send_text(client, json, strlen(json), pdMS_TO_TICKS(2000));
}

void ha_lovelace_request_all(esp_websocket_client_handle_t client, int *msg_id) {
    char buf[80];
    snprintf(buf, sizeof(buf),
             "{\"id\":%d,\"type\":\"lovelace/config\"}", (*msg_id)++);
    send_json(client, buf);
}

void ha_lovelace_request(esp_websocket_client_handle_t client,
                         int *msg_id, const char *view_path) {
    char buf[128];
    snprintf(buf, sizeof(buf),
             "{\"id\":%d,\"type\":\"lovelace/config\","
             "\"url_path\":\"%s\"}", (*msg_id)++, view_path);
    send_json(client, buf);
}

void ha_lovelace_handle_result(const char *json, int len) {
    /* TODO: Implementatie — parseer views[] uit het JSON-resultaat
       en stuur HA_EVT_VIEWS_LOADED of HA_EVT_ENTITIES_LOADED naar de UI-taak. */
    ESP_LOGI(TAG, "Lovelace config ontvangen (%d bytes)", len);

    ha_event_t evt = {.type = HA_EVT_VIEWS_LOADED};
    xQueueSend(ha_event_queue, &evt, 0);
}
