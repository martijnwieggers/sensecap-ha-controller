#pragma once
#include <stdint.h>
#include <stdbool.h>

#define ENTITIES_PER_PAGE   6
#define MAX_PAGES           5
#define MAX_ENTITIES        (ENTITIES_PER_PAGE * MAX_PAGES)

typedef enum {
    DOMAIN_SWITCH,
    DOMAIN_LIGHT,
    DOMAIN_CLIMATE,
    DOMAIN_SENSOR,
    DOMAIN_BINARY_SENSOR,
    DOMAIN_INPUT_BOOLEAN,
    DOMAIN_UNKNOWN,
} entity_domain_t;

typedef enum {
    WIDGET_TOGGLE,
    WIDGET_SLIDER_BRIGHTNESS,
    WIDGET_SLIDER_TEMPERATURE,
    WIDGET_LABEL,
} widget_type_t;

typedef struct {
    char            entity_id[64];
    char            name[48];
    char            state[32];
    entity_domain_t domain;
    widget_type_t   widget;
    float           brightness_pct;  /* 0–100, -1 = n.v.t. */
    float           temperature;     /* graden C, -1 = n.v.t. */
    float           temp_min;
    float           temp_max;
    char            unit[8];
    bool            available;
} entity_t;

typedef struct {
    entity_t *entities[ENTITIES_PER_PAGE];
    int       count;
} page_t;

typedef struct {
    char  view_path[64];
    char  view_title[48];
    page_t pages[MAX_PAGES];
    int   page_count;
    int   total_entities;
} view_model_t;

entity_domain_t entities_parse_domain(const char *entity_id);
widget_type_t   entities_resolve_widget(const entity_t *e);
void            entities_build_pages(view_model_t *vm, entity_t *arr, int count);
entity_t       *entities_find(view_model_t *vm, const char *entity_id);
