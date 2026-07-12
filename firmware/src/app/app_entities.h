#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ENTITIES_PER_PAGE   6
#define MAX_PAGES           5
#define MAX_ENTITIES        (ENTITIES_PER_PAGE * MAX_PAGES)

/* Climate: hvac_modes / fan_modes lijsten (US-011) */
#define MAX_MODES           8
#define MODE_STR_LEN        16

typedef enum {
    DOMAIN_SWITCH,
    DOMAIN_LIGHT,
    DOMAIN_CLIMATE,
    DOMAIN_SENSOR,
    DOMAIN_BINARY_SENSOR,
    DOMAIN_INPUT_BOOLEAN,
    DOMAIN_SCRIPT,
    DOMAIN_SCENE,
    DOMAIN_BUTTON,
    DOMAIN_AUTOMATION,
    DOMAIN_UNKNOWN,
} entity_domain_t;

typedef enum {
    WIDGET_TOGGLE,
    WIDGET_SLIDER_BRIGHTNESS,
    WIDGET_CLIMATE,   /* temperatuur-slider + hvac-mode- en fan-cycle-knoppen */
    WIDGET_LABEL,
    WIDGET_BUTTON,
} widget_type_t;

typedef struct {
    char            entity_id[64];
    char            name[48];
    bool            name_custom;     /* naam komt uit de lovelace-kaart (name:)
                                        en mag niet door friendly_name
                                        overschreven worden */
    char            state[32];
    entity_domain_t domain;
    widget_type_t   widget;
    float           brightness_pct;  /* 0–100, -1 = n.v.t. */
    float           temperature;     /* graden C, -1 = n.v.t. */
    float           temp_min;
    float           temp_max;
    char            unit[8];
    bool            available;
    /* Climate (US-011) — state bevat de actuele hvac_mode */
    char            fan_mode[MODE_STR_LEN];
    char            fan_modes[MAX_MODES][MODE_STR_LEN];
    int             fan_mode_count;
    char            hvac_modes[MAX_MODES][MODE_STR_LEN];
    int             hvac_mode_count;
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

#ifdef __cplusplus
}
#endif
