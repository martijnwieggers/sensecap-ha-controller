#include "ha_messages.h"
#include "ha_lovelace.h"
#include "../app/app_events.h"
#include "../app/app_entities.h"
#include "../platform/storage.h"
#include "ArduinoJson.h"
#include "esp_log.h"
#include <string.h>

extern "C" QueueHandle_t ha_event_queue;
static const char *TAG = "ha_messages";

/* Id van het lopende get_states-request, -1 als er geen loopt */
static int s_get_states_id = -1;

static void send_json(esp_websocket_client_handle_t client, const char *json) {
    esp_websocket_client_send_text(client, json, strlen(json), pdMS_TO_TICKS(2000));
}

void ha_messages_send_auth(esp_websocket_client_handle_t client,
                           const char *token) {
    /* Token kan tot 511 tekens zijn (web_config_result_t) + JSON-omhulsel */
    char buf[576];
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
    s_get_states_id = *msg_id;
    char buf[64];
    snprintf(buf, sizeof(buf), "{\"id\":%d,\"type\":\"get_states\"}", (*msg_id)++);
    send_json(client, buf);
}

/* Vul mode-lijst (hvac_modes / fan_modes) uit een JSON-array (US-011) */
static void copy_mode_list(JsonVariant arr, char dst[][MODE_STR_LEN], int *count) {
    *count = 0;
    for (JsonVariant v : arr.as<JsonArray>()) {
        if (*count >= MAX_MODES) break;
        const char *s = v.as<const char *>();
        if (!s) continue;
        strncpy(dst[*count], s, MODE_STR_LEN - 1);
        dst[*count][MODE_STR_LEN - 1] = '\0';
        (*count)++;
    }
}

/* Gedeelde attribuut-parsing voor state_changed-events en get_states */
static void parse_state_attrs(JsonVariant attrs, ha_event_t *evt) {
    const char *fname = attrs["friendly_name"].as<const char *>();
    if (fname) {
        strncpy(evt->friendly_name, fname, sizeof(evt->friendly_name) - 1);
    }
    if (attrs["brightness"].is<float>()) {
        evt->brightness_pct = attrs["brightness"].as<float>() / 2.55f;
    }
    /* Dimbaar = supported_color_modes bevat meer dan alleen "onoff" (US-014) */
    if (attrs["supported_color_modes"].is<JsonArray>()) {
        evt->dimmable = 2;
        for (JsonVariant m : attrs["supported_color_modes"].as<JsonArray>()) {
            const char *s = m.as<const char *>();
            if (s && strcmp(s, "onoff") != 0) { evt->dimmable = 1; break; }
        }
    }
    if (attrs["temperature"].is<float>()) {
        evt->temperature = attrs["temperature"].as<float>();
    }
    /* Climate (US-011) */
    const char *fan = attrs["fan_mode"].as<const char *>();
    if (fan) {
        strncpy(evt->fan_mode, fan, sizeof(evt->fan_mode) - 1);
    }
    if (attrs["fan_modes"].is<JsonArray>()) {
        copy_mode_list(attrs["fan_modes"], evt->fan_modes, &evt->fan_mode_count);
    }
    if (attrs["hvac_modes"].is<JsonArray>()) {
        copy_mode_list(attrs["hvac_modes"], evt->hvac_modes, &evt->hvac_mode_count);
    }
}

/* get_states-antwoord: alle entiteiten in de actieve view als
   STATE_CHANGED doorzetten naar de UI (US-008: refresh na herverbinding).
   Entiteiten buiten de actieve view worden genegeerd. */
