#include "touch.h"
#include "lvgl.h"
#include "driver/i2c.h"
#include "esp_log.h"

static const char *TAG = "touch";

#define I2C_MASTER_NUM   I2C_NUM_0
#define I2C_SDA_PIN      39
#define I2C_SCL_PIN      40
#define FT5X06_ADDR      0x38

static void read_cb(lv_indev_drv_t *drv, lv_indev_data_t *data) {
    /* TODO: FT5X06 registers uitlezen via I2C en x/y coördinaten vullen */
    data->state = LV_INDEV_STATE_RELEASED;
}

void touch_init(void) {
    i2c_config_t cfg = {
        .mode             = I2C_MODE_MASTER,
        .sda_io_num       = I2C_SDA_PIN,
        .scl_io_num       = I2C_SCL_PIN,
        .sda_pullup_en    = GPIO_PULLUP_ENABLE,
        .scl_pullup_en    = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 400000,
    };
    i2c_param_config(I2C_MASTER_NUM, &cfg);
    i2c_driver_install(I2C_MASTER_NUM, cfg.mode, 0, 0, 0);

    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type    = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = read_cb;
    lv_indev_drv_register(&indev_drv);

    ESP_LOGI(TAG, "Touch klaar (FT5X06 op I2C addr 0x%02X)", FT5X06_ADDR);
}
