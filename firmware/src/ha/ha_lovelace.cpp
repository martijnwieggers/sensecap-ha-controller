#include "ha_lovelace.h"
#include "../app/app_events.h"
#include "../app/app_entities.h"
#include "ArduinoJson.h"
#include "esp_log.h"
#include <string.h>

extern "C" QueueHandle_t ha_event_queue;
static const char *TAG = "ha_lovelace";

/* ---- View-lijst ---- */
static ha_view_info_t s_views[MAX_VIEWS];
static int            s_view_count = 0;

/* ---- Entiteiten (geladen bij view-selectie) ---- */
static entity_t      s_entities[MAX_ENTITIES];
static view_model_t  s_view_model;

/* ---- Pending request tracking ---- */
typedef enum { PENDING_NONE, PENDING_VIEWS, PENDING_ENTITIES } pending_t;
static pending_t s_pending         = PENDING_NONE;
static int       s_pending_id      = -1;
static char      s_pending_path[64] = {0};

/* ---- Stuur JSON via WebSocket ---- */
static void send_json(esp_websocket_client_handle_t client, const char *json) {
    esp_websocket_client_send_text(client, json, strlen(json), pdMS_TO_TICKS(2000));
}

/* ---- Publieke request-functies ---- */

void ha_lovelace_request_all(esp_websocket_client_handle_t client, int *msg_id) {
    s_pending    = PENDING_VIEWS;
    s_pending_id = *msg_id;
    char buf[80];
    snprintf(buf, sizeof(buf),
             "{\"id\":%d,\"type\":\"lovelace/config\"}", (*msg_id)++);
    send_json(client, buf);
    ESP_LOGI(TAG, "Lovelace config opgehaald (id=%d)", s_pending_id);
}

void ha_lovelace_request(esp_websocket_client_handle_t client,
                         int *msg_id, const char *view_path) {
    s_pending    = PENDING_ENTITIES;
    s_pending_id = *msg_id;
    strncpy(s_pending_path, view_path, sizeof(s_pending_path) - 1);

    char buf[128];
    snprintf(buf, sizeof(buf),
             "{\"id\":%d,\"type\":\"lovelace/config\"}", (*msg_id)++);
    send_json(client, buf);
    ESP_LOGI(TAG, "Lovelace voor view '%s' opgehaald (id=%d)",
             view_path, s_pending_id);
}

/* ---- Hulpfuncties voor JSON-parsing ---- */

static void set_custom_name(entity_t *e, const char *name) {
    if (!name || !name[0]) return;
    strncpy(e->name, name, sizeof(e->name) - 1);
    e->name[sizeof(e->name) - 1] = '\0';
    e->name_custom = true;
}

static void fill_entity_defaults(entity_t *e) {
    e->domain = entities_parse_domain(e->entity_id);
    e->name_custom = false;
    e->dimmable    = false;   /* wordt gezet zodra get_states binnenkomt */

    /* Naam: domein-prefix verwijderen en underscores vervangen */
    const char *dot = strchr(e->entity_id, '.');
    if (dot) {
        strncpy(e->name, dot + 1, sizeof(e->name) - 1);
    } else {
        strncpy(e->name, e->entity_id, sizeof(e->name) - 1);
    }
    e->name[sizeof(e->name) - 1] = '\0';
    for (char *p = e->name; *p; p++) {
        if (*p == '_') *p = ' ';
    }

    strncpy(e->state, "unavailable", sizeof(e->state) - 1);
    e->brightness_pct = -1.0f;
    e->temperature    = -1.0f;
    e->temp_min       = 16.0f;
    e->temp_max       = 30.0f;
    e->available      = false;
    e->widget         = entities_resolve_widget(e);
}

static bool entity_already_added(int count, const char *eid) {
    for (int i = 0; i < count; i++) {
        if (strcmp(s_entities[i].entity_id, eid) == 0) return true;
    }
    return false;
}

