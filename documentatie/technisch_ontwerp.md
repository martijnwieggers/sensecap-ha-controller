# Technisch Ontwerp — SenseCap Indicator HA Controller

**Datum:** 2026-06-27  
**Versie:** 1.0  
**Status:** Concept  
**Stack:** PlatformIO + ESP-IDF v5.1.x · LVGL 8.3.3 · esp_websocket_client · ArduinoJson v7

---

## 1. Systeemoverzicht

```
┌─────────────────────────────────────────────────────────────────┐
│                      SenseCap Indicator                         │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │                      ESP32-S3                            │   │
│  │                                                          │   │
│  │  ┌─────────────┐   Queue    ┌──────────────────────┐    │   │
│  │  │  task_ui    │◄──────────►│   task_ha_ws         │    │   │
│  │  │  (Core 0)   │  EventGrp  │   (Core 1)           │    │   │
│  │  │             │            │                       │    │   │
│  │  │  LVGL loop  │            │  WebSocket client     │    │   │
│  │  │  Touch input│            │  Auth flow            │    │   │
│  │  │  UI render  │            │  State subscribe      │    │   │
│  │  │  Swipe/page │            │  Call service         │    │   │
│  │  └──────┬──────┘            └──────────┬────────────┘    │   │
│  │         │                              │                  │   │
│  │  ┌──────▼──────┐            ┌──────────▼────────────┐    │   │
│  │  │  PSRAM      │            │  NVS Flash            │    │   │
│  │  │  Framebuffer│            │  WiFi SSID/pass       │    │   │
│  │  │  Entity data│            │  HA URL + token       │    │   │
│  │  │  JSON buffer│            │  Geselecteerde view   │    │   │
│  │  └─────────────┘            └───────────────────────┘    │   │
│  └──────────────────────────────────────────────────────────┘   │
│                              │ WiFi                             │
└──────────────────────────────┼──────────────────────────────────┘
                               │
                    ┌──────────▼──────────┐
                    │   Home Assistant    │
                    │   WebSocket API     │
                    │   ws://host:8123/   │
                    │   api/websocket     │
                    └─────────────────────┘
```

---

## 2. Software-lagen

```
┌──────────────────────────────────────────────────────┐
│  UI-laag          (ui_*.c)                           │
│  Schermen: setup, view-selectie, entiteiten, status  │
├──────────────────────────────────────────────────────┤
│  Applicatielaag   (app_*.c)                          │
│  Entiteitsmodel, paginering, renderen van widgets    │
├──────────────────────────────────────────────────────┤
│  HA-client laag   (ha_client.c / ha_ws.c)            │
│  WebSocket, auth, subscribe, call_service, reconnect │
├──────────────────────────────────────────────────────┤
│  Platformlaag     (config.c, storage.c, display.c)   │
│  NVS, WiFi, LVGL init, touch driver                  │
├──────────────────────────────────────────────────────┤
│  ESP-IDF / LVGL / ArduinoJson                        │
└──────────────────────────────────────────────────────┘
```

---

## 3. FreeRTOS Taakverdeling

### 3.1 Overzicht taken

| Taak          | Core | Stack   | Prioriteit | Verantwoordelijkheid                        |
|---------------|------|---------|------------|---------------------------------------------|
| `task_ui`     | 0    | 8 KB    | 5          | LVGL tick, touch events, scherm renderen    |
| `task_ha_ws`  | 1    | 6 KB    | 4          | WebSocket verbinding, HA-protocol, reconnect|
| `task_wifi`   | 1    | 4 KB    | 6          | WiFi-verbinding, event callbacks            |

LVGL vereist dat alle `lv_*`-aanroepen vanuit dezelfde taak komen (`task_ui`). Communicatie van `task_ha_ws` naar `task_ui` gaat uitsluitend via FreeRTOS-queue of event group — nooit directe LVGL-aanroepen vanuit `task_ha_ws`.

### 3.2 Communicatie tussen taken

```
task_ha_ws ──[ha_event_queue]──► task_ui
task_ui    ──[cmd_queue]───────► task_ha_ws
```

