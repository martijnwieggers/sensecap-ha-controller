#include "display.h"
#include "board_io.h"
#include "lvgl.h"
#include "driver/gpio.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "display";

#define LCD_H_RES   480
#define LCD_V_RES   480

/* RGB-interface pinnen (SenseCap Indicator, Seeed BSP) */
#define PIN_LCD_VSYNC   17
#define PIN_LCD_HSYNC   16
#define PIN_LCD_DE      18
#define PIN_LCD_PCLK    21
#define PIN_LCD_BL      45   /* backlight, actief hoog */

/* 3-draads init-SPI: CS via IO-expander, klok/data direct op de ESP32 */
#define PIN_SPI_SCK     41
#define PIN_SPI_MOSI    48

/* LVGL tekent in twee kleine buffers in intern DMA-RAM; het volledige
   framebuffer van het RGB-panel zelf staat in PSRAM (fb_in_psram) */
#define DRAW_BUF_LINES  48

static lv_disp_draw_buf_t s_draw_buf;

/* ── ST7701S init via 9-bit software-SPI ─────────────────────────────────
   Gedrag (inclusief extra klokpuls en CS-toggle per woord) is 1-op-1
   overgenomen uit de Seeed factory-firmware — bewezen werkend op dit paneel */

static void spi_sck(int lvl)  { gpio_set_level(PIN_SPI_SCK, lvl); }
static void spi_sdo(int lvl)  { gpio_set_level(PIN_SPI_MOSI, lvl); }
static void spi_cs(int lvl)   { ioexp_set_level(IOEXP_LCD_CS, lvl);
                                esp_rom_delay_us(10); }

static void spi_send9(uint16_t w) {
    for (int n = 0; n < 9; n++) {
        spi_sdo((w & 0x0100) ? 1 : 0);
        w <<= 1;
        spi_sck(1);
        esp_rom_delay_us(10);
        spi_sck(0);
        esp_rom_delay_us(10);
    }
}

static void spi_write_cmd(uint8_t c) {
    spi_cs(0);
    spi_sck(0);
    esp_rom_delay_us(10);

    spi_send9(0x0000);          /* hoge byte (altijd 0) met D/C=0 */
    spi_sck(1);
    esp_rom_delay_us(10);
    spi_sck(0);

    spi_cs(1);
    spi_cs(0);

    spi_send9(c);               /* commando met D/C=0 */
    spi_cs(1);
}

static void spi_write_data(uint8_t d) {
    spi_cs(0);
    spi_sck(0);
    esp_rom_delay_us(10);

    spi_send9(0x0100 | d);      /* D/C=1 */
    spi_sck(1);
    esp_rom_delay_us(10);
    spi_sck(0);
    esp_rom_delay_us(10);

    spi_cs(1);
}

/* Init-sequence: cmd, aantal databytes, data...
   0xFE = pauze (volgende byte = ms), 0xFD = einde */
