# Status — SenseCap Indicator Home Assistant Controller

**Laatste update:** 2026-07-07
**Fase:** Implementatie — US-001 t/m US-007 gerealiseerd

---

## User Story Status

| ID     | Titel                                | Status              | Opmerkingen                                      |
|--------|--------------------------------------|---------------------|--------------------------------------------------|
| US-001 | Initiële configuratie instellen      | **Gerealiseerd** ✅ | Setup-wizard + web-config server + QR-code       |
| US-002 | View selecteren via on-device menu   | **Gerealiseerd** ✅ | Scrollbare lijst, opslaan in NVS                 |
| US-003 | Entiteiten uit HA-view weergeven     | **Gerealiseerd** ✅ | Tileview + swipe-paginering + dots-indicator     |
| US-004 | Schakelaar bedienen (aan/uit)        | **Gerealiseerd** ✅ | Toggle via lv_switch, optimistic update          |
| US-005 | Schuifregelaar bedienen              | **Gerealiseerd** ✅ | Brightness en temperature slider, throttle       |
| US-006 | Sensorwaarden bekijken               | **Gerealiseerd** ✅ | Read-only label, eenheid, binaire sensor         |
| US-007 | Knoppen / scripts / scenes triggeren | **Gerealiseerd** ✅ | WIDGET_BUTTON, debounce 500 ms, visuele feedback; simulator gebuild en getest |
| US-008 | Realtime updates ontvangen vanuit HA | **Gerealiseerd** ✅ | Widget-refresh via registry; `get_states` bij (her)verbinding én na view-laden; firmware én simulator compileren |
| US-009 | Verbindingsfout tonen en herstellen  | **Deels** ⚠️        | Reconnect-lus aanwezig; bij DISCONNECTED wordt nu de statuspagina getoond (besluit 2026-07-07: statuspagina i.p.v. overlay); melding "verbinding herstellen…" ontbreekt nog |
| US-010 | Statuspagina bekijken                | **Deels** ⚠️        | Scherm bestaat; IP-adres, view-naam en Instellingen-knop missen nog |
| US-011 | Airco bedienen: mode en ventilatie   | **Gerealiseerd** ✅ | Climate-rij met twee-regel-layout: hvac-mode- en fan-cycle-knoppen + temperatuur-slider; set_hvac_mode/set_fan_mode services |

---

## Volgende stappen (prioriteits­volgorde)

### 1. US-009 — Verbindingsfout-melding afronden
**Bestanden:** `firmware/src/ui/ui_status.c`, `firmware/src/app/app_events.c`
**Stand:** `ui_manager_show_disconnected()` toont nu de statuspagina (besluit gebruiker 2026-07-07: statuspagina i.p.v. overlay).
**Wat nog mist:**
- Op de statuspagina een expliciete melding "Verbinding verbroken — opnieuw verbinden…" (foutkleur `#EF5350`) zolang `STATE_DISCONNECTED` actief is.
- Bij `HA_EVT_CONNECTED` vanuit disconnected-toestand terug naar het view-menu (werkt al via `app_events_handle`), inclusief herladen van entiteitsstatussen.
- Onderscheid WiFi-probleem vs. HA-probleem in de melding (acceptatiecriterium).
- Simulator: test met `ha_available=0` in `config.ini` (mock stuurt dan `HA_EVT_DISCONNECTED` na 800 ms).

### 2. US-010 — Statuspagina voltooien
**Bestand:** `firmware/src/ui/ui_status.c`
**Wat mist:**
- IP-adres: label `s_lbl_ip` wordt aangemaakt maar nooit gevuld in `refresh_cb` — voeg `esp_netif_get_ip_info()` toe.
- Actieve view-naam: ophalen uit NVS (`selected_view`) en tonen.
- "Instellingen"-knop onderaan die `ui_manager_show_setup()` aanroept.

---

## Architectuur-snapshot

```
firmware/src/
├── main.c                 task_ui (Core 0) + task_ha_ws (Core 1)
├── app/
│   ├── app_state.c/h      toestandsmachine (STATE_BOOT … STATE_DISCONNECTED)
│   ├── app_entities.c/h   entity_t, widget-selectie, build_pages
│   └── app_events.c/h     HA-event → UI-actie koppeling
├── ha/
│   ├── ha_client.c/h      WebSocket + reconnect + service-aanroepen
│   ├── ha_messages.c/h    JSON parsen/samenstellen, auth-flow
│   └── ha_lovelace.c/h    Lovelace-config → view-lijst + entiteiten
├── ui/
│   ├── ui_manager.c/h     schermwisseling (lv_scr_load_anim)
│   ├── ui_setup.c/h       setup-wizard 3 stappen (US-001)
│   ├── ui_view_menu.c/h   scrollbare view-lijst (US-002)
│   ├── ui_entities.c/h    tileview + paginering (US-003)
│   ├── ui_widgets.c/h     toggle / slider / label / actieknop (US-004–007)
│   └── ui_status.c/h      statuspagina (US-010, deels)
└── platform/
    ├── storage.c/h        NVS-abstractie
    ├── display.c/h        LVGL + ST7701S driver init
    ├── touch.c/h          FT5X06 touch-driver
    ├── wifi.c/h           WiFi-scan, connect, events
    └── web_config.c/h     HTTP-server voor HA-URL/token invoer (stap 2 wizard)
```