**`ha_event_queue`** — berichten van HA naar UI:

```c
typedef enum {
    HA_EVT_STATE_CHANGED,   // entiteit-staat gewijzigd
    HA_EVT_CONNECTED,       // verbinding + auth geslaagd
    HA_EVT_DISCONNECTED,    // verbinding verbroken
    HA_EVT_VIEWS_LOADED,    // lovelace view-lijst beschikbaar
    HA_EVT_ENTITIES_LOADED, // entiteiten voor geselecteerde view geladen
} ha_event_type_t;

typedef struct {
    ha_event_type_t type;
    char entity_id[64];
    char state[32];
    float brightness_pct;   // 0–100, -1 = niet van toepassing
    float temperature;      // -1 = niet van toepassing
} ha_event_t;
```

**`cmd_queue`** — commando's van UI naar HA-client:

```c
typedef enum {
    CMD_TOGGLE_ENTITY,
    CMD_SET_BRIGHTNESS,
    CMD_SET_TEMPERATURE,
    CMD_PRESS_BUTTON,
    CMD_LOAD_VIEW,
    CMD_GET_VIEWS,
} ha_cmd_type_t;

typedef struct {
    ha_cmd_type_t type;
    char entity_id[64];
    float value;            // dimmer % of temperatuur
    char view_path[64];
} ha_cmd_t;
```

---

## 4. Applicatie-toestandsmachine

```
                   ┌─────────────┐
          Start    │   BOOT      │
       ───────────►│             │
                   └──────┬──────┘
                          │ NVS lezen
                          ▼
                   ┌─────────────┐   Geen config   ┌───────────────┐
                   │  CONFIG     │────────────────►│  SETUP_WIZARD │
                   │  CHECK      │                  │  (US-001)     │
                   └──────┬──────┘◄─────────────────┤               │
                          │ Config OK               └───────────────┘
                          ▼
                   ┌─────────────┐
                   │  WIFI       │
                   │  CONNECTING │◄──── Retry (exponential backoff)
                   └──────┬──────┘
                          │ WiFi OK
                          ▼
                   ┌─────────────┐
                   │  HA_WS      │
                   │  CONNECTING │◄──── Retry (exponential backoff)
                   └──────┬──────┘
                          │ WS open
                          ▼
                   ┌─────────────┐
                   │  HA_AUTH    │
                   └──────┬──────┘
                          │ auth_ok
                          ▼
                   ┌─────────────┐   Geen opgeslagen view  ┌─────────────┐
                   │  VIEW_      │──────────────────────────►│  VIEW_      │
                   │  SELECT     │◄─────────────────────────│  MENU       │
                   │  CHECK      │  Gebruiker kiest view    │  (US-002)   │
                   └──────┬──────┘                          └─────────────┘
                          │ View geselecteerd
                          ▼
                   ┌─────────────┐
                   │  ENTITIES_  │
                   │  LOADING    │
                   └──────┬──────┘
                          │ Entiteiten geladen
                          ▼
                   ┌─────────────┐
                   │  VIEW_READY │◄──── state_changed events (US-008)
                   │  (US-003)   │
                   └──────┬──────┘
                          │ WS verbroken
                          ▼
                   ┌─────────────┐
                   │  DISCONN-   │──── Automatisch terug naar HA_WS_CONNECTING
                   │  ECTED      │     (US-009)
                   │  (US-009)   │
                   └─────────────┘
```

---

## 5. Geheugenindeling

### 5.1 PSRAM (8 MB)

| Segment                   | Grootte    | Toewijzing                              |
|---------------------------|------------|-----------------------------------------|
| LVGL framebuffer (primair)| 460 KB     | `heap_caps_malloc(MALLOC_CAP_SPIRAM)`   |
| LVGL framebuffer (flush)  | 460 KB     | Optioneel tweede buffer voor vloeiend renderen |
| ArduinoJson parse buffer  | 256 KB     | Lovelace-config parsen                  |
| Entiteitsdata array       | ~50 KB     | Max ~200 entiteiten in geheugen         |
| WebSocket ontvangstbuffer | 16 KB      | Grote HA-berichten                      |
| **Totaal**                | **~1.2 MB**| Ruim binnen 8 MB                        |

