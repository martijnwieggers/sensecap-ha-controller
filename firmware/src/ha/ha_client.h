#pragma once

/* Blokkeert — roept intern ha_ws_connect() aan en herverbindt bij verbreking.
   Roep aan vanuit een dedicated FreeRTOS-taak. */
void ha_client_run(void);

/* Stuur een service-aanroep naar HA (thread-safe via interne queue). */
void ha_client_toggle(const char *entity_id);
void ha_client_set_brightness(const char *entity_id, float pct);
void ha_client_set_temperature(const char *entity_id, float temp);
void ha_client_set_hvac_mode(const char *entity_id, const char *mode);
void ha_client_set_fan_mode(const char *entity_id, const char *mode);
void ha_client_press_button(const char *entity_id);
void ha_client_load_view(const char *view_path);
void ha_client_get_views(void);
void ha_client_get_states(void);   /* statussen van actieve view verversen */
