#pragma once
#include "lvgl.h"

lv_obj_t *ui_entities_create(void);
void      ui_entities_update(const char *entity_id, const char *state,
                              float brightness_pct, float temperature);