### 5.2 Intern SRAM (~350 KB vrij)

| Segment                   | Grootte    | Toewijzing                              |
|---------------------------|------------|-----------------------------------------|
| FreeRTOS task stacks      | ~30 KB     | 3 taken × ~8 KB gemiddeld              |
| FreeRTOS kernel           | ~10 KB     | Queues, event groups, mutexes           |
| WiFi stack (ESP-IDF)      | ~60 KB     | Gereserveerd door WiFi-driver           |
| WebSocket client          | ~20 KB     | esp_websocket_client intern             |
| Applicatiecode + globals  | ~30 KB     | Toestandsmachine, kleine buffers        |
| **Totaal**                | **~150 KB**| Past binnen ~350 KB vrij SRAM           |

---

## 6. Entiteitsmodel

### 6.0 Ondersteunde entiteitstypen (versie 1)

| Domein            | Widget               | Versie |
|-------------------|----------------------|--------|
| `switch`          | Toggle               | v1     |
| `input_boolean`   | Toggle               | v1     |
| `light`           | Toggle of Slider     | v1     |
| `sensor`          | Read-only label      | v1     |
| `binary_sensor`   | Read-only label      | v1     |
| `climate`         | Slider + °C-waarde   | v1     |
| `script`          | Actieknop            | v2     |
| `scene`           | Actieknop            | v2     |
| `button`          | Actieknop            | v2     |
| `automation`      | Toggle               | v2     |

Entiteiten met een domein dat niet in de tabel staat worden weergegeven als read-only label (naam + staat tekst). Geen crash, geen skip.

### 6.1 Datastructuren

```c
// Versie 1: ondersteunde domeinen
typedef enum {
    DOMAIN_SWITCH,
    DOMAIN_LIGHT,
    DOMAIN_CLIMATE,
    DOMAIN_SENSOR,
    DOMAIN_BINARY_SENSOR,
    DOMAIN_INPUT_BOOLEAN,
    DOMAIN_UNKNOWN,           // alles wat niet herkend wordt → WIDGET_LABEL
    // v2:
    // DOMAIN_SCRIPT, DOMAIN_SCENE, DOMAIN_BUTTON, DOMAIN_AUTOMATION
} entity_domain_t;

typedef enum {
    WIDGET_TOGGLE,
    WIDGET_SLIDER_BRIGHTNESS,
    WIDGET_SLIDER_TEMPERATURE,
    WIDGET_LABEL,
    // v2: WIDGET_BUTTON
} widget_type_t;

typedef struct {
    char         entity_id[64];
    char         name[48];
    char         state[32];       // "on", "off", "21.5", "unavailable"
    entity_domain_t domain;
    widget_type_t   widget;
    float        brightness_pct;  // 0.0–100.0, -1 = niet beschikbaar
    float        temperature;     // graden Celsius, -1 = niet beschikbaar
    float        temp_min;
    float        temp_max;
    char         unit[8];         // "°C", "%", "lx", etc.
    bool         available;
} entity_t;
```

### 6.2 Widget-selectielogica

```c
widget_type_t resolve_widget(const entity_t *e) {
    switch (e->domain) {
        case DOMAIN_SWITCH:
        case DOMAIN_INPUT_BOOLEAN:
            return WIDGET_TOGGLE;
        case DOMAIN_LIGHT:
            return (e->brightness_pct >= 0) ? WIDGET_SLIDER_BRIGHTNESS
                                             : WIDGET_TOGGLE;
        case DOMAIN_CLIMATE:
            return WIDGET_SLIDER_TEMPERATURE;
        case DOMAIN_SENSOR:
        case DOMAIN_BINARY_SENSOR:
        default:                         // onbekend domein → veilig label
            return WIDGET_LABEL;
    }
}
```

### 6.3 Paginering