static void handle_states_result(JsonDocument &doc) {
    view_model_t *vm = ha_lovelace_get_view_model();
    if (!vm || vm->total_entities == 0) return;

    int updated = 0;
    for (JsonObject st : doc["result"].as<JsonArray>()) {
        const char *entity_id = st["entity_id"];
        const char *state     = st["state"];
        if (!entity_id || !state) continue;
        if (!entities_find(vm, entity_id)) continue;

        ha_event_t evt = {.type = HA_EVT_STATE_CHANGED,
                          .brightness_pct = -1, .temperature = -1};
        strncpy(evt.entity_id, entity_id, sizeof(evt.entity_id) - 1);
        strncpy(evt.state,     state,     sizeof(evt.state) - 1);

        parse_state_attrs(st["attributes"], &evt);
        /* Burst kan groter zijn dan de queue-diepte: korte timeout als backpressure */
        xQueueSend(ha_event_queue, &evt, pdMS_TO_TICKS(100));
        updated++;
    }
    ESP_LOGI(TAG, "get_states: %d entiteiten in actieve view ververst", updated);
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

void ha_messages_call_service_str(esp_websocket_client_handle_t client,
                                  int *msg_id,
                                  const char *entity_id,
                                  const char *service,
                                  const char *data_key,
                                  const char *data_value) {
    char buf[256];
    char domain[32];

    const char *dot = strchr(entity_id, '.');
    size_t dlen = dot ? (size_t)(dot - entity_id) : sizeof(domain) - 1;
    strncpy(domain, entity_id, dlen);
    domain[dlen] = '\0';

    snprintf(buf, sizeof(buf),
             "{\"id\":%d,\"type\":\"call_service\",\"domain\":\"%s\","
             "\"service\":\"%s\",\"target\":{\"entity_id\":\"%s\"},"
             "\"service_data\":{\"%s\":\"%s\"}}",
             (*msg_id)++, domain, service, entity_id, data_key, data_value);
    send_json(client, buf);
}

void ha_messages_handle(esp_websocket_client_handle_t client,
                        const char *data, int len, int *msg_id) {
    /* Filter-parse: alleen benodigde velden worden opgeslagen. Zonder filter
       duurt het parsen van een grote lovelace-config of get_states-payload
       vele seconden (ArduinoJson string-pool is kwadratisch) en houdt de
       websocket-taak de CPU vast (task-watchdog). Het lovelace-result wordt
       hier bewust NIET meegenomen — ha_lovelace parseert de ruwe data zelf
       met een eigen filter. */
    /* Let op: volledige ketting-toewijzingen. Een JsonVariant-tussenvariabele
       (bv. `JsonVariant v = filter["result"][0]`) is in ArduinoJson v7 een
       losgekoppelde null-variant — schrijfacties erop komen niet in het
       filterdocument terecht en die tak wordt dan volledig weggefilterd. */
    JsonDocument filter;
    filter["type"]    = true;
    filter["id"]      = true;
    filter["success"] = true;
    filter["event"]["data"]["entity_id"]          = true;
    filter["event"]["data"]["new_state"]["state"] = true;
    filter["event"]["data"]["new_state"]["attributes"]["friendly_name"] = true;
    filter["event"]["data"]["new_state"]["attributes"]["brightness"]  = true;
    filter["event"]["data"]["new_state"]["attributes"]["temperature"] = true;
    filter["event"]["data"]["new_state"]["attributes"]["fan_mode"]    = true;
    filter["event"]["data"]["new_state"]["attributes"]["fan_modes"]   = true;
    filter["event"]["data"]["new_state"]["attributes"]["hvac_modes"]  = true;
    filter["event"]["data"]["new_state"]["attributes"]["supported_color_modes"] = true;
    /* get_states-result is een array; lovelace-result (object) valt hierdoor
       automatisch buiten het filter */
    filter["result"][0]["entity_id"] = true;
    filter["result"][0]["state"]     = true;
    filter["result"][0]["attributes"]["friendly_name"] = true;
    filter["result"][0]["attributes"]["brightness"]  = true;
    filter["result"][0]["attributes"]["temperature"] = true;
    filter["result"][0]["attributes"]["fan_mode"]    = true;
    filter["result"][0]["attributes"]["fan_modes"]   = true;
    filter["result"][0]["attributes"]["hvac_modes"]  = true;
    filter["result"][0]["attributes"]["supported_color_modes"] = true;

    JsonDocument doc;
    DeserializationError err = deserializeJson(
        doc, data, len,
        DeserializationOption::Filter(filter),
        DeserializationOption::NestingLimit(20));
    if (err) {
        ESP_LOGW(TAG, "JSON parse fout: %s", err.c_str());
        return;
    }

    const char *type = doc["type"];
    if (!type) return;

    if (strcmp(type, "auth_required") == 0) {
        /* Even groot als de opslagkant (web_config_result_t.ha_token):
           een te kleine buffer laat nvs_get_str volledig falen en dan
           wordt er een lege token gestuurd */
        char token[512];
        storage_get_string("ha_token", token, sizeof(token), "");
        if (!token[0]) ESP_LOGE(TAG, "Geen token in NVS — auth gaat falen");
        ha_messages_send_auth(client, token);

    } else if (strcmp(type, "auth_ok") == 0) {
        ESP_LOGI(TAG, "HA authenticatie geslaagd");
        ha_messages_get_states(client, msg_id);
        ha_messages_subscribe_events(client, msg_id);
        ha_event_t evt = {.type = HA_EVT_CONNECTED};
        xQueueSend(ha_event_queue, &evt, pdMS_TO_TICKS(200));

    } else if (strcmp(type, "auth_invalid") == 0) {
        ESP_LOGE(TAG, "HA authenticatie mislukt — controleer token");
        ha_event_t evt = {.type = HA_EVT_AUTH_FAILED};
        xQueueSend(ha_event_queue, &evt, pdMS_TO_TICKS(200));

    } else if (strcmp(type, "result") == 0) {
        int  id      = doc["id"].as<int>();
        bool success = doc["success"].as<bool>();
        if (!success) {
            ESP_LOGW(TAG, "Request %d mislukt", id);
            return;
        }
        if (id == s_get_states_id) {
            s_get_states_id = -1;
            handle_states_result(doc);
            return;
        }
        ha_lovelace_handle_result(id, data, len);

    } else if (strcmp(type, "event") == 0) {
        const char *entity_id = doc["event"]["data"]["entity_id"];
        const char *new_state = doc["event"]["data"]["new_state"]["state"];
        if (!entity_id || !new_state) return;

        ha_event_t evt = {.type = HA_EVT_STATE_CHANGED,
                          .brightness_pct = -1, .temperature = -1};
        strncpy(evt.entity_id, entity_id, sizeof(evt.entity_id) - 1);
        strncpy(evt.state,     new_state, sizeof(evt.state) - 1);

        parse_state_attrs(doc["event"]["data"]["new_state"]["attributes"], &evt);
        xQueueSend(ha_event_queue, &evt, 0);
    }
}