static int extract_entities_from_cards(JsonArray cards, int count) {
    for (JsonObject card : cards) {
        if (count >= MAX_ENTITIES) break;

        /* Type 1: card heeft een enkel entity-veld; card-level "name"
           overschrijft de entiteitsnaam (Lovelace-gedrag) */
        const char *single = card["entity"];
        if (single && !entity_already_added(count, single)) {
            strncpy(s_entities[count].entity_id, single,
                    sizeof(s_entities[count].entity_id) - 1);
            fill_entity_defaults(&s_entities[count]);
            set_custom_name(&s_entities[count], card["name"]);
            count++;
        }

        /* Type 2: card heeft een entities-lijst; list-items kunnen een
           eigen "name" dragen */
        if (card["entities"].is<JsonArray>()) {
            for (JsonVariant ent : card["entities"].as<JsonArray>()) {
                if (count >= MAX_ENTITIES) break;
                const char *eid   = NULL;
                const char *ename = NULL;
                if (ent.is<const char *>()) {
                    eid = ent.as<const char *>();
                } else if (ent.is<JsonObject>()) {
                    eid   = ent["entity"];
                    ename = ent["name"];
                }
                if (eid && !entity_already_added(count, eid)) {
                    strncpy(s_entities[count].entity_id, eid,
                            sizeof(s_entities[count].entity_id) - 1);
                    fill_entity_defaults(&s_entities[count]);
                    set_custom_name(&s_entities[count], ename);
                    count++;
                }
            }
        }
    }
    return count;
}

/* Cards hangen direct onder de view (klassiek dashboard) of genest per
   sectie in views[].sections[].cards[] (sections-dashboard, HA 2024+) */
static int extract_entities_from_view(JsonObject view) {
    int count = 0;
    if (view["cards"].is<JsonArray>()) {
        count = extract_entities_from_cards(view["cards"].as<JsonArray>(), count);
    }
    if (view["sections"].is<JsonArray>()) {
        for (JsonObject sec : view["sections"].as<JsonArray>()) {
            if (count >= MAX_ENTITIES) break;
            if (sec["cards"].is<JsonArray>()) {
                count = extract_entities_from_cards(sec["cards"].as<JsonArray>(),
                                                    count);
            }
        }
    }
    return count;
}

/* ---- Verwerk result-bericht ---- */

