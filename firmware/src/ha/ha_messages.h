#pragma once
#include "esp_websocket_client.h"

void ha_messages_handle(esp_websocket_client_handle_t client,
                        const char *data, int len, int *msg_id);

void ha_messages_send_auth(esp_websocket_client_handle_t client,
                           const char *token);

void ha_messages_subscribe_events(esp_websocket_client_handle_t client,
                                  int *msg_id);

void ha_messages_get_states(esp_websocket_client_handle_t client,
                            int *msg_id);

void ha_messages_call_service(esp_websocket_client_handle_t client,
                              int *msg_id,
                              const char *entity_id,
                              const char *service,
                              int turn_on,
                              float brightness_pct,
                              float temperature);
