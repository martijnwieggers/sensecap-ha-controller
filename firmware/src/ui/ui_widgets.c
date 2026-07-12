#include "ui_widgets.h"
#include "../ha/ha_client.h"
#include <stdio.h>

/* Kleurpalet (donker thema) */
#define CLR_BG       0x1A1A2E
#define CLR_ACCENT   0x4FC3F7
#define CLR_INACTIVE 0x424242
#define CLR_TEXT     0xE0E0E0
#define CLR_SUBTEXT  0x9E9E9E

/* ================================================================
   Widget-registry: per entiteit het LVGL-object en waarde-label
   onthouden zodat een state_changed gericht bijgewerkt kan worden.
   ================================================================ */

typedef struct {
    const entity_t *ent;
    lv_obj_t       *widget;    /* switch / slider / waarde-label */
    lv_obj_t       *val_lbl;   /* label naast slider, NULL indien n.v.t. */
    lv_obj_t       *name_lbl;  /* naam-label links (friendly_name-update) */
    lv_obj_t       *slider;    /* light: helderheids-slider (US-014) */
    lv_obj_t       *mode_lbl;  /* climate: label in hvac-mode-knop */
    lv_obj_t       *fan_lbl;   /* climate: label in fan-knop */
} widget_ref_t;

static widget_ref_t s_refs[MAX_ENTITIES];
static int          s_ref_count = 0;

void ui_widgets_reset_refs(void) {
    s_ref_count = 0;
}

static void register_ref(const entity_t *e, lv_obj_t *widget,
                         lv_obj_t *val_lbl) {
    if (s_ref_count >= MAX_ENTITIES) return;
    s_refs[s_ref_count].ent      = e;
    s_refs[s_ref_count].widget   = widget;
    s_refs[s_ref_count].val_lbl  = val_lbl;
    s_refs[s_ref_count].name_lbl = NULL;
    s_refs[s_ref_count].slider   = NULL;
    s_refs[s_ref_count].mode_lbl = NULL;
    s_refs[s_ref_count].fan_lbl  = NULL;
    s_ref_count++;
}

/* Koppel het naam-label aan de zojuist geregistreerde ref van deze entiteit,
   zodat een friendly_name uit HA de naam live kan bijwerken */
static void register_name_label(const entity_t *e, lv_obj_t *name_lbl) {
    for (int i = s_ref_count - 1; i >= 0; i--) {
        if (s_refs[i].ent == e) {
            s_refs[i].name_lbl = name_lbl;
            return;
        }
    }
}

static void register_climate_ref(const entity_t *e, lv_obj_t *slider,
                                 lv_obj_t *val_lbl, lv_obj_t *mode_lbl,
                                 lv_obj_t *fan_lbl) {
    if (s_ref_count >= MAX_ENTITIES) return;
    register_ref(e, slider, val_lbl);
    s_refs[s_ref_count - 1].mode_lbl = mode_lbl;
    s_refs[s_ref_count - 1].fan_lbl  = fan_lbl;
}

static widget_ref_t *find_ref_by_widget(lv_obj_t *widget) {
    for (int i = 0; i < s_ref_count; i++) {
        if (s_refs[i].widget == widget ||
            s_refs[i].slider == widget) return &s_refs[i];
    }
    return NULL;
}

/* ---- Label-formattering (gedeeld door render en update) ---- */

static void set_brightness_label(lv_obj_t *lbl, int pct) {
    char buf[8];
    snprintf(buf, sizeof(buf), "%d%%", pct);
    lv_label_set_text(lbl, buf);
}

static void set_temperature_label(lv_obj_t *lbl, float temp) {
    char buf[12];
    snprintf(buf, sizeof(buf), "%.1f°C", temp);
    lv_label_set_text(lbl, buf);
}

static void set_state_label(lv_obj_t *lbl, const entity_t *e) {
    char buf[48];
    if (e->unit[0]) {
        snprintf(buf, sizeof(buf), "%s %s", e->state, e->unit);
    } else {
        snprintf(buf, sizeof(buf), "%s", e->state);
    }
    lv_label_set_text(lbl, buf);
}

