#pragma once
#include "esp_websocket_client.h"

#ifdef __cplusplus
extern "C" {
#endif

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

/* Service-aanroep met één string-parameter in service_data,
   bijv. set_hvac_mode {"hvac_mode":"cool"} (US-011). */
void ha_messages_call_service_str(esp_websocket_client_handle_t client,
                                  int *msg_id,
                                  const char *entity_id,
                                  const char *service,
                                  const char *data_key,
                                  const char *data_value);

#ifdef __cplusplus
}
#endif
