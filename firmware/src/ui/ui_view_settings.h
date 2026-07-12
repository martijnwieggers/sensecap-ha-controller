#pragma once
#include <stdbool.h>
#include <stddef.h>
#include "lvgl.h"

/* View-instellingen (US-012): aanvinken welke HA-views op het apparaat
   gebruikt worden. Opslag in NVS-key "enabled_views" als kommagescheiden
   lijst van view-paden; leeg/afwezig betekent: geen filter, alle views. */

lv_obj_t *ui_view_settings_create(void);

/* Lijst opnieuw opbouwen (na HA_EVT_VIEWS_LOADED) */
void ui_view_settings_refresh(void);

/* True als de view door het filter komt (of als er geen filter is).
   Pure NVS-lezers — geen LVGL, vanuit elke taak aanroepbaar. */
bool view_settings_is_enabled(const char *path);

/* Aantal aangevinkte views; het eerste (of enige) pad wordt naar
   first_out gekopieerd als die niet NULL is. */
int view_settings_enabled_count(char *first_out, size_t out_len);
