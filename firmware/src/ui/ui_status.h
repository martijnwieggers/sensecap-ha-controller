#pragma once
#include "lvgl.h"

lv_obj_t *ui_status_create(void);

/* Direct verversen (o.a. bij tonen na verbindingsverlies) */
void ui_status_refresh(void);
