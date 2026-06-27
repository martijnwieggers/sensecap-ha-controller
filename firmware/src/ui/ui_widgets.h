#pragma once
#include "lvgl.h"
#include "../app/app_entities.h"

/* Rendert de juiste widget rechts in een rij op basis van e->widget */
void ui_widgets_render_row(lv_obj_t *row, entity_t *e);

/* Widget-specifieke renderers */
void ui_widgets_render_toggle(lv_obj_t *parent, entity_t *e);
void ui_widgets_render_slider_brightness(lv_obj_t *parent, entity_t *e);
void ui_widgets_render_slider_temperature(lv_obj_t *parent, entity_t *e);
void ui_widgets_render_label(lv_obj_t *parent, entity_t *e);
