#include "mock_ha.h"
#include "../../firmware/src/ha/ha_client.h"
#include "../../firmware/src/ha/ha_lovelace.h"
#include "../../firmware/src/app/app_entities.h"
#include "../../firmware/src/platform/storage.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <SDL2/SDL.h>

static void (*s_send)(const ha_event_t *) = NULL;

/* ================================================================
   Nep entiteiten die de simulator aanbiedt
   ================================================================ */
static struct {
    const char *entity_id;
    const char *name;
    char        state[16];
    float brightness_pct;
    float temperature;
    char        fan_mode[16];   /* alleen climate (US-011) */
} s_entities[] = {
    {"switch.woonkamer_licht",  "Woonkamer licht",  "off",  -1,    -1,   ""     },
    {"light.dimmer_bank",       "Dimmer bank",       "on",   72,    -1,   ""     },
    {"climate.woonkamer",       "Thermostaat",       "heat", -1,    21.5, ""     },
    {"climate.airco",           "Airco",             "cool", -1,    23.0, "auto" },
    {"sensor.temperatuur",      "Buitentemperatuur", "14.2", -1,    -1,   ""     },
    {"binary_sensor.deurbel",   "Deurbel",           "off",  -1,    -1,   ""     },
    {"sensor.luchtvochtigheid", "Luchtvochtigheid",  "58",   -1,    -1,   ""     },
    {"script.goedemorgen",      "Goedemorgen",       "off",  -1,    -1,   ""     },
    {"scene.filmavond",         "Filmavond",         "off",  -1,    -1,   ""     },
    {"button.bel_aan",          "Bel aan",           "off",  -1,    -1,   ""     },
};
#define N_ENTITIES (int)(sizeof(s_entities)/sizeof(s_entities[0]))

/* Mode-lijsten voor de climate-entiteiten in de simulator */
static const char *SIM_HVAC_MODES[] = {"off", "cool", "heat", "dry", "fan_only"};
static const char *SIM_FAN_MODES[]  = {"auto", "low", "medium", "high"};
#define N_SIM_HVAC (int)(sizeof(SIM_HVAC_MODES)/sizeof(SIM_HVAC_MODES[0]))
#define N_SIM_FAN  (int)(sizeof(SIM_FAN_MODES)/sizeof(SIM_FAN_MODES[0]))

/* ================================================================
   Lovelace stubs — view-lijst en view-model voor de simulator
   ================================================================ */

static ha_view_info_t s_sim_views[] = {
    {"Woonkamer",  "woonkamer"},
    {"Slaapkamer", "slaapkamer"},
    {"Buiten",     "buiten"},
};
#define N_VIEWS 3

int ha_lovelace_get_view_count(void) {
    return N_VIEWS;
}

const ha_view_info_t *ha_lovelace_get_view(int idx) {
    if (idx < 0 || idx >= N_VIEWS) return NULL;
    return &s_sim_views[idx];
}

/* Statisch view-model met de 6 simulatie-entiteiten */
static entity_t     s_sim_entities[MAX_ENTITIES];
static view_model_t s_sim_vm;
static bool         s_vm_initialized = false;

