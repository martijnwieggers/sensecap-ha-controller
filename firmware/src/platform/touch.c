#include "touch.h"
#include "board_io.h"
#include "lvgl.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "touch";

/* FT5X06-adres verschilt per schermrevisie: 0x48 op het GX-paneel (ST7701S),
   0x38 op oudere revisies — we proberen beide */
#define FT5X06_ADDR_GX     0x48
#define FT5X06_ADDR_ALT    0x38

#define FT5X06_TOUCH_POINTS  0x02
#define FT5X06_TOUCH1_XH     0x03

#define LCD_H_RES  480
#define LCD_V_RES  480

static uint8_t s_addr = 0;

static void read_cb(lv_indev_drv_t *drv, lv_indev_data_t *data) {
    static lv_coord_t last_x = 0, last_y = 0;

    uint8_t points = 0;
    if (s_addr == 0 ||
        board_i2c_read_reg8(s_addr, FT5X06_TOUCH_POINTS, &points, 1) != ESP_OK ||
        points == 0 || points > 5) {
        data->point.x = last_x;
        data->point.y = last_y;
        data->state   = LV_INDEV_STATE_RELEASED;
        return;
    }

    uint8_t buf[4];
    if (board_i2c_read_reg8(s_addr, FT5X06_TOUCH1_XH, buf, 4) != ESP_OK) {
        data->state = LV_INDEV_STATE_RELEASED;
        return;
    }

    lv_coord_t x = ((buf[0] & 0x0F) << 8) | buf[1];
    lv_coord_t y = ((buf[2] & 0x0F) << 8) | buf[3];
    if (x >= LCD_H_RES) x = LCD_H_RES - 1;
    if (y >= LCD_V_RES) y = LCD_V_RES - 1;

    last_x = x;
    last_y = y;
    data->point.x = x;
    data->point.y = y;
    data->state   = LV_INDEV_STATE_PRESSED;
}

void touch_init(void) {
    board_i2c_init();

    /* Touch-controller reset via de IO-expander (pin 7, actief laag) */
    if (ioexp_init() == ESP_OK) {
        ioexp_set_level(IOEXP_TP_RESET, 0);
        ioexp_set_direction(IOEXP_TP_RESET, true);
        vTaskDelay(pdMS_TO_TICKS(5));
        ioexp_set_level(IOEXP_TP_RESET, 1);
        vTaskDelay(pdMS_TO_TICKS(100));  /* opstarttijd controller */
    }

    const uint8_t candidates[] = { FT5X06_ADDR_GX, FT5X06_ADDR_ALT };
    for (size_t i = 0; i < sizeof(candidates); i++) {
        if (board_i2c_probe(candidates[i]) == ESP_OK) {
            s_addr = candidates[i];
            break;
        }
    }
    if (s_addr == 0) {
        ESP_LOGE(TAG, "FT5X06 niet gevonden op 0x%02X/0x%02X — touch uitgeschakeld",
                 FT5X06_ADDR_GX, FT5X06_ADDR_ALT);
    } else {
        /* Drempelwaardes zoals de Seeed factory-firmware ze zet */
        board_i2c_write_reg8(s_addr, 0x80, (uint8_t[]){70}, 1);  /* THGROUP */
        board_i2c_write_reg8(s_addr, 0x81, (uint8_t[]){60}, 1);  /* THPEAK */
        board_i2c_write_reg8(s_addr, 0x85, (uint8_t[]){20}, 1);  /* THDIFF */
        board_i2c_write_reg8(s_addr, 0x88, (uint8_t[]){12}, 1);  /* PERIODACTIVE */
        ESP_LOGI(TAG, "Touch klaar (FT5X06 op 0x%02X)", s_addr);
    }

    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type    = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = read_cb;
    lv_indev_drv_register(&indev_drv);
}
