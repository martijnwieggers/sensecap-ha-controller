#pragma once

/* Initialiseert het ST7701S scherm en registreert de LVGL display driver.
   Na deze aanroep is lv_disp_get_default() geldig. */
void display_init(void);
