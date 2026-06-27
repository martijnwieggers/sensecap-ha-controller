#pragma once
#include <stdbool.h>

typedef enum {
    STATE_BOOT,
    STATE_CONFIG_CHECK,
    STATE_SETUP_WIZARD,
    STATE_WIFI_CONNECTING,
    STATE_HA_CONNECTING,
    STATE_HA_AUTH,
    STATE_VIEW_SELECT,
    STATE_ENTITIES_LOADING,
    STATE_VIEW_READY,
    STATE_DISCONNECTED,
} app_state_t;

void        app_state_init(void);
app_state_t app_state_get(void);
void        app_state_set(app_state_t new_state);
bool        app_state_is(app_state_t state);
