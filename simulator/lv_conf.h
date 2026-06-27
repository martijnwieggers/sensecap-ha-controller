#if 1  /* Schakel dit blok in (1) of uit (0) */

#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

/* ─── Kleurdiepte ────────────────────────────────────────────────────────── */
#define LV_COLOR_DEPTH 32      /* 32-bit op pc (SDL2); apparaat gebruikt 16 */

/* ─── Schermresolutie ────────────────────────────────────────────────────── */
#define LV_HOR_RES_MAX 480
#define LV_VER_RES_MAX 480

/* ─── Geheugen ───────────────────────────────────────────────────────────── */
#define LV_MEM_CUSTOM 0
#define LV_MEM_SIZE   (512 * 1024U)   /* 512 KB voor simulator */

/* ─── HAL tick ───────────────────────────────────────────────────────────── */
/* Tick wordt handmatig aangeroepen via lv_tick_inc(5) in de hoofdlus */
#define LV_TICK_CUSTOM 0

/* ─── Logging ────────────────────────────────────────────────────────────── */
#define LV_USE_LOG 1
#define LV_LOG_LEVEL LV_LOG_LEVEL_INFO
#define LV_LOG_PRINTF 1

/* ─── Widgets inschakelen ────────────────────────────────────────────────── */
#define LV_USE_ARC        1
#define LV_USE_BAR        1
#define LV_USE_BTN        1
#define LV_USE_BTNMATRIX  1
#define LV_USE_CANVAS     0
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
#define LV_FONT_DEFAULT &lv_font_montserrat_14

/* ─── Animatie ───────────────────────────────────────────────────────────── */
#define LV_USE_ANIMATION 1

/* ─── Overig ─────────────────────────────────────────────────────────────── */
#define LV_USE_PERF_MONITOR 0
#define LV_USE_MEM_MONITOR  0

#endif /* LV_CONF_H */
#endif /* Schakelaar */
