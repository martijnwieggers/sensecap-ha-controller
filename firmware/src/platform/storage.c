#include "storage.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG       = "storage";
static const char *NAMESPACE = "ha-cfg";

void storage_init(void) {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
        err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS wissen en opnieuw initialiseren");
        nvs_flash_erase();
        nvs_flash_init();
    }
    ESP_LOGI(TAG, "NVS klaar");
}

void storage_get_string(const char *key, char *out, size_t len,
                        const char *default_val) {
    nvs_handle_t h;
    if (nvs_open(NAMESPACE, NVS_READONLY, &h) != ESP_OK) {
        strncpy(out, default_val ? default_val : "", len);
        return;
    }
    if (nvs_get_str(h, key, out, &len) != ESP_OK) {
        strncpy(out, default_val ? default_val : "", len);
    }
    nvs_close(h);
}

void storage_set_string(const char *key, const char *value) {
    nvs_handle_t h;
    if (nvs_open(NAMESPACE, NVS_READWRITE, &h) != ESP_OK) return;
    nvs_set_str(h, key, value);
    nvs_commit(h);
    nvs_close(h);
}

uint8_t storage_get_u8(const char *key, uint8_t default_val) {
    nvs_handle_t h;
    uint8_t val = default_val;
    if (nvs_open(NAMESPACE, NVS_READONLY, &h) != ESP_OK) return val;
    nvs_get_u8(h, key, &val);
    nvs_close(h);
    return val;
}

void storage_set_u8(const char *key, uint8_t value) {
    nvs_handle_t h;
    if (nvs_open(NAMESPACE, NVS_READWRITE, &h) != ESP_OK) return;
    nvs_set_u8(h, key, value);
    nvs_commit(h);
    nvs_close(h);
}

uint32_t storage_get_u32(const char *key, uint32_t default_val) {
    nvs_handle_t h;
    uint32_t val = default_val;
    if (nvs_open(NAMESPACE, NVS_READONLY, &h) != ESP_OK) return val;
    nvs_get_u32(h, key, &val);
    nvs_close(h);
    return val;
}

void storage_set_u32(const char *key, uint32_t value) {
    nvs_handle_t h;
    if (nvs_open(NAMESPACE, NVS_READWRITE, &h) != ESP_OK) return;
    nvs_set_u32(h, key, value);
    nvs_commit(h);
    nvs_close(h);
}

void storage_clear_all(void) {
    nvs_handle_t h;
    if (nvs_open(NAMESPACE, NVS_READWRITE, &h) != ESP_OK) return;
    nvs_erase_all(h);
    nvs_commit(h);
    nvs_close(h);
    ESP_LOGI(TAG, "NVS gewist — factory reset");
}