```c
#define ENTITIES_PER_PAGE   6
#define MAX_PAGES           5    // max 5 pagina's = 30 entiteiten
#define MAX_ENTITIES        (ENTITIES_PER_PAGE * MAX_PAGES)  // 30

typedef struct {
    entity_t  *entities[ENTITIES_PER_PAGE];
    int        count;            // 1–6
} page_t;

typedef struct {
    char    view_path[64];
    char    view_title[48];
    page_t  pages[MAX_PAGES];
    int     page_count;
    int     total_entities;
} view_model_t;
```

Paginaopbouw (eenmalig na laden van entiteiten):

```c
void build_pages(view_model_t *vm, entity_t *entities, int count) {
    vm->total_entities = count;
    vm->page_count = (count + ENTITIES_PER_PAGE - 1) / ENTITIES_PER_PAGE;
    for (int i = 0; i < count; i++) {
        int page = i / ENTITIES_PER_PAGE;
        int slot = i % ENTITIES_PER_PAGE;
        vm->pages[page].entities[slot] = &entities[i];
        vm->pages[page].count = slot + 1;
    }
}
```

---

## 7. Home Assistant WebSocket Client

### 7.1 Berichtenstroom

```
ESP32                              Home Assistant
  │                                      │
  │──── TCP connect ────────────────────►│
  │◄─── {"type":"auth_required"} ────────│
  │──── {"type":"auth",                  │
  │      "access_token":"..."} ─────────►│
  │◄─── {"type":"auth_ok"} ──────────────│
  │                                      │
  │──── {"id":1,"type":"get_states"} ───►│
  │◄─── {"type":"result","result":[...]} │
  │                                      │
  │──── {"id":2,"type":                  │
  │      "lovelace/config"} ────────────►│
  │◄─── {"type":"result","result":{...}} │
  │                                      │
  │──── {"id":3,"type":                  │
  │      "subscribe_events",             │
  │      "event_type":                   │
  │      "state_changed"} ──────────────►│
  │◄─── {"type":"result","success":true} │
  │                                      │
  │◄─── {"type":"event",                 │
  │      "event":{...}} ─────────────────│  (realtime, US-008)
  │                                      │
  │──── {"id":4,"type":"call_service",   │
  │      "domain":"switch",              │
  │      "service":"turn_on",            │
  │      "target":{"entity_id":"..."}}──►│
  │◄─── {"type":"result","success":true} │
```

### 7.2 Berichtsnummer (ID) beheer

Elk uitgaand bericht heeft een uniek, oplopend `id`. De client houdt een teller bij en slaat pending callbacks op in een tabel:

```c
#define MAX_PENDING_MSGS   16

typedef struct {
    int   id;
    void (*callback)(const cJSON *result, void *ctx);
    void *ctx;
} pending_msg_t;

static pending_msg_t pending[MAX_PENDING_MSGS];
static int msg_id_counter = 1;
```

### 7.3 Reconnect-strategie (US-009)

```
Verbinding verbroken
  │
  ▼
Wacht 5 s  → probeer opnieuw
  │ Mislukt
  ▼
Wacht 10 s → probeer opnieuw
  │ Mislukt
  ▼
Wacht 20 s → probeer opnieuw
  │ Mislukt
  ▼
Wacht 40 s → probeer opnieuw
  │ Mislukt
  ▼
Wacht 60 s → probeer opnieuw (blijft 60 s)
  │ Geslaagd
  ▼
auth_ok → get_states (alle entiteiten verversen) → subscribe_events
```

Implementatie via FreeRTOS `xTaskDelayUntil` in `task_ha_ws`. Geen busy-wait.

### 7.4 Lovelace-config verwerking

1. `lovelace/config` ophalen (volledig dashboard-object)
2. Filter op `views` waar `path === geselecteerd_pad`
3. Verzamel alle `entity_id`s uit `cards[].entity` en `cards[].entities[]`
4. Verwijder duplicaten
5. Match met `get_states` resultaat om namen en actuele staat te vullen
6. Roep `build_pages()` aan

---

## 8. UI-architectuur (LVGL)

### 8.1 Schermhiërarchie

```
lv_scr_act()
└── screen_manager          (beheert actief scherm)
    ├── screen_setup        (US-001 — setup-wizard)
    ├── screen_view_menu    (US-002 — view-selectie)
    ├── screen_entities     (US-003 — entiteiten + swipe)
    └── screen_status       (US-010 — statuspagina)
```

