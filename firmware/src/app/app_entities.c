#include "app_entities.h"
#include <string.h>
#include <stdio.h>

/* Vervangt UTF-8 em-dash/en-dash (U+2014/U+2013) door '-': de ingebakken
   Montserrat-font mist die glyphs en LVGL logt anders een warning per teken.
   Toepassen op alle teksten die uit HA komen en op het scherm belanden. */
void entities_sanitize_label(char *s) {
    char *r = s, *w = s;
    while (*r) {
        if ((unsigned char)r[0] == 0xE2 && (unsigned char)r[1] == 0x80 &&
            ((unsigned char)r[2] == 0x93 || (unsigned char)r[2] == 0x94)) {
            *w++ = '-';
            r += 3;
        } else {
            *w++ = *r++;
        }
    }
    *w = '\0';
}

entity_domain_t entities_parse_domain(const char *entity_id) {
    if (strncmp(entity_id, "switch.",        7)  == 0) return DOMAIN_SWITCH;
    if (strncmp(entity_id, "light.",         6)  == 0) return DOMAIN_LIGHT;
    if (strncmp(entity_id, "climate.",       8)  == 0) return DOMAIN_CLIMATE;
    if (strncmp(entity_id, "sensor.",        7)  == 0) return DOMAIN_SENSOR;
    if (strncmp(entity_id, "binary_sensor.", 14) == 0) return DOMAIN_BINARY_SENSOR;
    if (strncmp(entity_id, "input_boolean.", 14) == 0) return DOMAIN_INPUT_BOOLEAN;
    if (strncmp(entity_id, "script.",        7)  == 0) return DOMAIN_SCRIPT;
    if (strncmp(entity_id, "scene.",         6)  == 0) return DOMAIN_SCENE;
    if (strncmp(entity_id, "button.",        7)  == 0) return DOMAIN_BUTTON;
    if (strncmp(entity_id, "automation.",   11)  == 0) return DOMAIN_AUTOMATION;
    return DOMAIN_UNKNOWN;
}

widget_type_t entities_resolve_widget(const entity_t *e) {
    switch (e->domain) {
        case DOMAIN_SWITCH:
        case DOMAIN_INPUT_BOOLEAN:
        case DOMAIN_AUTOMATION:
            return WIDGET_TOGGLE;
        case DOMAIN_LIGHT:
            /* Schakelaar + slider-bij-aan-en-dimbaar in één widget (US-014) */
            return WIDGET_LIGHT;
        case DOMAIN_CLIMATE:
            return WIDGET_CLIMATE;
        case DOMAIN_SCRIPT:
        case DOMAIN_SCENE:
        case DOMAIN_BUTTON:
            return WIDGET_BUTTON;
        case DOMAIN_SENSOR:
        case DOMAIN_BINARY_SENSOR:
        default:
            return WIDGET_LABEL;
    }
}

void entities_build_pages(view_model_t *vm, entity_t *arr, int count) {
    if (count > MAX_ENTITIES) count = MAX_ENTITIES;
    vm->total_entities = count;
    vm->page_count     = (count + ENTITIES_PER_PAGE - 1) / ENTITIES_PER_PAGE;

    for (int i = 0; i < count; i++) {
        int page = i / ENTITIES_PER_PAGE;
        int slot = i % ENTITIES_PER_PAGE;
        vm->pages[page].entities[slot] = &arr[i];
        vm->pages[page].count = slot + 1;
    }
}

entity_t *entities_find(view_model_t *vm, const char *entity_id) {
    for (int p = 0; p < vm->page_count; p++) {
        for (int s = 0; s < vm->pages[p].count; s++) {
            entity_t *e = vm->pages[p].entities[s];
            if (e && strcmp(e->entity_id, entity_id) == 0) return e;
        }
    }
    return NULL;
}
