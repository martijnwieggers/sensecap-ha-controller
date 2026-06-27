#include "mock_ha.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <SDL2/SDL.h>

static void (*s_send)(const ha_event_t *) = NULL;

/* Initiële staat van de 6 nep-entiteiten */
static struct {
    const char *entity_id;
    const char *name;
    const char *state;
    float brightness_pct;
    float temperature;
} s_entities[] = {
    {"switch.woonkamer_licht",  "Woonkamer licht",  "off",  -1,   -1  },
    {"light.dimmer_bank",       "Dimmer bank",       "on",   72,   -1  },
    {"climate.woonkamer",       "Thermostaat",       "heat", -1,   21.5},
    {"sensor.temperatuur",      "Buitentemperatuur", "14.2", -1,   -1  },
    {"binary_sensor.deurbel",   "Deurbel",           "off",  -1,   -1  },
    {"sensor.luchtvochtigheid", "Luchtvochtigheid",  "58",   -1,   -1  },
};
#define N_ENTITIES (int)(sizeof(s_entities)/sizeof(s_entities[0]))

static void send_connected(void) {
    ha_event_t evt = {.type = HA_EVT_CONNECTED};
    s_send(&evt);
}

static void send_entities_loaded(void) {
    /* Stuur alle initiële staten als state_changed events */
    for (int i = 0; i < N_ENTITIES; i++) {
        ha_event_t evt = {
            .type           = HA_EVT_STATE_CHANGED,
            .brightness_pct = s_entities[i].brightness_pct,
            .temperature    = s_entities[i].temperature,
        };
        strncpy(evt.entity_id, s_entities[i].entity_id, sizeof(evt.entity_id)-1);
        strncpy(evt.state,     s_entities[i].state,     sizeof(evt.state)-1);
        s_send(&evt);
    }
    ha_event_t loaded = {.type = HA_EVT_ENTITIES_LOADED};
    s_send(&loaded);
}

/* Eenvoudige periodicke event-simulatie via SDL-timer */
static Uint32 timer_cb(Uint32 interval, void *param) {
    static int tick = 0;
    tick++;

    /* Elke 30 seconden: deurbel "triggert" */
    if (tick % 6 == 0) {
        ha_event_t evt = {
            .type           = HA_EVT_STATE_CHANGED,
            .brightness_pct = -1, .temperature = -1
        };
        strcpy(evt.entity_id, "binary_sensor.deurbel");
        strcpy(evt.state, "on");
        s_send(&evt);

        SDL_AddTimer(1000, timer_cb, NULL);  /* na 1s terug naar off */
    }
    /* Elke minuut: temperatuur fluctuatie */
    if (tick % 12 == 0) {
        ha_event_t evt = {
            .type           = HA_EVT_STATE_CHANGED,
            .brightness_pct = -1, .temperature = -1
        };
        strcpy(evt.entity_id, "sensor.temperatuur");
        float new_temp = 14.0f + (rand() % 30) / 10.0f;
        snprintf(evt.state, sizeof(evt.state), "%.1f", new_temp);
        s_send(&evt);
    }
    return interval;
}

void mock_ha_start(void (*send_fn)(const ha_event_t *)) {
    s_send = send_fn;
    printf("[mock_ha] Simuleer 6 entiteiten — verbinden...\n");

    /* Simuleer auth delay van 500 ms, daarna entiteiten sturen */
    SDL_Delay(500);
    send_connected();
    SDL_Delay(200);
    send_entities_loaded();

    /* Start periodieke simulator-timer (elke 5 seconden) */
    SDL_AddTimer(5000, timer_cb, NULL);
}

void mock_ha_handle_cmd(const ha_cmd_t *cmd) {
    /* Zoek entiteit en pas staat aan */
    for (int i = 0; i < N_ENTITIES; i++) {
        if (strcmp(s_entities[i].entity_id, cmd->entity_id) != 0) continue;

        ha_event_t evt = {
            .type           = HA_EVT_STATE_CHANGED,
            .brightness_pct = s_entities[i].brightness_pct,
            .temperature    = s_entities[i].temperature,
        };
        strncpy(evt.entity_id, s_entities[i].entity_id, sizeof(evt.entity_id)-1);

        switch (cmd->type) {
            case CMD_TOGGLE_ENTITY:
                s_entities[i].state = strcmp(s_entities[i].state, "on") == 0 ? "off" : "on";
                strncpy(evt.state, s_entities[i].state, sizeof(evt.state)-1);
                break;
            case CMD_SET_BRIGHTNESS:
                s_entities[i].brightness_pct = cmd->value;
                evt.brightness_pct = cmd->value;
                strncpy(evt.state, "on", sizeof(evt.state)-1);
                break;
            case CMD_SET_TEMPERATURE:
                s_entities[i].temperature = cmd->value;
                evt.temperature = cmd->value;
                break;
            default:
                return;
        }
        printf("[mock_ha] %s → %s\n", cmd->entity_id, evt.state);
        if (s_send) s_send(&evt);
        return;
    }
}