void ha_lovelace_handle_result(int id, const char *data, int len) {
    if (id != s_pending_id || s_pending == PENDING_NONE) return;

    /* Filter-parse: alleen titel/pad en de entity-velden van cards worden
       opgeslagen; de rest van de (mogelijk zeer grote) dashboard-config
       wordt overgeslagen. Zie ha_messages_handle voor de achtergrond. */
    /* Let op: volledige ketting-toewijzingen. Een JsonVariant-tussenvariabele
       (bv. `JsonVariant v = filter["result"]["views"][0]`) is in ArduinoJson
       v7 een losgekoppelde null-variant — schrijfacties erop komen niet in
       het filterdocument terecht en het filter blijft dan leeg. */
    JsonDocument filter;
    filter["result"]["views"][0]["title"]                = true;
    filter["result"]["views"][0]["path"]                 = true;
    filter["result"]["views"][0]["cards"][0]["entity"]   = true;
    filter["result"]["views"][0]["cards"][0]["entities"] = true;
    filter["result"]["views"][0]["cards"][0]["name"]     = true;
    /* Sections-dashboard (HA 2024+) */
    filter["result"]["views"][0]["sections"][0]["cards"][0]["entity"]   = true;
    filter["result"]["views"][0]["sections"][0]["cards"][0]["entities"] = true;
    filter["result"]["views"][0]["sections"][0]["cards"][0]["name"]     = true;

    JsonDocument doc;
    DeserializationError err = deserializeJson(
        doc, data, len,
        DeserializationOption::Filter(filter),
        DeserializationOption::NestingLimit(20));
    if (err) {
        ESP_LOGW(TAG, "JSON parse fout: %s", err.c_str());
        return;
    }

    JsonObject result = doc["result"];
    if (!result) {
        /* Toon het begin van het ruwe antwoord: onderscheidt een
           strategy-dashboard zonder opgeslagen config ("result":null),
           een afwijkende structuur en een leeg gefilterd antwoord */
        int head = len < 200 ? len : 200;
        ESP_LOGW(TAG, "Geen 'result'-object in antwoord (id=%d); "
                 "begin payload: %.*s", id, head, data);
        return;
    }

    if (s_pending == PENDING_VIEWS) {
        /* Parseer views[] uit het dashboard-object */
        s_view_count = 0;
        JsonArray views = result["views"].as<JsonArray>();
        ESP_LOGI(TAG, "Antwoord: %d bytes, views-array met %d element(en)",
                 len, (int)views.size());
        for (JsonObject view : views) {
            if (s_view_count >= MAX_VIEWS) break;
            const char *title = view["title"];
            const char *path  = view["path"];
            if (!title) title = "(zonder titel)";
            /* Views zonder pad (URL-veld leeg in HA) krijgen een synthetisch
               pad "#<index>" zodat ze toch kiesbaar en laadbaar zijn */
            char synth[16];
            if (!path || !path[0]) {
                snprintf(synth, sizeof(synth), "#%d", s_view_count);
                path = synth;
            }
            ESP_LOGI(TAG, "  view %d: '%s' (pad '%s')",
                     s_view_count, title, path);
            strncpy(s_views[s_view_count].title, title,
                    sizeof(s_views[s_view_count].title) - 1);
            strncpy(s_views[s_view_count].path, path,
                    sizeof(s_views[s_view_count].path) - 1);
            s_view_count++;
        }
        ESP_LOGI(TAG, "%d view(s) geladen", s_view_count);

        ha_event_t evt = {.type = HA_EVT_VIEWS_LOADED};
        xQueueSend(ha_event_queue, &evt, 0);

    } else if (s_pending == PENDING_ENTITIES) {
        /* Zoek de geselecteerde view en extraheer entiteiten */
        memset(s_entities, 0, sizeof(s_entities));
        memset(&s_view_model, 0, sizeof(s_view_model));

        int entity_count = 0;
        int view_idx     = 0;
        JsonArray views  = result["views"].as<JsonArray>();
        for (JsonObject view : views) {
            /* Zelfde synthetische "#<index>"-paden als bij PENDING_VIEWS */
            char synth[16];
            const char *path = view["path"];
            if (!path || !path[0]) {
                snprintf(synth, sizeof(synth), "#%d", view_idx);
                path = synth;
            }
            view_idx++;

            /* Vergelijk met geselecteerd pad (ook '' matcht eerste view) */
            bool match = (strcmp(path, s_pending_path) == 0) ||
                         (s_pending_path[0] == '\0');
            if (!match) continue;

            /* Titel opslaan */
            const char *title = view["title"];
            if (title) {
                strncpy(s_view_model.view_title, title,
                        sizeof(s_view_model.view_title) - 1);
                s_view_model.view_title[sizeof(s_view_model.view_title) - 1] = '\0';
            }
            strncpy(s_view_model.view_path, s_pending_path,
                    sizeof(s_view_model.view_path) - 1);
            s_view_model.view_path[sizeof(s_view_model.view_path) - 1] = '\0';

            /* Entiteiten ophalen uit cards (direct of per sectie) */
            entity_count = extract_entities_from_view(view);
            break;
        }

        ESP_LOGI(TAG, "%d entiteit(en) geladen voor view '%s'",
                 entity_count, s_pending_path);
        entities_build_pages(&s_view_model, s_entities, entity_count);

        ha_event_t evt = {.type = HA_EVT_ENTITIES_LOADED};
        xQueueSend(ha_event_queue, &evt, 0);
    }

    s_pending    = PENDING_NONE;
    s_pending_id = -1;
}

/* ---- Publieke accessor-functies ---- */

int ha_lovelace_get_view_count(void) {
    return s_view_count;
}

const ha_view_info_t *ha_lovelace_get_view(int idx) {
    if (idx < 0 || idx >= s_view_count) return NULL;
    return &s_views[idx];
}

view_model_t *ha_lovelace_get_view_model(void) {
    return &s_view_model;
}