static float slider_pct_to_temp(const entity_t *e, int pct) {
    float min = e->temp_min > 0 ? e->temp_min : 15.0f;
    float max = e->temp_max > 0 ? e->temp_max : 30.0f;
    return min + (max - min) * pct / 100.0f;
}

/* ================================================================
   Event-callbacks
   ================================================================ */

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

/* Live label-update tijdens het slepen (lokaal, geen HA-aanroep) */
static void brightness_drag_cb(lv_event_t *e) {
    lv_obj_t *slider = lv_event_get_target(e);
    widget_ref_t *ref = find_ref_by_widget(slider);
    if (ref && ref->val_lbl) {
        set_brightness_label(ref->val_lbl, lv_slider_get_value(slider));
    }
}

static void temperature_drag_cb(lv_event_t *e) {
    lv_obj_t *slider = lv_event_get_target(e);
    widget_ref_t *ref = find_ref_by_widget(slider);
    if (ref && ref->val_lbl) {
        set_temperature_label(ref->val_lbl,
            slider_pct_to_temp(ref->ent, lv_slider_get_value(slider)));
    }
}

void ui_widgets_render_row(lv_obj_t *row, entity_t *e) {
    /* Climate heeft een eigen twee-regel-layout inclusief naam (US-011) */
    if (e->widget == WIDGET_CLIMATE) {
        ui_widgets_render_climate(row, e);
        return;
    }
    /* Light: schakelaar + conditionele helderheids-slider (US-014) */
    if (e->widget == WIDGET_LIGHT) {
        ui_widgets_render_light(row, e);
        return;
    }

    /* Naam links */
    lv_obj_t *name_lbl = lv_label_create(row);
    lv_label_set_text(name_lbl, e->name);
    lv_label_set_long_mode(name_lbl, LV_LABEL_LONG_DOT);
    lv_obj_set_width(name_lbl, 200);
    lv_obj_set_style_text_color(name_lbl, lv_color_hex(CLR_TEXT), 0);
    lv_obj_align(name_lbl, LV_ALIGN_LEFT_MID, 8, 0);

    /* Widget rechts op basis van type */
    switch (e->widget) {
        case WIDGET_TOGGLE:            ui_widgets_render_toggle(row, e);            break;
        case WIDGET_SLIDER_BRIGHTNESS: ui_widgets_render_slider_brightness(row, e); break;
        case WIDGET_BUTTON:            ui_widgets_render_action_button(row, e);     break;
        case WIDGET_LABEL:
        default:                       ui_widgets_render_label(row, e);             break;
    }
    register_name_label(e, name_lbl);
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
    register_ref(e, sw, NULL);
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
    set_brightness_label(val_lbl, (int)e->brightness_pct);
    lv_obj_set_style_text_color(val_lbl, lv_color_hex(CLR_SUBTEXT), 0);
    lv_obj_align(val_lbl, LV_ALIGN_RIGHT_MID, -8, 0);

    register_ref(e, slider, val_lbl);
    lv_obj_add_event_cb(slider, brightness_drag_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(slider, brightness_event_cb, LV_EVENT_RELEASED, e);
}

/* ---- Light: schakelaar + helderheids-slider bij aan+dimbaar (US-014) ---- */

/* Slider alleen tonen als de lamp aan én dimbaar is; naam en schakelaar
   verhuizen dan naar de bovenste regel (zelfde twee-regel-idee als climate) */
static void light_apply_layout(widget_ref_t *ref, const entity_t *e) {
    bool show_slider = e->dimmable && strcmp(e->state, "on") == 0;

    if (show_slider) {
        if (ref->name_lbl) lv_obj_align(ref->name_lbl, LV_ALIGN_TOP_LEFT, 8, 9);
        lv_obj_align(ref->widget, LV_ALIGN_TOP_RIGHT, -8, 6);
        lv_obj_clear_flag(ref->slider,  LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ref->val_lbl, LV_OBJ_FLAG_HIDDEN);
    } else {
        if (ref->name_lbl) lv_obj_align(ref->name_lbl, LV_ALIGN_LEFT_MID, 8, 0);
        lv_obj_align(ref->widget, LV_ALIGN_RIGHT_MID, -8, 0);
        lv_obj_add_flag(ref->slider,  LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ref->val_lbl, LV_OBJ_FLAG_HIDDEN);
    }
}

void ui_widgets_render_light(lv_obj_t *row, entity_t *e) {
    lv_obj_t *name_lbl = lv_label_create(row);
    lv_label_set_text(name_lbl, e->name);
    lv_label_set_long_mode(name_lbl, LV_LABEL_LONG_DOT);
    lv_obj_set_width(name_lbl, 240);
    lv_obj_set_style_text_color(name_lbl, lv_color_hex(CLR_TEXT), 0);

    lv_obj_t *sw = lv_switch_create(row);
    lv_obj_set_style_bg_color(sw, lv_color_hex(CLR_INACTIVE), LV_PART_MAIN);
    lv_obj_set_style_bg_color(sw, lv_color_hex(CLR_ACCENT), LV_PART_INDICATOR);
    if (strcmp(e->state, "on") == 0) {
        lv_obj_add_state(sw, LV_STATE_CHECKED);
    }
    lv_obj_add_event_cb(sw, toggle_event_cb, LV_EVENT_VALUE_CHANGED, e);

    int pct = e->brightness_pct >= 0.0f ? (int)e->brightness_pct : 0;

    lv_obj_t *slider = lv_slider_create(row);
    lv_obj_set_size(slider, 300, 10);
    lv_obj_align(slider, LV_ALIGN_BOTTOM_LEFT, 12, -14);
    lv_slider_set_range(slider, 0, 100);
    lv_slider_set_value(slider, pct, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(slider, lv_color_hex(CLR_ACCENT), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(slider, lv_color_hex(CLR_ACCENT), LV_PART_KNOB);

    lv_obj_t *val_lbl = lv_label_create(row);
    set_brightness_label(val_lbl, pct);
    lv_obj_set_style_text_color(val_lbl, lv_color_hex(CLR_SUBTEXT), 0);
    lv_obj_align(val_lbl, LV_ALIGN_BOTTOM_RIGHT, -8, -10);

    lv_obj_add_event_cb(slider, brightness_drag_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(slider, brightness_event_cb, LV_EVENT_RELEASED, e);

    register_ref(e, sw, val_lbl);
    s_refs[s_ref_count - 1].slider = slider;
    register_name_label(e, name_lbl);
    light_apply_layout(&s_refs[s_ref_count - 1], e);
}

/* ================================================================
   Keuzepopup voor hvac-mode en ventilatiestand (US-015)

   Modale overlay als kind van het actieve scherm: dimt de achtergrond,
   vangt alle aanraking af en wordt bij een schermwissel automatisch
   mee-opgeruimd (LV_EVENT_DELETE reset de pointer).
   ================================================================ */

static lv_obj_t *s_popup;        /* overlay; NULL = geen popup open */
static entity_t *s_popup_ent;
static bool      s_popup_is_fan;

static void mode_popup_close(void) {
    if (s_popup) lv_obj_del(s_popup);
}

static void popup_deleted_cb(lv_event_t *ev) {
    (void)ev;
    s_popup = NULL;
}

static void popup_overlay_cb(lv_event_t *ev) {
    (void)ev;
    mode_popup_close();  /* tik buiten het paneel = sluiten zonder wijziging */
}

static void popup_option_cb(lv_event_t *ev) {
    const char *mode = (const char *)lv_event_get_user_data(ev);
    entity_t   *e    = s_popup_ent;
    if (e && mode) {
        if (s_popup_is_fan) ha_client_set_fan_mode(e->entity_id, mode);
        else                ha_client_set_hvac_mode(e->entity_id, mode);
    }
    mode_popup_close();  /* knoplabel volgt via de state-update uit HA */
}

static void mode_popup_open(entity_t *e, bool is_fan) {
    int count = is_fan ? e->fan_mode_count : e->hvac_mode_count;
    if (count <= 0) return;  /* geen modes bekend: knop blijft neutraal */
    const char *current = is_fan ? e->fan_mode : e->state;

    mode_popup_close();
    s_popup_ent    = e;
    s_popup_is_fan = is_fan;

    lv_obj_t *overlay = lv_obj_create(lv_scr_act());
    s_popup = overlay;
    lv_obj_set_pos(overlay, 0, 0);
    lv_obj_set_size(overlay, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_bg_color(overlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(overlay, LV_OPA_60, 0);
    lv_obj_set_style_border_width(overlay, 0, 0);
    lv_obj_set_style_radius(overlay, 0, 0);
    lv_obj_set_style_pad_all(overlay, 0, 0);
    lv_obj_clear_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);
    /* Vegen op de popup mag geen paginawissel triggeren: gesture stopt
       hier in plaats van door te bubbelen naar het scherm (ui_entities) */
    lv_obj_clear_flag(overlay, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_add_event_cb(overlay, popup_overlay_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(overlay, popup_deleted_cb, LV_EVENT_DELETE, NULL);

    /* Paneel: titel + verticale modeslijst; bij veel standen scrollt de lijst */
    int list_h = count * 54 - 8;   /* rijen van 46 px + 8 px tussenruimte */
    if (list_h > 330) list_h = 330;
    int panel_h = 24 + 34 + list_h;

    lv_obj_t *panel = lv_obj_create(overlay);
    lv_obj_set_size(panel, 300, panel_h);
    lv_obj_center(panel);
    lv_obj_set_style_bg_color(panel, lv_color_hex(0x2A2A4A), 0);
    lv_obj_set_style_border_width(panel, 0, 0);
    lv_obj_set_style_radius(panel, 8, 0);
    lv_obj_set_style_pad_all(panel, 12, 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(panel);
    lv_label_set_text(title, is_fan ? "Ventilatie" : "Mode");
    lv_obj_set_style_text_color(title, lv_color_hex(CLR_TEXT), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 0);

    lv_obj_t *list = lv_obj_create(panel);
    lv_obj_set_size(list, lv_pct(100), list_h);
    lv_obj_align(list, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_opa(list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(list, 0, 0);
    lv_obj_set_style_pad_all(list, 0, 0);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(list, 8, 0);

    for (int i = 0; i < count; i++) {
        const char *mode = is_fan ? e->fan_modes[i] : e->hvac_modes[i];
        bool active = strcmp(mode, current) == 0;

        lv_obj_t *btn = lv_btn_create(list);
        lv_obj_set_size(btn, lv_pct(100), 46);
        lv_obj_set_style_bg_color(btn,
            lv_color_hex(active ? CLR_ACCENT : 0x1F1F3A), 0);
        lv_obj_set_style_radius(btn, 6, 0);
        lv_obj_set_style_border_width(btn, 0, 0);

        lv_obj_t *lbl = lv_label_create(btn);
        lv_label_set_text(lbl, mode);
        lv_obj_set_style_text_color(lbl,
            lv_color_hex(active ? CLR_BG : CLR_TEXT), 0);
        lv_obj_center(lbl);

        lv_obj_add_event_cb(btn, popup_option_cb, LV_EVENT_CLICKED,
                            (void *)mode);
    }
}

/* ---- Climate: mode- en fan-knoppen openen de keuzepopup (US-011/015) ---- */

static void hvac_mode_btn_cb(lv_event_t *ev) {
    entity_t *e = (entity_t *)lv_event_get_user_data(ev);
    if (!e) return;
    mode_popup_open(e, false);
}

static void fan_mode_btn_cb(lv_event_t *ev) {
    entity_t *e = (entity_t *)lv_event_get_user_data(ev);
    if (!e) return;
    mode_popup_open(e, true);
}

/* Compacte cycle-knop in de bovenste regel van een climate-rij */
static lv_obj_t *make_cycle_btn(lv_obj_t *row, lv_coord_t x_ofs,
                                const char *text, lv_event_cb_t cb,
                                entity_t *e) {
    lv_obj_t *btn = lv_btn_create(row);
    lv_obj_set_size(btn, 100, 26);
    lv_obj_align(btn, LV_ALIGN_TOP_RIGHT, x_ofs, 5);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x2A2A4A), 0);
    lv_obj_set_style_radius(btn, 4, 0);
    lv_obj_set_style_border_width(btn, 0, 0);

    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, text);
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_DOT);
    lv_obj_set_width(lbl, 92);
    lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(lbl, lv_color_hex(CLR_TEXT), 0);
    lv_obj_center(lbl);

    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, e);
    return lbl;  /* label wordt geregistreerd voor updates */
}

void ui_widgets_render_climate(lv_obj_t *row, entity_t *e) {
    /* Bovenste regel: naam + mode-knop + fan-knop */
    lv_obj_t *name_lbl = lv_label_create(row);
    lv_label_set_text(name_lbl, e->name);
    lv_label_set_long_mode(name_lbl, LV_LABEL_LONG_DOT);
    lv_obj_set_width(name_lbl, 240);
    lv_obj_set_style_text_color(name_lbl, lv_color_hex(CLR_TEXT), 0);
    lv_obj_align(name_lbl, LV_ALIGN_TOP_LEFT, 8, 9);

    lv_obj_t *fan_lbl  = make_cycle_btn(row, -8,   e->fan_mode[0] ? e->fan_mode : "-",
                                        fan_mode_btn_cb, e);
    lv_obj_t *mode_lbl = make_cycle_btn(row, -114, e->state[0] ? e->state : "-",
                                        hvac_mode_btn_cb, e);

    /* Onderste regel: temperatuur-slider + waarde */
    float min = e->temp_min > 0 ? e->temp_min : 15.0f;
    float max = e->temp_max > 0 ? e->temp_max : 30.0f;
    int pct   = (int)((e->temperature - min) / (max - min) * 100);

    lv_obj_t *slider = lv_slider_create(row);
    lv_obj_set_size(slider, 300, 10);
    lv_obj_align(slider, LV_ALIGN_BOTTOM_LEFT, 12, -14);
    lv_slider_set_range(slider, 0, 100);
    lv_slider_set_value(slider, pct, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(slider, lv_color_hex(CLR_ACCENT), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(slider, lv_color_hex(CLR_ACCENT), LV_PART_KNOB);

    lv_obj_t *val_lbl = lv_label_create(row);
    set_temperature_label(val_lbl, e->temperature);
    lv_obj_set_style_text_color(val_lbl, lv_color_hex(CLR_SUBTEXT), 0);
    lv_obj_align(val_lbl, LV_ALIGN_BOTTOM_RIGHT, -8, -10);

    register_climate_ref(e, slider, val_lbl, mode_lbl, fan_lbl);
    register_name_label(e, name_lbl);
    lv_obj_add_event_cb(slider, temperature_drag_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(slider, temperature_event_cb, LV_EVENT_RELEASED, e);
}

void ui_widgets_render_label(lv_obj_t *parent, entity_t *e) {
    lv_obj_t *lbl = lv_label_create(parent);
    set_state_label(lbl, e);
    lv_obj_set_style_text_color(lbl, lv_color_hex(CLR_SUBTEXT), 0);
    lv_obj_align(lbl, LV_ALIGN_RIGHT_MID, -8, 0);
    register_ref(e, lbl, NULL);
}

/* ---- Actieknop (script / scene / button) ---- */

static void action_btn_restore_cb(lv_timer_t *t) {
    lv_obj_t *btn = (lv_obj_t *)t->user_data;
    lv_obj_set_style_bg_color(btn, lv_color_hex(CLR_ACCENT), 0);
    lv_obj_t *lbl = lv_obj_get_child(btn, 0);
    if (lbl) lv_label_set_text(lbl, LV_SYMBOL_PLAY);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
}

static void action_btn_event_cb(lv_event_t *e) {
    entity_t  *ent = (entity_t *)lv_event_get_user_data(e);
    lv_obj_t  *btn = lv_event_get_target(e);

    /* Visuele feedback: donkerder accent voor ≥ 500 ms */
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x0288D1), 0);
    lv_obj_t *lbl = lv_obj_get_child(btn, 0);
    if (lbl) lv_label_set_text(lbl, LV_SYMBOL_REFRESH);

    /* Debounce: knop vergrendelen tot timer afloopt */
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_CLICKABLE);
    lv_timer_t *t = lv_timer_create(action_btn_restore_cb, 500, btn);
    lv_timer_set_repeat_count(t, 1);

    ha_client_press_button(ent->entity_id);
}

void ui_widgets_render_action_button(lv_obj_t *parent, entity_t *e) {
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_size(btn, 120, 44);
    lv_obj_align(btn, LV_ALIGN_RIGHT_MID, -8, 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(CLR_ACCENT), 0);
    lv_obj_set_style_radius(btn, 6, 0);
    lv_obj_set_style_border_width(btn, 0, 0);

    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, LV_SYMBOL_PLAY);
    lv_obj_set_style_text_color(lbl, lv_color_hex(CLR_BG), 0);
    lv_obj_center(lbl);

    lv_obj_add_event_cb(btn, action_btn_event_cb, LV_EVENT_CLICKED, e);
    /* Ref zodat ook actieknop-rijen een naam-update kunnen krijgen */
    register_ref(e, btn, NULL);
}

/* ================================================================
   Gerichte widget-update na state_changed (US-008)
   ================================================================ */

void ui_widgets_update(const entity_t *e) {
    for (int i = 0; i < s_ref_count; i++) {
        if (s_refs[i].ent != e) continue;
        lv_obj_t *w   = s_refs[i].widget;
        lv_obj_t *lbl = s_refs[i].val_lbl;

        /* Naam kan wijzigen zodra HA de friendly_name meelevert */
        if (s_refs[i].name_lbl) {
            lv_label_set_text(s_refs[i].name_lbl, e->name);
        }

        switch (e->widget) {
            case WIDGET_TOGGLE:
                if (strcmp(e->state, "on") == 0) {
                    lv_obj_add_state(w, LV_STATE_CHECKED);
                } else {
                    lv_obj_clear_state(w, LV_STATE_CHECKED);
                }
                break;

            case WIDGET_SLIDER_BRIGHTNESS:
                /* Niet ingrijpen zolang de gebruiker de slider vasthoudt */
                if (lv_obj_has_state(w, LV_STATE_PRESSED)) break;
                lv_slider_set_value(w, (int)e->brightness_pct, LV_ANIM_OFF);
                if (lbl) set_brightness_label(lbl, (int)e->brightness_pct);
                break;

            case WIDGET_LIGHT: {
                if (strcmp(e->state, "on") == 0) {
                    lv_obj_add_state(w, LV_STATE_CHECKED);
                } else {
                    lv_obj_clear_state(w, LV_STATE_CHECKED);
                }
                lv_obj_t *sl = s_refs[i].slider;
                if (sl && !lv_obj_has_state(sl, LV_STATE_PRESSED)) {
                    int pct = e->brightness_pct >= 0.0f
                              ? (int)e->brightness_pct : 0;
                    lv_slider_set_value(sl, pct, LV_ANIM_OFF);
                    if (lbl) set_brightness_label(lbl, pct);
                }
                light_apply_layout(&s_refs[i], e);
                break;
            }

            case WIDGET_CLIMATE: {
                if (!lv_obj_has_state(w, LV_STATE_PRESSED)) {
                    float min = e->temp_min > 0 ? e->temp_min : 15.0f;
                    float max = e->temp_max > 0 ? e->temp_max : 30.0f;
                    int pct = (int)((e->temperature - min) / (max - min) * 100);
                    lv_slider_set_value(w, pct, LV_ANIM_OFF);
                    if (lbl) set_temperature_label(lbl, e->temperature);
                }
                if (s_refs[i].mode_lbl) {
                    lv_label_set_text(s_refs[i].mode_lbl,
                                      e->state[0] ? e->state : "-");
                }
                if (s_refs[i].fan_lbl) {
                    lv_label_set_text(s_refs[i].fan_lbl,
                                      e->fan_mode[0] ? e->fan_mode : "-");
                }
                break;
            }

            case WIDGET_LABEL:
                set_state_label(w, e);
                break;

            case WIDGET_BUTTON:
            default:
                break;  /* actieknop heeft geen toestand */
        }
        return;
    }
}
