#include "ui_setup.h"
#include "../platform/storage.h"
#include "../ui/ui_manager.h"
#include "lvgl.h"

/* TODO: Volledige setup-wizard implementatie (US-001)
   Stap 1: WiFi scan + selectie + wachtwoord
   Stap 2: HA URL + token invoer
   Stap 3: Test verbinding + opslaan */

lv_obj_t *ui_setup_create(void) {
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x1A1A2E), 0);

    lv_obj_t *lbl = lv_label_create(screen);
    lv_label_set_text(lbl, "Setup — Home Assistant Controller");
    lv_obj_set_style_text_color(lbl, lv_color_hex(0xE0E0E0), 0);
    lv_obj_align(lbl, LV_ALIGN_CENTER, 0, -40);

    lv_obj_t *hint = lv_label_create(screen);
    lv_label_set_text(hint, "TODO: WiFi selectie, HA URL en token invoer");
    lv_obj_set_style_text_color(hint, lv_color_hex(0x9E9E9E), 0);
    lv_obj_align(hint, LV_ALIGN_CENTER, 0, 0);

    return screen;
}