static const uint8_t st7701s_init_seq[] = {
    /* Command2 BK0 — display- en gamma-instellingen */
    0xFF, 5, 0x77, 0x01, 0x00, 0x00, 0x10,
    0xC0, 2, 0x3B, 0x00,                       /* 480 regels */
    0xC1, 2, 0x0D, 0x02,
    0xC2, 2, 0x31, 0x05,
    0xC7, 1, 0x04,
    0xCD, 1, 0x08,
    0xB0, 16, 0x00, 0x11, 0x18, 0x0E, 0x11, 0x06, 0x07, 0x08,
              0x07, 0x22, 0x04, 0x12, 0x0F, 0xAA, 0x31, 0x18,
    0xB1, 16, 0x00, 0x11, 0x19, 0x0E, 0x12, 0x07, 0x08, 0x08,
              0x08, 0x22, 0x04, 0x11, 0x11, 0xA9, 0x32, 0x18,
    /* Command2 BK1 — spanningsinstellingen */
    0xFF, 5, 0x77, 0x01, 0x00, 0x00, 0x11,
    0xB0, 1, 0x60,
    0xB1, 1, 0x32,
    0xB2, 1, 0x07,
    0xB3, 1, 0x80,
    0xB5, 1, 0x49,
    0xB7, 1, 0x85,
    0xB8, 1, 0x21,
    0xC1, 1, 0x78,
    0xC2, 1, 0x78,
    0xFE, 20,
    0xE0, 3, 0x00, 0x1B, 0x02,
    0xE1, 11, 0x08, 0xA0, 0x00, 0x00, 0x07, 0xA0, 0x00, 0x00,
              0x00, 0x44, 0x44,
    0xE2, 12, 0x11, 0x11, 0x44, 0x44, 0xED, 0xA0, 0x00, 0x00,
              0xEC, 0xA0, 0x00, 0x00,
    0xE3, 4, 0x00, 0x00, 0x11, 0x11,
    0xE4, 2, 0x44, 0x44,
    0xE5, 16, 0x0A, 0xE9, 0xD8, 0xA0, 0x0C, 0xEB, 0xD8, 0xA0,
              0x0E, 0xED, 0xD8, 0xA0, 0x10, 0xEF, 0xD8, 0xA0,
    0xE6, 4, 0x00, 0x00, 0x11, 0x11,
    0xE7, 2, 0x44, 0x44,
    0xE8, 16, 0x09, 0xE8, 0xD8, 0xA0, 0x0B, 0xEA, 0xD8, 0xA0,
              0x0D, 0xEC, 0xD8, 0xA0, 0x0F, 0xEE, 0xD8, 0xA0,
    0xEB, 7, 0x02, 0x00, 0xE4, 0xE4, 0x88, 0x00, 0x40,
    0xEC, 2, 0x3C, 0x00,
    0xED, 16, 0xAB, 0x89, 0x76, 0x54, 0x02, 0xFF, 0xFF, 0xFF,
              0xFF, 0xFF, 0xFF, 0x20, 0x45, 0x67, 0x98, 0xBA,
    0x36, 1, 0x10,
    /* Command2 BK3 */
    0xFF, 5, 0x77, 0x01, 0x00, 0x00, 0x13,
    0xE5, 1, 0xE4,
    /* Terug naar Command1 */
    0xFF, 5, 0x77, 0x01, 0x00, 0x00, 0x00,
    0x3A, 1, 0x60,                             /* RGB666-formaat op de bus */
    0x21, 0,                                   /* inversion on */
    0x11, 0,                                   /* sleep out */
    0xFE, 120,
    0x29, 0,                                   /* display on */
    0xFE, 120,
    0xFD
};

static void st7701s_panel_init(void) {
    /* CS en reset hoog via de expander, klok/data als GPIO-uitgang */
    ioexp_set_level(IOEXP_LCD_CS, 1);
    ioexp_set_level(IOEXP_LCD_RESET, 1);
    ioexp_set_direction(IOEXP_LCD_CS, true);
    ioexp_set_direction(IOEXP_LCD_RESET, true);

    gpio_config_t io_conf = {
        .mode         = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << PIN_SPI_SCK) | (1ULL << PIN_SPI_MOSI),
    };
    gpio_config(&io_conf);
    spi_cs(1);
    spi_sck(1);
    spi_sdo(1);

    const uint8_t *p = st7701s_init_seq;
    while (*p != 0xFD) {
        if (*p == 0xFE) {
            vTaskDelay(pdMS_TO_TICKS(p[1]));
            p += 2;
            continue;
        }
        spi_write_cmd(*p);
        uint8_t n = p[1];
        p += 2;
        for (uint8_t i = 0; i < n; i++) spi_write_data(*p++);
    }

    spi_cs(1);
    spi_sck(1);
    spi_sdo(1);
    ESP_LOGI(TAG, "ST7701S init-sequence verstuurd");
}