static void init_view_model(void) {
    if (s_vm_initialized) return;
    s_vm_initialized = true;

    memset(s_sim_entities, 0, sizeof(s_sim_entities));
    memset(&s_sim_vm, 0, sizeof(s_sim_vm));

    strncpy(s_sim_vm.view_title, "Woonkamer", sizeof(s_sim_vm.view_title) - 1);
    strncpy(s_sim_vm.view_path,  "woonkamer", sizeof(s_sim_vm.view_path)  - 1);

    for (int i = 0; i < N_ENTITIES; i++) {
        strncpy(s_sim_entities[i].entity_id, s_entities[i].entity_id,
                sizeof(s_sim_entities[i].entity_id) - 1);
        strncpy(s_sim_entities[i].name, s_entities[i].name,
                sizeof(s_sim_entities[i].name) - 1);
        strncpy(s_sim_entities[i].state, s_entities[i].state,
                sizeof(s_sim_entities[i].state) - 1);
        s_sim_entities[i].brightness_pct = s_entities[i].brightness_pct;
        s_sim_entities[i].temperature    = s_entities[i].temperature;
        s_sim_entities[i].temp_min       = 16.0f;
        s_sim_entities[i].temp_max       = 30.0f;
        s_sim_entities[i].available      = true;
        s_sim_entities[i].domain  = entities_parse_domain(s_entities[i].entity_id);
        s_sim_entities[i].widget  = entities_resolve_widget(&s_sim_entities[i]);

        /* Climate: mode-lijsten en fan-stand meegeven (US-011).
           De thermostaat heeft bewust geen fan_modes — test voor de
           neutrale fan-knop. */
        if (s_sim_entities[i].domain == DOMAIN_CLIMATE) {
            for (int m = 0; m < N_SIM_HVAC && m < MAX_MODES; m++) {
                strncpy(s_sim_entities[i].hvac_modes[m], SIM_HVAC_MODES[m],
                        MODE_STR_LEN - 1);
            }
            s_sim_entities[i].hvac_mode_count = N_SIM_HVAC;

            if (s_entities[i].fan_mode[0]) {
                strncpy(s_sim_entities[i].fan_mode, s_entities[i].fan_mode,
                        sizeof(s_sim_entities[i].fan_mode) - 1);
                for (int m = 0; m < N_SIM_FAN && m < MAX_MODES; m++) {
                    strncpy(s_sim_entities[i].fan_modes[m], SIM_FAN_MODES[m],
                            MODE_STR_LEN - 1);
                }
                s_sim_entities[i].fan_mode_count = N_SIM_FAN;
            }
        }
    }
    entities_build_pages(&s_sim_vm, s_sim_entities, N_ENTITIES);
}

view_model_t *ha_lovelace_get_view_model(void) {
    init_view_model();
    return &s_sim_vm;
}

/* ha_lovelace_handle_result — niet nodig in simulator */
void ha_lovelace_handle_result(int id, const char *data, int len) {
    (void)id; (void)data; (void)len;
}

/* ha_lovelace_request_all en _request — stubs */
void ha_lovelace_request_all(esp_websocket_client_handle_t c, int *id) {
    (void)c; (void)id;
}
void ha_lovelace_request(esp_websocket_client_handle_t c, int *id,
                         const char *path) {
    (void)c; (void)id; (void)path;
}

/* ================================================================
   ha_client stubs — gebruiken s_send om events te genereren
   ================================================================ */

void ha_client_run(void) { /* niet gebruikt in simulator */ }

void ha_client_toggle(const char *entity_id) {
    ha_cmd_t cmd = {.type = CMD_TOGGLE_ENTITY};
    strncpy(cmd.entity_id, entity_id, sizeof(cmd.entity_id) - 1);
    mock_ha_handle_cmd(&cmd);
}

void ha_client_set_brightness(const char *entity_id, float pct) {
    ha_cmd_t cmd = {.type = CMD_SET_BRIGHTNESS, .value = pct};
    strncpy(cmd.entity_id, entity_id, sizeof(cmd.entity_id) - 1);
    mock_ha_handle_cmd(&cmd);
}

void ha_client_set_temperature(const char *entity_id, float temp) {
    ha_cmd_t cmd = {.type = CMD_SET_TEMPERATURE, .value = temp};
    strncpy(cmd.entity_id, entity_id, sizeof(cmd.entity_id) - 1);
    mock_ha_handle_cmd(&cmd);
}

void ha_client_press_button(const char *entity_id) {
    ha_cmd_t cmd = {.type = CMD_PRESS_BUTTON};
    strncpy(cmd.entity_id, entity_id, sizeof(cmd.entity_id) - 1);
    mock_ha_handle_cmd(&cmd);
}

void ha_client_set_hvac_mode(const char *entity_id, const char *mode) {
    ha_cmd_t cmd = {.type = CMD_SET_HVAC_MODE};
    strncpy(cmd.entity_id, entity_id, sizeof(cmd.entity_id) - 1);
    strncpy(cmd.str_value, mode, sizeof(cmd.str_value) - 1);
    mock_ha_handle_cmd(&cmd);
}

