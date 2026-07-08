#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "esp_err.h"

/* Gedeelde I2C-bus (SDA=39, SCL=40) en TCA9535 IO-expander (addr 0x20/0x39).
   Op de SenseCap Indicator lopen LCD-CS, LCD-reset en touch-reset via de
   expander — display.c en touch.c gebruiken daarom beide deze module. */

#ifdef __cplusplus
extern "C" {
#endif

/* Expander-pinnen (TCA9535) — nummering uit de Seeed BSP */
#define IOEXP_LCD_CS        4
#define IOEXP_LCD_RESET     5
#define IOEXP_TP_RESET      7
#define IOEXP_RP2040_RESET  8

void      board_i2c_init(void);   /* idempotent */
esp_err_t board_i2c_probe(uint8_t addr);
esp_err_t board_i2c_write_reg8(uint8_t addr, uint8_t reg,
                               const uint8_t *data, size_t len);
esp_err_t board_i2c_read_reg8(uint8_t addr, uint8_t reg,
                              uint8_t *data, size_t len);

esp_err_t ioexp_init(void);       /* idempotent; detecteert 0x20 of 0x39 */
esp_err_t ioexp_set_level(uint8_t pin, bool level);
esp_err_t ioexp_set_direction(uint8_t pin, bool output);

#ifdef __cplusplus
}
#endif
