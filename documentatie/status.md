# Status — SenseCap Indicator Home Assistant Controller

**Laatste update:** 2026-07-08
**Fase:** Implementatie — US-001 t/m US-011 gerealiseerd; hardware-drivers (display/touch) + factory reset geïmplementeerd, wacht op test op echt apparaat

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
| US-009 | Verbindingsfout tonen en herstellen  | **Gerealiseerd** ✅ | Statuspagina met foutmelding (WiFi- vs HA-onderscheid); na herstel automatisch terug naar actieve view + get_states; backoff-lus in ha_client |
| US-010 | Statuspagina bekijken                | **Gerealiseerd** ✅ | Titelbalk + terug-knop, RSSI-balkjes, IP, HA-status (groen/rood), actieve view-naam, uptime, Instellingen-knop; refresh 5 s |
| US-011 | Airco bedienen: mode en ventilatie   | **Gerealiseerd** ✅ | Climate-rij met twee-regel-layout: hvac-mode- en fan-cycle-knoppen + temperatuur-slider; set_hvac_mode/set_fan_mode services |

---

## Volgende stappen (prioriteits­volgorde)

### 1. Test op echte hardware tegen echte HA
De display- (ST7701S) en touch-driver (FT5X06) zijn nu volledig geïmplementeerd en de firmware compileert — maar nog nooit op het apparaat geflasht. Te verifiëren:
- Beeld: init-sequence en RGB-timings zijn 1-op-1 uit de Seeed factory-BSP overgenomen (pclk 18 MHz). Bij beeld-drift/flikkering naast WiFi-verkeer: pclk verlagen naar 12–14 MHz of `bounce_buffer_size_px` (bv. `480*10`) toevoegen aan de RGB-panelconfig in `display.c`.
- Touch: adres-detectie (0x48 GX-paneel / 0x38 fallback) en oriëntatie (rotatie 0 → geen transformatie verwacht).
- Factory reset: knop op GPIO38 5 s ingedrukt bij opstart → NVS gewist → setup-wizard.
- WebSocket-flow (auth, lovelace-parsing, get_states) tegen een echte HA-installatie.

### 2. Klein
- View-menu ververst de view-lijst niet automatisch na herverbinding via saved view (handmatige refresh-knop werkt).

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
    ├── board_io.c/h       gedeelde I2C-bus (SDA=39/SCL=40) + TCA9535 IO-expander (0x20)
    ├── display.c/h        ST7701S init (9-bit SW-SPI via expander-CS) + esp_lcd RGB-panel + LVGL
    ├── touch.c/h          FT5X06 touch-driver (0x48 GX-paneel, 0x38 fallback)
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
| Factory reset           | GPIO38 5 s ingedrukt bij opstart → NVS wissen (geïmplementeerd in main.c) |
| Display-init            | ST7701S via 9-bit software-SPI: CS=expander-pin 4, SCK=GPIO41, MOSI=GPIO48 |
| RGB-panel               | 16-bit parallel, pclk 18 MHz, framebuffer in PSRAM, LVGL dubbel gebufferd 480×48 intern DMA-RAM |
| IO-expander             | TCA9535 op I2C 0x20 (fallback 0x39): LCD_CS=4, LCD_RESET=5, TP_RESET=7 |

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
| 2026-07-08 | Hardware-drivers: `board_io.c/h` nieuw (gedeelde I2C-bus SDA=39/SCL=40 + TCA9535-expander met schaduwregisters geseed uit hardware); `display.c` volledig — ST7701S init-sequence (verbatim uit Seeed factory-BSP) via 9-bit software-SPI (CS=expander-4, SCK=41, MOSI=48), esp_lcd RGB-panel 16-bit @ 18 MHz (porches uit BSP), framebuffer zwart vóór backlight (GPIO45) aan, LVGL dubbele 480×48-tekenbuffer in intern DMA-RAM; `touch.c` volledig — TP-reset via expander-7, FT5X06-probe 0x48/0x38, drempel-config, read_cb met klemming op 480; factory reset GPIO38 (5 s bij boot → storage_clear_all + herstart) in main.c; sdkconfig: cache-line 64B + SPIRAM fetch/rodata tegen RGB-underrun. Firmware compileert (RAM 16,7 %, flash 20,8 %); nog niet op hardware getest |
| 2026-07-07 | US-009 + US-010 afgerond: statuspagina herzien (titelbalk + terug-knop, RSSI-balkjes 0–4, IP via wifi_get_ip, HA-status groen/rood o.b.v. app_state, actieve view-naam via lovelace-lijst, Instellingen-knop); foutmelding bij STATE_DISCONNECTED met WiFi/HA-onderscheid + directe refresh via ui_status_refresh(); na herverbinding automatisch terug naar opgeslagen view (geen get_views ernaast — één pending-slot in ha_lovelace); mock: bij ha_available=0 komt HA na 8 s weer online (herstel-test); sim-stub esp_wifi geeft nu nep-AP terug. Herstel-scenario end-to-end geverifieerd in simulator |