Schermwisseling via `lv_scr_load_anim()` met **slide-animatie** (200 ms, `LV_SCR_LOAD_ANIM_MOVE_LEFT` vooruit, `LV_SCR_LOAD_ANIM_MOVE_RIGHT` terug).

### 8.2 Entiteitenscherm — layout (US-003)

```
┌────────────────────────────────────────────┐  ▲
│  [☰]  Woonkamer                    [ℹ]    │  │ 40 px (titelbar)
├────────────────────────────────────────────┤  │
│  lv_tileview (horizontaal, swipe-enabled)  │  │
│  ┌────────────────────────────────────────┐│  │
│  │ Pagina 1                               ││  │
│  │ ┌────────────────────────────────────┐ ││  │
│  │ │ Plafondlamp          [toggle  ●  ] │ ││  │  6 × 70 px
│  │ ├────────────────────────────────────┤ ││  │  = 420 px
│  │ │ Dimmer woonkamer  [━━━━━●━━━] 72% │ ││  │
│  │ ├────────────────────────────────────┤ ││  │
│  │ │ Temperatuur           21.5 °C      │ ││  │
│  │ ├────────────────────────────────────┤ ││  │
│  │ │ Luchtvochtigheid      58 %         │ ││  │
│  │ ├────────────────────────────────────┤ ││  │
│  │ │ Scene: Filmavond      [START]      │ ││  │
│  │ ├────────────────────────────────────┤ ││  │
│  │ │ Plafondlamp slaapkamer [toggle ○ ] │ ││  │
│  │ └────────────────────────────────────┘ ││  │
│  └────────────────────────────────────────┘│  │
├────────────────────────────────────────────┤  │
│              ●  ○  ○                       │  │ 20 px (dots)
└────────────────────────────────────────────┘  ▼
                                                480 px totaal
```

**Maatvoering:**
- Titelbar: 40 px (view-naam links, ☰-menu en ℹ-status rechts)
- `lv_tileview` hoogte: 420 px (6 × 70 px per rij)
- Paginering dots: 20 px
- Totaal: 40 + 420 + 20 = **480 px** ✓

### 8.3 Swipe-navigatie implementatie

```c
// Tileview aanmaken
lv_obj_t *tv = lv_tileview_create(parent);
lv_obj_set_size(tv, 480, 420);

// Per pagina een tile aanmaken
for (int p = 0; p < vm->page_count; p++) {
    lv_obj_t *tile = lv_tileview_add_tile(tv, p, 0, LV_DIR_HOR);
    render_page(tile, &vm->pages[p]);
}

// Paginering dots bijwerken bij swipe
lv_obj_add_event_cb(tv, on_tile_changed, LV_EVENT_VALUE_CHANGED, dots_obj);
```

### 8.4 Entiteitsrij — widget per type

Elke rij is een `lv_obj_t` van 70 px hoogte met:
- Links: `lv_label` met entiteitnaam (max 24 tekens, afgekapt met "…")
- Rechts: widget op basis van `widget_type_t`

```c
void render_entity_row(lv_obj_t *parent, const entity_t *e) {
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_set_size(row, 480, 70);

    lv_obj_t *name = lv_label_create(row);
    lv_label_set_text(name, e->name);
    lv_obj_align(name, LV_ALIGN_LEFT_MID, 8, 0);

    switch (e->widget) {
        case WIDGET_TOGGLE:
            render_toggle(row, e);  break;
        case WIDGET_SLIDER_BRIGHTNESS:
            render_slider_brightness(row, e);  break;
        case WIDGET_SLIDER_TEMPERATURE:
            render_slider_temperature(row, e);  break;
        case WIDGET_LABEL:
        default:
            render_sensor_label(row, e);  break;
        // v2: case WIDGET_BUTTON: render_action_button(row, e); break;
    }
}
```

### 8.5 Realtime updates

Wanneer `task_ha_ws` een `state_changed` event ontvangt, plaatst het een `ha_event_t` in `ha_event_queue`. `task_ui` leest de queue elke LVGL-tick (5 ms) en roept `entity_update()` aan:

