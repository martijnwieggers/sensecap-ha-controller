#include "app_events.h"
#include "app_state.h"
#include "app_entities.h"
#include "../ui/ui_manager.h"
#include "../ui/ui_status.h"
#include "../ui/ui_view_settings.h"
#include "../ha/ha_client.h"
#include "../platform/storage.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "app_events";

void app_events_handle(const ha_event_t *evt) {
    /* Zolang de gebruiker in de setup-wizard/instellingen zit mag een
       HA-event het scherm niet overnemen; na sluiten of herstart
       synchroniseert de normale flow alles opnieuw. */
    bool setup_open = ui_manager_setup_active();

    switch (evt->type) {
        case HA_EVT_CONNECTED: {
            if (setup_open) {
                ESP_LOGI(TAG, "HA verbonden (genegeerd: instellingen open)");
                break;
            }
            bool was_disconnected = app_state_is(STATE_DISCONNECTED);
            ESP_LOGI(TAG, "HA verbonden%s",
                     was_disconnected ? " (hersteld)" : "");
            ui_status_set_auth_failed(false);

            char saved_view[64] = {0};
            if (was_disconnected) {
                storage_get_string("selected_view", saved_view,
                                   sizeof(saved_view), "");
            }

            char single_view[64] = {0};
            if (saved_view[0]) {
                /* Herverbinding: direct terug naar de actieve view (US-009).
                   Let op: geen get_views hiernaast — ha_lovelace heeft één
                   pending-slot, een tweede request zou de eerste overschrijven. */
                app_state_set(STATE_ENTITIES_LOADING);
                ha_client_load_view(saved_view);
            } else if (view_settings_enabled_count(single_view,
                                                   sizeof(single_view)) == 1) {
                /* Precies één actieve view (US-012): keuzemenu overslaan */
                ESP_LOGI(TAG, "Eén actieve view ('%s') — menu overgeslagen",
                         single_view);
                storage_set_string("selected_view", single_view);
                app_state_set(STATE_ENTITIES_LOADING);
                ha_client_load_view(single_view);
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
            if (!setup_open) ui_manager_show_disconnected();
            break;

        case HA_EVT_AUTH_FAILED:
            ESP_LOGE(TAG, "HA-token geweigerd");
            ui_status_set_auth_failed(true);
            app_state_set(STATE_DISCONNECTED);
            if (!setup_open) ui_manager_show_disconnected();
            break;

        case HA_EVT_VIEWS_LOADED:
            ui_manager_refresh_view_list();
            break;

        case HA_EVT_ENTITIES_LOADED:
            if (setup_open) break;
            app_state_set(STATE_VIEW_READY);
            ui_manager_show_entities();
            ha_client_get_states();  /* initiële statussen van de view ophalen */
            break;

        case HA_EVT_STATE_CHANGED:
            ui_manager_update_entity(evt);
            break;
    }
}