void ha_client_set_fan_mode(const char *entity_id, const char *mode) {
    ha_cmd_t cmd = {.type = CMD_SET_FAN_MODE};
    strncpy(cmd.entity_id, entity_id, sizeof(cmd.entity_id) - 1);
    strncpy(cmd.str_value, mode, sizeof(cmd.str_value) - 1);
    mock_ha_handle_cmd(&cmd);
}

void ha_client_get_views(void) {
    printf("[mock_ha] get_views → HA_EVT_VIEWS_LOADED (%d views)\n", N_VIEWS);
    if (s_send) {
        ha_event_t evt = {.type = HA_EVT_VIEWS_LOADED};
        s_send(&evt);
    }
}

void ha_client_get_states(void) {
    printf("[mock_ha] get_states → %d STATE_CHANGED events\n", N_ENTITIES);
    if (!s_send) return;
    for (int i = 0; i < N_ENTITIES; i++) {
        ha_event_t evt = {
            .type           = HA_EVT_STATE_CHANGED,
            .brightness_pct = s_entities[i].brightness_pct,
            .temperature    = s_entities[i].temperature,
        };
        strncpy(evt.entity_id, s_entities[i].entity_id, sizeof(evt.entity_id)-1);
        strncpy(evt.state,     s_entities[i].state,     sizeof(evt.state)-1);
        strncpy(evt.fan_mode,  s_entities[i].fan_mode,  sizeof(evt.fan_mode)-1);
        s_send(&evt);
    }
}

void ha_client_load_view(const char *view_path) {
    printf("[mock_ha] load_view: '%s' → entiteiten laden\n", view_path);
    if (!s_send) return;

    /* Initiële staat van alle entiteiten sturen */
    for (int i = 0; i < N_ENTITIES; i++) {
        ha_event_t evt = {
            .type           = HA_EVT_STATE_CHANGED,
            .brightness_pct = s_entities[i].brightness_pct,
            .temperature    = s_entities[i].temperature,
        };
        strncpy(evt.entity_id, s_entities[i].entity_id, sizeof(evt.entity_id)-1);
        strncpy(evt.state,     s_entities[i].state,     sizeof(evt.state)-1);
        strncpy(evt.fan_mode,  s_entities[i].fan_mode,  sizeof(evt.fan_mode)-1);
        s_send(&evt);
    }

    /* View-model initialiseren en entiteiten-geladen signaleren */
    init_view_model();
    ha_event_t loaded = {.type = HA_EVT_ENTITIES_LOADED};
    s_send(&loaded);
}

/* ================================================================
   Periodieke event-simulator (realtime updates)
   ================================================================ */

/* Eenmalige verbindings-callback via SDL_AddTimer (niet-blokkerend) */
static Uint32 connect_delay_cb(Uint32 interval, void *param) {
    (void)interval; (void)param;
    printf("[mock_ha] HA verbonden (gesimuleerd)\n");
    if (s_send) {
        ha_event_t evt = {.type = HA_EVT_CONNECTED};
        s_send(&evt);
    }
    return 0; /* one-shot: return 0 stopt de timer */
}

/* Simuleert 'HA niet gevonden': na de connect-timeout een DISCONNECTED-event.
   Na 8 s 'komt HA weer online' zodat het herstel-scenario (US-009)
   end-to-end te testen is. */
static Uint32 connect_delay_cb(Uint32 interval, void *param);
static Uint32 timer_cb(Uint32 interval, void *param);

static Uint32 connect_fail_cb(Uint32 interval, void *param) {
    (void)interval; (void)param;
    printf("[mock_ha] HA niet bereikbaar (gesimuleerd) → HA_EVT_DISCONNECTED\n");
    if (s_send) {
        ha_event_t evt = {.type = HA_EVT_DISCONNECTED};
        s_send(&evt);
    }
    printf("[mock_ha] HA komt over 8 s weer online (gesimuleerd herstel)\n");
    SDL_AddTimer(8000, connect_delay_cb, NULL);
    SDL_AddTimer(13000, timer_cb, NULL);
    return 0; /* one-shot */
}

