#include "ui_view_menu.h"
#include "../ha/ha_client.h"
#include "lvgl.h"

/* TODO: Volledige view-selectie implementatie (US-002)
   Lijst van HA-views ophalen en weergeven als scrollbare lijst */

static lv_obj_t *s_list = NULL;

lv_obj_t *ui_view_menu_create(void) {
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x1A1A2E), 0);

    lv_obj_t *title = lv_label_create(screen);
    lv_label_set_text(title, "Kies een view");
    lv_obj_set_style_text_color(title, lv_color_hex(0xE0E0E0), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 16);

    s_list = lv_list_create(screen);
    lv_obj_set_size(s_list, 440, 380);
    lv_obj_align(s_list, LV_ALIGN_CENTER, 0, 20);
    lv_obj_set_style_bg_color(s_list, lv_color_hex(0x16213E), 0);

    /* Placeholder — wordt gevuld zodra HA views stuurt */
    lv_list_add_text(s_list, "Verbinden met Home Assistant...");

    ha_client_get_views();
    return screen;
}

void ui_view_menu_refresh(void) {
    if (!s_list) return;
    lv_obj_clean(s_list);
    /* TODO: Views toevoegen vanuit ha_lovelace resultaat */
    lv_list_add_text(s_list, "Views geladen — TODO: implementeren");
}
