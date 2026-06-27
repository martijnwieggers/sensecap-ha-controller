#include "ui_widgets.h"
#include "../ha/ha_client.h"
#include <stdio.h>

/* Kleurpalet (donker thema) */
#define CLR_BG       0x1A1A2E
#define CLR_ACCENT   0x4FC3F7
#define CLR_INACTIVE 0x424242
#define CLR_TEXT     0xE0E0E0
#define CLR_SUBTEXT  0x9E9E9E

static void toggle_event_cb(lv_event_t *e) {
    entity_t *ent = (entity_t *)lv_event_get_user_data(e);
    if (!ent) return;
    ha_client_toggle(ent->entity_id);
}

static void brightness_event_cb(lv_event_t *e) {
    entity_t *ent = (entity_t *)lv_event_get_user_data(e);
    lv_obj_t *slider = lv_event_get_target(e);
    ha_client_set_brightness(ent->entity_id, (float)lv_slider_get_value(slider));
}

static void temperature_event_cb(lv_event_t *e) {
    entity_t *ent = (entity_t *)lv_event_get_user_data(e);
    lv_obj_t *slider = lv_event_get_target(e);
    float temp = ent->temp_min + (ent->temp_max - ent->temp_min)
                 * lv_slider_get_value(slider) / 100.0f;
    ha_client_set_temperature(ent->entity_id, temp);
}

void ui_widgets_render_row(lv_obj_t *row, entity_t *e) {
    /* Naam links */
    lv_obj_t *name_lbl = lv_label_create(row);
    lv_label_set_text(name_lbl, e->name);
    lv_label_set_long_mode(name_lbl, LV_LABEL_LONG_DOT);
    lv_obj_set_width(name_lbl, 200);
    lv_obj_set_style_text_color(name_lbl, lv_color_hex(CLR_TEXT), 0);
    lv_obj_align(name_lbl, LV_ALIGN_LEFT_MID, 8, 0);

    /* Widget rechts op basis van type */
    switch (e->widget) {
        case WIDGET_TOGGLE:            ui_widgets_render_toggle(row, e);             break;
        case WIDGET_SLIDER_BRIGHTNESS: ui_widgets_render_slider_brightness(row, e);  break;
        case WIDGET_SLIDER_TEMPERATURE:ui_widgets_render_slider_temperature(row, e); break;
        case WIDGET_LABEL:
        default:                       ui_widgets_render_label(row, e);              break;
    }
}

void ui_widgets_render_toggle(lv_obj_t *parent, entity_t *e) {
    lv_obj_t *sw = lv_switch_create(parent);
    lv_obj_align(sw, LV_ALIGN_RIGHT_MID, -8, 0);
    lv_obj_set_style_bg_color(sw, lv_color_hex(CLR_INACTIVE), LV_PART_MAIN);
    lv_obj_set_style_bg_color(sw, lv_color_hex(CLR_ACCENT), LV_PART_INDICATOR);

    if (strcmp(e->state, "on") == 0) {
        lv_obj_add_state(sw, LV_STATE_CHECKED);
    }
    lv_obj_add_event_cb(sw, toggle_event_cb, LV_EVENT_VALUE_CHANGED, e);
}

void ui_widgets_render_slider_brightness(lv_obj_t *parent, entity_t *e) {
    lv_obj_t *slider = lv_slider_create(parent);
    lv_obj_set_size(slider, 180, 10);
    lv_obj_align(slider, LV_ALIGN_RIGHT_MID, -60, 0);
    lv_slider_set_range(slider, 0, 100);
    lv_slider_set_value(slider, (int)e->brightness_pct, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(slider, lv_color_hex(CLR_ACCENT), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(slider, lv_color_hex(CLR_ACCENT), LV_PART_KNOB);

    lv_obj_t *val_lbl = lv_label_create(parent);
    char buf[8];
    snprintf(buf, sizeof(buf), "%d%%", (int)e->brightness_pct);
    lv_label_set_text(val_lbl, buf);
    lv_obj_set_style_text_color(val_lbl, lv_color_hex(CLR_SUBTEXT), 0);
    lv_obj_align(val_lbl, LV_ALIGN_RIGHT_MID, -8, 0);

    lv_obj_add_event_cb(slider, brightness_event_cb, LV_EVENT_RELEASED, e);
}

void ui_widgets_render_slider_temperature(lv_obj_t *parent, entity_t *e) {
    float min = e->temp_min > 0 ? e->temp_min : 15.0f;
    float max = e->temp_max > 0 ? e->temp_max : 30.0f;
    int pct   = (int)((e->temperature - min) / (max - min) * 100);

    lv_obj_t *slider = lv_slider_create(parent);
    lv_obj_set_size(slider, 160, 10);
    lv_obj_align(slider, LV_ALIGN_RIGHT_MID, -70, 0);
    lv_slider_set_range(slider, 0, 100);
    lv_slider_set_value(slider, pct, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(slider, lv_color_hex(CLR_ACCENT), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(slider, lv_color_hex(CLR_ACCENT), LV_PART_KNOB);

    lv_obj_t *val_lbl = lv_label_create(parent);
    char buf[12];
    snprintf(buf, sizeof(buf), "%.1f°C", e->temperature);
    lv_label_set_text(val_lbl, buf);
    lv_obj_set_style_text_color(val_lbl, lv_color_hex(CLR_SUBTEXT), 0);
    lv_obj_align(val_lbl, LV_ALIGN_RIGHT_MID, -8, 0);

    lv_obj_add_event_cb(slider, temperature_event_cb, LV_EVENT_RELEASED, e);
}

void ui_widgets_render_label(lv_obj_t *parent, entity_t *e) {
    char buf[48];
    if (e->unit[0]) {
        snprintf(buf, sizeof(buf), "%s %s", e->state, e->unit);
    } else {
        snprintf(buf, sizeof(buf), "%s", e->state);
    }
    lv_obj_t *lbl = lv_label_create(parent);
    lv_label_set_text(lbl, buf);
    lv_obj_set_style_text_color(lbl, lv_color_hex(CLR_SUBTEXT), 0);
    lv_obj_align(lbl, LV_ALIGN_RIGHT_MID, -8, 0);
}
