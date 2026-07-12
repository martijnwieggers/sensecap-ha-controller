#pragma once
#include "lvgl.h"

lv_obj_t *ui_status_create(void);

/* Direct verversen (o.a. bij tonen na verbindingsverlies) */
void ui_status_refresh(void);

/* HA wees het token af (auth_invalid): statuspagina toont dan
   "Token ongeldig" tot de eerstvolgende geslaagde verbinding */
void ui_status_set_auth_failed(bool failed);
