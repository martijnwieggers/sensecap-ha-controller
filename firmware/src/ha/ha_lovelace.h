#pragma once
#include "esp_websocket_client.h"

/* Haal alle views op van het standaard dashboard */
void ha_lovelace_request_all(esp_websocket_client_handle_t client, int *msg_id);

/* Haal entiteiten op voor een specifieke view */
void ha_lovelace_request(esp_websocket_client_handle_t client,
                         int *msg_id, const char *view_path);

/* Verwerk een lovelace/config resultaat (aangeroepen vanuit ha_messages) */
void ha_lovelace_handle_result(const char *json, int len);