**Communicatie tussen taken:**
- `ha_event_queue` (ha_ws → ui): CONNECTED, DISCONNECTED, VIEWS_LOADED, ENTITIES_LOADED, STATE_CHANGED
- `cmd_queue` (ui → ha_ws): TOGGLE, SET_BRIGHTNESS, SET_TEMPERATURE, PRESS_BUTTON, LOAD_VIEW, GET_VIEWS

---

## Technische beslissingen (samenvatting)

| Beslissing              | Keuze                                                       |
|-------------------------|-------------------------------------------------------------|
| Build-systeem           | PlatformIO + ESP-IDF v5.1.x                                 |
| UI-framework            | LVGL 8.3.3                                                  |
| WebSocket               | `esp_websocket_client` (native ESP-IDF)                     |
| JSON                    | ArduinoJson v7 (PSRAM-allocator)                            |
| Config-opslag           | NVS namespace `ha-cfg`                                      |
| Verbindingsprotocol     | WS (plaintext) — lokaal thuisnetwerk                        |
| Max entiteiten per view | 30 (5 pagina's × 6 rijen)                                   |
| Schermanimatie          | Slide 200 ms (`LV_SCR_LOAD_ANIM_MOVE_LEFT/RIGHT`)           |
| Kleurthema              | Donker — achtergrond `#1A1A2E`, accent `#4FC3F7`            |
| Factory reset           | GPIO38 5 s ingedrukt bij opstart → NVS wissen               |

---

## Wijzigingslog

| Datum      | Omschrijving                                                                             |
|------------|------------------------------------------------------------------------------------------|
| 2026-06-27 | Project gestart, user stories + technische verkenning + technisch ontwerp                |
| 2026-06-27 | US-001 t/m US-002: WiFi-platform, setup-wizard (web-config), lovelace-parser, view-menu  |
| 2026-06-27 | US-003 t/m US-006: entiteitenscherm, tileview, paginering, toggle, slider, sensorlabel   |
| 2026-06-27 | US-007: WIDGET_BUTTON, script/scene/button-domeinen, ha_client_press_button, debounce    |
| 2026-06-27 | Simulator fix: CMD_PRESS_BUTTON toegevoegd aan enum, mock_ha stub + 3 test-entiteiten    |
| 2026-07-07 | Bugfix leeg scherm bij opstart: LVGL 8.3 `lv_scr_load_anim` op al-actief scherm schuift het scherm off-screen; `load_screen()`-guard in ui_manager. `show_disconnected()` toont nu statuspagina. Mock: `ha_available=0` simuleert onbereikbare HA |
| 2026-07-07 | Setup-wizard: terug-knop in titelbalk (stap 1 en 2) als apparaat al geconfigureerd is — via tandwiel kon je de wizard niet meer verlaten. Terug = timers/web-config opruimen → view-menu |
| 2026-07-07 | US-008 widget-refresh: registry in ui_widgets (entity → widget + waarde-label), `ui_widgets_update()` bij state_changed; sliders tonen live waarde tijdens slepen; slider-updates uit HA worden genegeerd zolang de gebruiker sleept |
| 2026-07-07 | US-008 afgerond: get_states-antwoord wordt geparsed in ha_messages (gefilterd op actieve view, STATE_CHANGED per entiteit, queue-send met timeout tegen burst-verlies); `ha_client_get_states()` na ENTITIES_LOADED zodat een vers geladen view direct echte statussen toont i.p.v. "unavailable". Mock: get_states stuurt alle statussen opnieuw |
| 2026-07-07 | Firmware compileert nu (PlatformIO 6.1.19, `pip install --user`): partitions.csv + sdkconfig.defaults (8MB flash, OPI PSRAM, custom partitietabel); ha_messages/ha_lovelace → .cpp (ArduinoJson is C++) met extern-"C"-guards op gedeelde headers; include/lv_conf.h (RGB565, malloc/PSRAM, esp_timer-tick); esp_websocket_client 1.2.3 gevendord in components/ (PlatformIO's component-manager 1.2.3 begrijpt nieuwe registry-manifests niet); PRIu32-format in ui_status; margin→pad_column in ui_entities, lv_compat.c-stub verwijderd. RAM 13,9%, flash 20,4% |
| 2026-07-07 | US-011 airco: WIDGET_CLIMATE vervangt WIDGET_SLIDER_TEMPERATURE — twee-regel-rij (naam + hvac/fan-cycle-knoppen boven, slider + temp onder); entity_t/ha_event_t dragen fan_mode + hvac_modes/fan_modes-lijsten; parse_state_attrs in ha_messages; ha_messages_call_service_str voor string-service-data; ha_client_set_hvac_mode/set_fan_mode; ui_entities_update neemt nu het hele event aan; mock: climate.airco met mode-lijsten, CMD_SET_HVAC_MODE/CMD_SET_FAN_MODE |
