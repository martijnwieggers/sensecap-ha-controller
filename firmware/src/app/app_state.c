#include "app_state.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_log.h"

static const char    *TAG        = "app_state";
static app_state_t    s_state    = STATE_BOOT;
static SemaphoreHandle_t s_mutex = NULL;

void app_state_init(void) {
    s_mutex = xSemaphoreCreateMutex();
    s_state = STATE_BOOT;
}

app_state_t app_state_get(void) {
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    app_state_t st = s_state;
    xSemaphoreGive(s_mutex);
    return st;
}

void app_state_set(app_state_t new_state) {
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    ESP_LOGI(TAG, "State: %d -> %d", s_state, new_state);
    s_state = new_state;
    xSemaphoreGive(s_mutex);
}

bool app_state_is(app_state_t state) {
    return app_state_get() == state;
}
