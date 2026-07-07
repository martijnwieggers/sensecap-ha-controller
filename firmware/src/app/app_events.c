#include "app_events.h"
#include "app_state.h"
#include "app_entities.h"
#include "../ui/ui_manager.h"
#include "../ha/ha_client.h"
#include "esp_log.h"

static const char *TAG = "app_events";

void app_events_handle(const ha_event_t *evt) {
    switch (evt->type) {
        case HA_EVT_CONNECTED:
            ESP_LOGI(TAG, "HA verbonden");
            app_state_set(STATE_VIEW_SELECT);
            ui_manager_show_view_menu();
            ha_client_get_views();  /* altijd views ophalen bij (her)verbinding */
            break;

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
