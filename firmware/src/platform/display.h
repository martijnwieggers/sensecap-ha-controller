#pragma once
#include <stdbool.h>

/* Initialiseert het ST7701S scherm en registreert de LVGL display driver.
   Na deze aanroep is lv_disp_get_default() geldig. */
void display_init(void);

/* Backlight aan/uit (GPIO45) — scherm-timeout US-013. Het RGB-panel en
   LVGL blijven gewoon doorlopen; alleen de verlichting schakelt. */
void display_set_backlight(bool on);
bool display_backlight_on(void);
