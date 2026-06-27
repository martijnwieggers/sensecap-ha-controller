#pragma once
#include <stdint.h>

void     mock_system_init(void);
uint32_t mock_tick_get(void);  /* gebruikt door LV_TICK_CUSTOM_SYS_TIME_EXPR */

/* ESP-IDF API stubs — alleen declaraties zodat gedeelde bronbestanden compileren */
void     esp_log_write(int level, const char *tag, const char *fmt, ...);

#define ESP_LOGI(tag, fmt, ...) printf("[I][%s] " fmt "\n", tag, ##__VA_ARGS__)
#define ESP_LOGW(tag, fmt, ...) printf("[W][%s] " fmt "\n", tag, ##__VA_ARGS__)
#define ESP_LOGE(tag, fmt, ...) printf("[E][%s] " fmt "\n", tag, ##__VA_ARGS__)
