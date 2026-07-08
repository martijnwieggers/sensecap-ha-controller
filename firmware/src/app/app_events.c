#include "app_events.h"
#include "app_state.h"
#include "app_entities.h"
#include "../ui/ui_manager.h"
#include "../ha/ha_client.h"
#include "../platform/storage.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "app_events";

void app_events_handle(const ha_event_t *evt) {
    switch (evt->type) {
        case HA_EVT_CONNECTED: {
            bool was_disconnected = app_state_is(STATE_DISCONNECTED);
            ESP_LOGI(TAG, "HA verbonden%s",
                     was_disconnected ? " (hersteld)" : "");

            char saved_view[64] = {0};
            if (was_disconnected) {
                storage_get_string("selected_view", saved_view,
                                   sizeof(saved_view), "");
            }

            if (saved_view[0]) {
                /* Herverbinding: direct terug naar de actieve view (US-009).
                   Let op: geen get_views hiernaast — ha_lovelace heeft één
                   pending-slot, een tweede request zou de eerste overschrijven. */
                app_state_set(STATE_ENTITIES_LOADING);
                ha_client_load_view(saved_view);
            } else {
                app_state_set(STATE_VIEW_SELECT);
                ui_manager_show_view_menu();
                ha_client_get_views();
            }
            break;
        }

        case HA_EVT_DISCONNECTED:
            ESP_LOGW(TAG, "HA verbroken");
            app_state_set(STATE_DISCONNECTED);
            ui_manager_show_disconnected();
            break;

        case HA_EVT_VIEWS_LOADED:
            ui_manager_refresh_view_list();
            break;

        case HA_EVT_ENTITIES_LOADED:
            app_state_set(STATE_VIEW_READY);
            ui_manager_show_entities();
            ha_client_get_states();  /* initiële statussen van de view ophalen */
            break;

        case HA_EVT_STATE_CHANGED:
            ui_manager_update_entity(evt);
            break;
    }
}
