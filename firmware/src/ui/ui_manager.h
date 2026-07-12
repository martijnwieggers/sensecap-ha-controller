#pragma once
#include <stdbool.h>
#include "../app/app_events.h"

void ui_manager_init(void);
void ui_manager_show_setup(void);
/* True zolang de setup-wizard/instellingen het actieve scherm is —
   HA-events mogen dan geen schermwissel forceren */
bool ui_manager_setup_active(void);
void ui_manager_show_view_menu(void);
void ui_manager_show_entities(void);
void ui_manager_show_status(void);
void ui_manager_show_disconnected(void);

/* Realtime updates — aanroepen vanuit app_events_handle() */
void ui_manager_refresh_view_list(void);
void ui_manager_update_entity(const ha_event_t *evt);
