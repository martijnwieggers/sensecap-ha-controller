#include "display.h"
#include "lvgl.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_log.h"
#include "esp_heap_caps.h"

static const char *TAG = "display";

#define LCD_H_RES   480
#define LCD_V_RES   480

static lv_disp_draw_buf_t s_draw_buf;
static lv_color_t        *s_buf1 = NULL;

static void flush_cb(lv_disp_drv_t *drv, const lv_area_t *area,
                     lv_color_t *color_map) {
    esp_lcd_panel_handle_t panel = (esp_lcd_panel_handle_t)drv->user_data;
    esp_lcd_panel_draw_bitmap(panel, area->x1, area->y1,
                               area->x2 + 1, area->y2 + 1, color_map);
    lv_disp_flush_ready(drv);
}

void display_init(void) {
    /* Framebuffer in PSRAM — intern SRAM is te klein voor 480x480 */
    s_buf1 = heap_caps_malloc(LCD_H_RES * LCD_V_RES * sizeof(lv_color_t),
                              MALLOC_CAP_SPIRAM);
    if (!s_buf1) {
        ESP_LOGE(TAG, "PSRAM toewijzing mislukt — OPI PSRAM ingeschakeld?");
        return;
    }

    /* TODO: ST7701S RGB panel initialisatie via esp_lcd_panel_rgb_new() */
    /* TODO: FT5X06 touch initialisatie via I2C */

    lv_init();
    lv_disp_draw_buf_init(&s_draw_buf, s_buf1, NULL, LCD_H_RES * LCD_V_RES);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res   = LCD_H_RES;
    disp_drv.ver_res   = LCD_V_RES;
    disp_drv.flush_cb  = flush_cb;
    disp_drv.draw_buf  = &s_draw_buf;
    lv_disp_drv_register(&disp_drv);

    ESP_LOGI(TAG, "Display klaar (%dx%d)", LCD_H_RES, LCD_V_RES);
}
