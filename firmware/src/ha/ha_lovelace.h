#pragma once
#include "esp_websocket_client.h"
#include "../app/app_entities.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_VIEWS 10

typedef struct {
    char title[48];
    char path[64];
} ha_view_info_t;

/* Haal de lijst van alle dashboards/views op (standaard lovelace config). */
void ha_lovelace_request_all(esp_websocket_client_handle_t client, int *msg_id);

/* Haal de entiteiten op voor een specifieke view. */
void ha_lovelace_request(esp_websocket_client_handle_t client,
                         int *msg_id, const char *view_path);

/* Verwerk een lovelace/config result-bericht (id = het message-id). */
void ha_lovelace_handle_result(int id, const char *data, int len);

/* Geeft het aantal beschikbare views terug na HA_EVT_VIEWS_LOADED. */
int                   ha_lovelace_get_view_count(void);
const ha_view_info_t *ha_lovelace_get_view(int idx);

/* Geeft de view_model_t terug na HA_EVT_ENTITIES_LOADED. */
view_model_t *ha_lovelace_get_view_model(void);

#ifdef __cplusplus
}
#endif