```c
void entity_update(view_model_t *vm, const ha_event_t *evt) {
    // Zoek entiteit op entity_id
    entity_t *e = find_entity(vm, evt->entity_id);
    if (!e) return;

    // Werk data bij
    strncpy(e->state, evt->state, sizeof(e->state));
    e->brightness_pct = evt->brightness_pct;
    e->temperature    = evt->temperature;

    // Werk widget bij als die zichtbaar is
    lv_obj_t *widget = find_widget_obj(e->entity_id);
    if (widget) refresh_widget(widget, e);
    // Niet-zichtbare pagina's: data al bijgewerkt, widget refresh bij swipe
}
```

---

## 9. NVS Configuratieopslag

### 9.1 Sleutel-schema

| NVS namespace | Sleutel          | Type   | Inhoud                         |
|---------------|------------------|--------|--------------------------------|
| `ha-cfg`      | `wifi_ssid`      | string | WiFi netwerknaam               |
| `ha-cfg`      | `wifi_pass`      | string | WiFi wachtwoord                |
| `ha-cfg`      | `ha_url`         | string | `http://192.168.1.10:8123`     |
| `ha-cfg`      | `ha_token`       | string | Long-Lived Access Token        |
| `ha-cfg`      | `view_path`      | string | Geselecteerd dashboard-pad     |
| `ha-cfg`      | `configured`     | u8     | 1 = setup voltooid, 0 = nieuw  |

### 9.2 API-gebruik

```c
// Lezen
Preferences prefs;
prefs.begin("ha-cfg", true);   // read-only
String url = prefs.getString("ha_url", "");
prefs.end();

// Schrijven
prefs.begin("ha-cfg", false);
prefs.putString("ha_url",   url);
prefs.putString("ha_token", token);
prefs.putUChar("configured", 1);
prefs.end();
```

### 9.3 Token-beveiliging

Access token wordt opgeslagen als plaintext string in NVS (geen encryptie in v1). Het token is alleen leesbaar voor iemand met fysieke toegang en een flash-dump tool — aanvaardbaar voor een thuisproject op een privénetwerk. Encryptie (HMAC-NVS) is voorzien voor een eventuele v2.

### 9.4 Factory reset

Ingedrukt houden van de user-knop (GPIO38) gedurende 5 seconden bij opstart wist alle NVS-sleutels en herstart de setup-wizard.

---

## 10. Setup-wizard flow (US-001)

```
┌─────────────────────────────┐
│  Stap 1: WiFi selecteren    │
│  [lijst van netwerken]      │
│  SSID: __________________   │
│  Wachtwoord: ____________   │
│  [Verbinden]                │
└──────────────┬──────────────┘
               │ WiFi OK
┌──────────────▼──────────────┐
│  Stap 2: HA configureren    │
│  URL:   ________________    │
│  Token: ________________    │
│  [Testen & Opslaan]         │
└──────────────┬──────────────┘
               │ auth_ok
┌──────────────▼──────────────┐
│  Stap 3: Voltooid           │
│  Verbonden met HA           │
│  [Doorgaan]                 │
└──────────────┬──────────────┘
               │
               ▼ View-selectiemenu
```

WiFi-netwerken worden gescand via `esp_wifi_scan_start()`. Het on-screen toetsenbord is de LVGL `lv_keyboard` widget gekoppeld aan een `lv_textarea`.

---

## 11. Statusscherm (US-010)

Bereikbaar via ℹ-knop rechts in de titelbar. Toont:

| Veld                  | Bron                              |
|-----------------------|-----------------------------------|
| WiFi SSID             | NVS + `esp_wifi_sta_get_config()` |
| WiFi signaal (RSSI)   | `esp_wifi_sta_get_ap_info()`      |
| IP-adres              | `esp_netif_get_ip_info()`         |
| HA-verbindingsstatus  | Toestandsmachine globale staat    |
| Actieve view          | NVS `view_path`                   |
| Firmware-versie       | Compile-time macro `APP_VERSION`  |
| Uptime                | `esp_timer_get_time() / 1e6`      |

