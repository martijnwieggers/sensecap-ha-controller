#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_system.h"
#include "driver/gpio.h"
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

/* Scherm-timeout (US-013): backlight uit na een instelbare periode zonder
   aanraking; wekken gebeurt in touch.c. 0 = nooit uitschakelen. */
#define SCREEN_TIMEOUT_DEFAULT_S 30

static void screen_timeout_cb(lv_timer_t *t) {
    (void)t;
    uint32_t timeout_s = storage_get_u32("screen_timeout",
                                         SCREEN_TIMEOUT_DEFAULT_S);
    /* Tijdens de setup-wizard/instellingen blijft het scherm aan */
    if (timeout_s == 0 || ui_manager_setup_active()) {
        if (!display_backlight_on()) display_set_backlight(true);
        return;
    }
    if (lv_disp_get_inactive_time(NULL) >= timeout_s * 1000) {
        if (display_backlight_on()) display_set_backlight(false);
    }
}

static void task_ui(void *arg) {
    display_init();
    touch_init();
    ui_manager_init();
    lv_timer_create(screen_timeout_cb, 1000, NULL);

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

/* Factory reset: gebruikersknop (GPIO38, actief laag) 5 s ingedrukt houden
   bij het opstarten → NVS wissen en herstarten in de setup-wizard */
#define FACTORY_RESET_GPIO     GPIO_NUM_38
#define FACTORY_RESET_HOLD_MS  5000

static void factory_reset_check(void) {
    gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << FACTORY_RESET_GPIO,
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
    };
    gpio_config(&cfg);
    vTaskDelay(pdMS_TO_TICKS(20));

    if (gpio_get_level(FACTORY_RESET_GPIO) != 0) return;

    ESP_LOGW(TAG, "Knop ingedrukt — %d s vasthouden voor factory reset",
             FACTORY_RESET_HOLD_MS / 1000);
    for (int t = 0; t < FACTORY_RESET_HOLD_MS; t += 100) {
        vTaskDelay(pdMS_TO_TICKS(100));
        if (gpio_get_level(FACTORY_RESET_GPIO) != 0) {
            ESP_LOGI(TAG, "Knop losgelaten — factory reset geannuleerd");
            return;
        }
    }

    ESP_LOGW(TAG, "Factory reset: NVS wissen en herstarten");
    storage_clear_all();
    esp_restart();
}

static void task_ha_ws(void *arg) {
    ESP_LOGI(TAG, "HA WebSocket-taak gestart");
    ha_client_run();  /* blokkeert — interne reconnect-loop */
}

void app_main(void) {
    storage_init();
    factory_reset_check();
    app_state_init();
    wifi_init();

    ha_event_queue = xQueueCreate(16, sizeof(ha_event_t));
    cmd_queue      = xQueueCreate(8,  sizeof(ha_cmd_t));

    xTaskCreatePinnedToCore(task_ui,    "ui",    8192, NULL, 5, NULL, 0);
    xTaskCreatePinnedToCore(task_ha_ws, "ha_ws", 6144, NULL, 4, NULL, 1);
}
