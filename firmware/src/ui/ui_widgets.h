#pragma once
#include "lvgl.h"
#include "../app/app_entities.h"

/* Rendert de juiste widget rechts in een rij op basis van e->widget */
void ui_widgets_render_row(lv_obj_t *row, entity_t *e);

/* Widget-specifieke renderers */
void ui_widgets_render_toggle(lv_obj_t *parent, entity_t *e);
void ui_widgets_render_slider_brightness(lv_obj_t *parent, entity_t *e);
void ui_widgets_render_light(lv_obj_t *row, entity_t *e);
void ui_widgets_render_climate(lv_obj_t *row, entity_t *e);
void ui_widgets_render_label(lv_obj_t *parent, entity_t *e);
void ui_widgets_render_action_button(lv_obj_t *parent, entity_t *e);

/* Widget-registry: renderers registreren hun LVGL-objecten per entiteit,
   zodat een state_changed gericht bijgewerkt kan worden zonder herrender. */
void ui_widgets_reset_refs(void);            /* aanroepen vóór (her)opbouw scherm */
void ui_widgets_update(const entity_t *e);   /* widget bijwerken na state_changed */
