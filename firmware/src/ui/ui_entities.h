#pragma once
#include "lvgl.h"
#include "../app/app_events.h"

lv_obj_t *ui_entities_create(void);
void      ui_entities_update(const ha_event_t *evt);