static Uint32 timer_cb(Uint32 interval, void *param) {
    static int tick = 0;
    tick++;
    (void)param;

    if (!s_send) return interval;

    /* Elke 30 s: deurbel triggert */
    if (tick % 6 == 0) {
        ha_event_t evt = {.type = HA_EVT_STATE_CHANGED,
                          .brightness_pct = -1, .temperature = -1};
        strcpy(evt.entity_id, "binary_sensor.deurbel");
        strcpy(evt.state, "on");
        s_send(&evt);
    }
    /* Elke minuut: temperatuur fluctuatie */
    if (tick % 12 == 0) {
        ha_event_t evt = {.type = HA_EVT_STATE_CHANGED,
                          .brightness_pct = -1, .temperature = -1};
        strcpy(evt.entity_id, "sensor.temperatuur");
        float new_temp = 13.0f + (rand() % 40) / 10.0f;
        snprintf(evt.state, sizeof(evt.state), "%.1f", new_temp);
        s_send(&evt);
    }
    return interval;
}

void mock_ha_start(void (*send_fn)(const ha_event_t *)) {
    s_send = send_fn;
    init_view_model();

    uint8_t configured   = storage_get_u8("configured", 0);
    uint8_t ha_available = storage_get_u8("ha_available", 1);
    printf("[mock_ha] Simulator gestart — %d entiteiten, %d views "
           "(configured=%d, ha_available=%d)\n",
           N_ENTITIES, N_VIEWS, configured, ha_available);

    if (configured && !ha_available) {
        /* config.ini: ha_available=0 → simuleer 'HA niet gevonden' */
        SDL_AddTimer(800, connect_fail_cb, NULL);
    } else if (configured) {
        /* Geconfigureerd: simuleer HA-verbinding na 800 ms (niet-blokkerend).
           Views worden geladen via ha_client_get_views() vanuit app_events.
           Entiteiten via ha_client_load_view() wanneer gebruiker een view kiest. */
        SDL_AddTimer(800, connect_delay_cb, NULL);
        SDL_AddTimer(5000, timer_cb, NULL);
    } else {
        /* Niet geconfigureerd: setup wizard is actief, geen HA-verbinding starten. */
        printf("[mock_ha] Niet geconfigureerd — wachten op setup wizard\n");
    }
}

/* ================================================================
   Commando-verwerking (toggle, brightness, temperature)
   ================================================================ */

void mock_ha_handle_cmd(const ha_cmd_t *cmd) {
    for (int i = 0; i < N_ENTITIES; i++) {
        if (strcmp(s_entities[i].entity_id, cmd->entity_id) != 0) continue;

        ha_event_t evt = {
            .type           = HA_EVT_STATE_CHANGED,
            .brightness_pct = s_entities[i].brightness_pct,
            .temperature    = s_entities[i].temperature,
        };
        strncpy(evt.entity_id, s_entities[i].entity_id, sizeof(evt.entity_id)-1);
        strncpy(evt.state,     s_entities[i].state,     sizeof(evt.state)-1);
        strncpy(evt.fan_mode,  s_entities[i].fan_mode,  sizeof(evt.fan_mode)-1);

        switch (cmd->type) {
            case CMD_TOGGLE_ENTITY:
                strcpy(s_entities[i].state,
                       strcmp(s_entities[i].state, "on") == 0 ? "off" : "on");
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
            case CMD_SET_HVAC_MODE:
                strncpy(s_entities[i].state, cmd->str_value,
                        sizeof(s_entities[i].state) - 1);
                strncpy(evt.state, s_entities[i].state, sizeof(evt.state)-1);
                break;
            case CMD_SET_FAN_MODE:
                strncpy(s_entities[i].fan_mode, cmd->str_value,
                        sizeof(s_entities[i].fan_mode) - 1);
                strncpy(evt.fan_mode, s_entities[i].fan_mode,
                        sizeof(evt.fan_mode) - 1);
                break;
            case CMD_PRESS_BUTTON:
                printf("[mock_ha] knop/script/scene geactiveerd: %s\n",
                       cmd->entity_id);
                return;  /* geen state_changed voor eenmalige actie */
            default:
                return;
        }
        printf("[mock_ha] %s → state=%s fan=%s\n",
               cmd->entity_id, evt.state, evt.fan_mode);
        if (s_send) s_send(&evt);
        return;
    }
}
