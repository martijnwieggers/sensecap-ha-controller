#pragma once
#include <stdint.h>
#include "app_entities.h"   /* MAX_MODES / MODE_STR_LEN */

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    HA_EVT_CONNECTED,
    HA_EVT_DISCONNECTED,
    HA_EVT_AUTH_FAILED,     /* HA wees het token af (auth_invalid) */
    HA_EVT_VIEWS_LOADED,
    HA_EVT_ENTITIES_LOADED,
    HA_EVT_STATE_CHANGED,
} ha_event_type_t;

typedef struct {
    ha_event_type_t type;
    char entity_id[64];
    char state[32];
    /* friendly_name uit HA-attributen; leeg = niet meegeleverd */
    char friendly_name[48];
    float brightness_pct;
    float temperature;
    /* Dimbaarheid uit supported_color_modes (US-014):
       0 = niet meegeleverd, 1 = dimbaar, 2 = niet dimbaar */
    uint8_t dimmable;
    /* Climate (US-011). Lege string / count 0 = niet meegeleverd,
       bestaande waarde in entity_t blijft dan staan. */
    char fan_mode[MODE_STR_LEN];
    char fan_modes[MAX_MODES][MODE_STR_LEN];
    int  fan_mode_count;
    char hvac_modes[MAX_MODES][MODE_STR_LEN];
    int  hvac_mode_count;
} ha_event_t;

typedef enum {
    CMD_TOGGLE_ENTITY,
    CMD_SET_BRIGHTNESS,
    CMD_SET_TEMPERATURE,
    CMD_SET_HVAC_MODE,
    CMD_SET_FAN_MODE,
    CMD_PRESS_BUTTON,
    CMD_LOAD_VIEW,
    CMD_GET_VIEWS,
} ha_cmd_type_t;

typedef struct {
    ha_cmd_type_t type;
    char entity_id[64];
    float value;
    char str_value[MODE_STR_LEN];   /* hvac_mode / fan_mode (US-011) */
    char view_path[64];
} ha_cmd_t;

void app_events_handle(const ha_event_t *evt);

#ifdef __cplusplus
}
#endif
