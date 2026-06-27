#pragma once
#include <stdint.h>

typedef enum {
    HA_EVT_CONNECTED,
    HA_EVT_DISCONNECTED,
    HA_EVT_VIEWS_LOADED,
    HA_EVT_ENTITIES_LOADED,
    HA_EVT_STATE_CHANGED,
} ha_event_type_t;

typedef struct {
    ha_event_type_t type;
    char entity_id[64];
    char state[32];
    float brightness_pct;
    float temperature;
} ha_event_t;

typedef enum {
    CMD_TOGGLE_ENTITY,
    CMD_SET_BRIGHTNESS,
    CMD_SET_TEMPERATURE,
    CMD_LOAD_VIEW,
    CMD_GET_VIEWS,
} ha_cmd_type_t;

typedef struct {
    ha_cmd_type_t type;
    char entity_id[64];
    float value;
    char view_path[64];
} ha_cmd_t;

void app_events_handle(const ha_event_t *evt);
