/* LVGL-configuratie voor de SenseCap Indicator firmware.
   Gespiegeld aan simulator/lv_conf.h — verschillen:
   - kleurdiepte 16 (RGB565, ST7701S) i.p.v. 32 (SDL)
   - geheugen via malloc (PSRAM) i.p.v. statische pool
   - tick via esp_timer i.p.v. handmatige lv_tick_inc */

#if 1  /* Schakel dit blok in (1) of uit (0) */

#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

/* ─── Kleurdiepte ────────────────────────────────────────────────────────── */
#define LV_COLOR_DEPTH 16      /* RGB565 voor ST7701S RGB-panel */

/* ─── Schermresolutie ────────────────────────────────────────────────────── */
#define LV_HOR_RES_MAX 480
#define LV_VER_RES_MAX 480

/* ─── Geheugen ───────────────────────────────────────────────────────────── */
/* malloc/free — met CONFIG_SPIRAM_USE_MALLOC landen grote allocaties in PSRAM */
#define LV_MEM_CUSTOM 1
#define LV_MEM_CUSTOM_INCLUDE <stdlib.h>
#define LV_MEM_CUSTOM_ALLOC   malloc
#define LV_MEM_CUSTOM_FREE    free
#define LV_MEM_CUSTOM_REALLOC realloc

/* ─── HAL tick ───────────────────────────────────────────────────────────── */
#define LV_TICK_CUSTOM 1
#define LV_TICK_CUSTOM_INCLUDE "esp_timer.h"
#define LV_TICK_CUSTOM_SYS_TIME_EXPR ((uint32_t)(esp_timer_get_time() / 1000))

/* ─── Verversing en input ────────────────────────────────────────────────── */
/* Standaard 30 ms (33 fps, 30 ms touch-lag); op 240 MHz + -O2 haalt de
   renderer 15 ms ruim — soepelere animaties en halve input-latentie */
#define LV_DISP_DEF_REFR_PERIOD  15
#define LV_INDEV_DEF_READ_PERIOD 15

/* ─── Logging ────────────────────────────────────────────────────────────── */
#define LV_USE_LOG 1
#define LV_LOG_LEVEL LV_LOG_LEVEL_WARN
#define LV_LOG_PRINTF 1

/* ─── Widgets inschakelen (identiek aan simulator) ───────────────────────── */
#define LV_USE_ARC        1
#define LV_USE_BAR        1
#define LV_USE_BTN        1
#define LV_USE_BTNMATRIX  1
#define LV_USE_CANVAS     1
#define LV_USE_CHECKBOX   1
#define LV_USE_DROPDOWN   1
#define LV_USE_IMG        1
#define LV_USE_LABEL      1
#define LV_USE_LINE       1
#define LV_USE_ROLLER     1
#define LV_USE_SLIDER     1
#define LV_USE_SWITCH     1
#define LV_USE_TEXTAREA   1
#define LV_USE_TABLE      0
#define LV_USE_TILEVIEW   1
#define LV_USE_LIST       1
#define LV_USE_MSGBOX     1
#define LV_USE_SPINBOX    0
#define LV_USE_SPINNER    1
#define LV_USE_TABVIEW    1
#define LV_USE_WIN        0
#define LV_USE_SPAN       0
#define LV_USE_KEYBOARD   1

/* ─── Thema ──────────────────────────────────────────────────────────────── */
#define LV_USE_THEME_DEFAULT 1
#define LV_THEME_DEFAULT_DARK 1

/* ─── Font ───────────────────────────────────────────────────────────────── */
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_24 1
#define LV_FONT_MONTSERRAT_32 1
#define LV_FONT_MONTSERRAT_48 1
#define LV_FONT_DEFAULT &lv_font_montserrat_14

/* ─── Extra widgets ──────────────────────────────────────────────────────── */
#define LV_USE_QRCODE  1

/* ─── Animatie ───────────────────────────────────────────────────────────── */
#define LV_USE_ANIMATION 1

/* ─── Overig ─────────────────────────────────────────────────────────────── */
#define LV_USE_PERF_MONITOR 0
#define LV_USE_MEM_MONITOR  0

#endif /* LV_CONF_H */
#endif /* Schakelaar */