Ververst automatisch elke 5 seconden via een LVGL-timer (`lv_timer_create`).

---

## 12. Bestandsstructuur

```
SeedD1/
├── platformio.ini
├── src/
│   ├── main.c                  ← app_main, taakaanmaak
│   ├── app/
│   │   ├── app_state.c/h       ← globale toestandsmachine
│   │   ├── app_entities.c/h    ← entity_t, widget-selectie, build_pages
│   │   └── app_events.c/h      ← ha_event_t verwerking in UI-taak
│   ├── ha/
│   │   ├── ha_client.c/h       ← WebSocket verbinding, auth, reconnect
│   │   ├── ha_messages.c/h     ← JSON samenstellen en parsen
│   │   └── ha_lovelace.c/h     ← Lovelace-config parsen naar entiteitslijst
│   ├── ui/
│   │   ├── ui_manager.c/h      ← schermwisseling
│   │   ├── ui_setup.c/h        ← setup-wizard (US-001)
│   │   ├── ui_view_menu.c/h    ← view-selectiemenu (US-002)
│   │   ├── ui_entities.c/h     ← entiteitenscherm + paginering (US-003)
│   │   ├── ui_widgets.c/h      ← render_toggle, render_slider, etc.
│   │   └── ui_status.c/h       ← statusscherm (US-010)
│   └── platform/
│       ├── storage.c/h         ← NVS lezen/schrijven
│       ├── display.c/h         ← LVGL init, ST7701S driver
│       └── touch.c/h           ← FT5X06 driver
├── components/
│   ├── lvgl/                   ← LVGL 8.3.3 als ESP-IDF component
│   └── arduinojson/            ← ArduinoJson v7
└── documentatie/
    ├── userstories.md
    ├── technische_verkenning.md
    ├── technisch_ontwerp.md    ← dit document
    └── status.md
```

---

## 13. platformio.ini

```ini
[env:sensecap_indicator]
platform  = espressif32
board     = esp32s3box
framework = espidf

board_build.partitions = default_8MB.csv
board_upload.flash_size = 8MB

build_flags =
    -DBOARD_HAS_PSRAM
    -DARDUINO_USB_MODE=1
    -DLV_CONF_INCLUDE_SIMPLE
    -DAPP_VERSION=\"1.0.0\"

; OPI PSRAM verplicht inschakelen
board_build.arduino.memory_type = qio_opi
```

---

## 14. Ontwerpbeslissingen (vastgesteld)

| Punt                          | Beslissing                                              |
|-------------------------------|---------------------------------------------------------|
| Verbindingsprotocol           | **WS** (plaintext) — lokaal thuisnetwerk, geen TLS      |
| Maximaal aantal pagina's      | **5 pagina's** — max 30 entiteiten per view             |
| Animatie schermwisseling      | **Slide** — 200 ms, `LV_SCR_LOAD_ANIM_MOVE_LEFT/RIGHT` |
| Factory reset                 | **Knop (GPIO38) 5 s ingedrukt bij opstart**             |
| Kleurthema UI                 | **Donker** — donkere achtergrond, lichte tekst          |

---

## 15. UI-kleurpalet (donker thema)

| Element                 | Kleur (hex) | Toepassing                        |
|-------------------------|-------------|-----------------------------------|
| Achtergrond             | `#1A1A2E`   | Schermachtergrond, rijachtergrond |
| Titelbar                | `#16213E`   | Bovenste balk                     |
| Tekst primair           | `#E0E0E0`   | Entiteitnamen, labels             |
| Tekst secundair         | `#9E9E9E`   | Eenheden, statuswaarden           |
| Accent (actief/aan)     | `#4FC3F7`   | Toggle aan, slider knop, knop     |
| Inactief / uit          | `#424242`   | Toggle uit, achtergrond slider    |
| Foutkleur               | `#EF5350`   | Verbindingsfoutmelding            |
| Scheidingslijn          | `#2A2A4A`   | Rijscheiding                      |

---

*Gegenereerd op: 2026-06-27 · Versie 1.1 — beslissingen verwerkt*