static void flush_cb(lv_disp_drv_t *drv, const lv_area_t *area,
                     lv_color_t *color_map) {
    esp_lcd_panel_handle_t panel = (esp_lcd_panel_handle_t)drv->user_data;
    esp_lcd_panel_draw_bitmap(panel, area->x1, area->y1,
                               area->x2 + 1, area->y2 + 1, color_map);
    lv_disp_flush_ready(drv);
}

void display_init(void) {
    board_i2c_init();
    if (ioexp_init() != ESP_OK) {
        ESP_LOGE(TAG, "IO-expander niet bereikbaar — display-init afgebroken");
        return;
    }

    st7701s_panel_init();

    esp_lcd_rgb_panel_config_t panel_config = {
        .clk_src = LCD_CLK_SRC_PLL160M,
        .data_width = 16,
        .psram_trans_align = 64,
        /* Bounce-buffer in intern RAM: DMA leest niet rechtstreeks uit PSRAM,
           waardoor WiFi-/cacheverkeer geen beelddrift meer veroorzaakt; bij een
           underrun lijnt de driver het frame op de eerstvolgende vsync opnieuw uit */
        .bounce_buffer_size_px = LCD_H_RES * 10,
        .disp_gpio_num = GPIO_NUM_NC,
        .pclk_gpio_num = PIN_LCD_PCLK,
        .vsync_gpio_num = PIN_LCD_VSYNC,
        .hsync_gpio_num = PIN_LCD_HSYNC,
        .de_gpio_num = PIN_LCD_DE,
        .data_gpio_nums = {
            /* B0..B4, G0..G5, R0..R4 — volgorde uit de Seeed BSP */
            15, 14, 13, 12, 11,
            10,  9,  8,  7,  6, 5,
             4,  3,  2,  1,  0,
        },
        .timings = {
            .pclk_hz = 18 * 1000 * 1000,
            .h_res = LCD_H_RES,
            .v_res = LCD_V_RES,
            .hsync_back_porch = 50,
            .hsync_front_porch = 10,
            .hsync_pulse_width = 8,
            .vsync_back_porch = 20,
            .vsync_front_porch = 10,
            .vsync_pulse_width = 8,
            .flags.pclk_active_neg = 0,
        },
        .flags.fb_in_psram = 1,
    };

    esp_lcd_panel_handle_t panel = NULL;
    ESP_ERROR_CHECK(esp_lcd_new_rgb_panel(&panel_config, &panel));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel));

    /* Framebuffer zwart maken vóór de backlight aangaat */
    void *fb = NULL;
    if (esp_lcd_rgb_panel_get_frame_buffer(panel, 1, &fb) == ESP_OK && fb) {
        memset(fb, 0, LCD_H_RES * LCD_V_RES * sizeof(lv_color_t));
    }

    gpio_config_t bl_conf = {
        .mode         = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1ULL << PIN_LCD_BL,
    };
    gpio_config(&bl_conf);
    gpio_set_level(PIN_LCD_BL, 1);

    /* LVGL-tekenbuffers: dubbel gebufferd in intern DMA-RAM */
    size_t buf_px = LCD_H_RES * DRAW_BUF_LINES;
    lv_color_t *buf1 = heap_caps_malloc(buf_px * sizeof(lv_color_t),
                                        MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
    lv_color_t *buf2 = heap_caps_malloc(buf_px * sizeof(lv_color_t),
                                        MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
    if (!buf1) {
        ESP_LOGE(TAG, "Geen intern RAM voor LVGL-tekenbuffer");
        return;
    }

    lv_init();
    lv_disp_draw_buf_init(&s_draw_buf, buf1, buf2, buf_px);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res   = LCD_H_RES;
    disp_drv.ver_res   = LCD_V_RES;
    disp_drv.flush_cb  = flush_cb;
    disp_drv.draw_buf  = &s_draw_buf;
    disp_drv.user_data = panel;
    lv_disp_drv_register(&disp_drv);

    ESP_LOGI(TAG, "Display klaar (%dx%d, RGB565 @ 18 MHz)",
             LCD_H_RES, LCD_V_RES);
}
