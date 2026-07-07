#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "lvgl.h"

#include "platform/display.h"
#include "platform/touch.h"
#include "platform/storage.h"
#include "platform/wifi.h"
#include "app/app_state.h"
#include "app/app_entities.h"
#include "app/app_events.h"
#include "ha/ha_client.h"
#include "ui/ui_manager.h"

static const char *TAG = "main";

QueueHandle_t ha_event_queue;
QueueHandle_t cmd_queue;

static void task_ui(void *arg) {
    display_init();
    touch_init();
    ui_manager_init();

    ESP_LOGI(TAG, "UI-taak gestart");

    while (1) {
        ha_event_t evt;
        while (xQueueReceive(ha_event_queue, &evt, 0) == pdTRUE) {
            app_events_handle(&evt);
        }
        lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

static void task_ha_ws(void *arg) {
    ESP_LOGI(TAG, "HA WebSocket-taak gestart");
    ha_client_run();  /* blokkeert — interne reconnect-loop */
}

void app_main(void) {
    storage_init();
    app_state_init();
    wifi_init();

    ha_event_queue = xQueueCreate(16, sizeof(ha_event_t));
    cmd_queue      = xQueueCreate(8,  sizeof(ha_cmd_t));

    xTaskCreatePinnedToCore(task_ui,    "ui",    8192, NULL, 5, NULL, 0);
    xTaskCreatePinnedToCore(task_ha_ws, "ha_ws", 6144, NULL, 4, NULL, 1);
}
