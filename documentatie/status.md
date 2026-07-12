# Status — SenseCap Indicator Home Assistant Controller

**Laatste update:** 2026-07-12
**Fase:** Hardware-test — **HA-verbinding werkt end-to-end op het apparaat**: wss:// (extern DuckDNS-adres) + TLS + auth + lovelace-parsing + get_states (30 entiteiten in default_view geladen en ververst). Resterend: bediening/visuele checks op hardware

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

### 0. HA-verbinding — OPGELOST (2026-07-12)
Twee oorzaken gevonden en verholpen:
1. **Typefout in geconfigureerd adres** (`duckns` i.p.v. `duckdns`) — zichtbaar gemaakt via nieuwe HA-adres-regel op de statuspagina; gebruiker heeft het adres gecorrigeerd.
2. **ArduinoJson v7-valkuil in de filter-opbouw**: een `JsonVariant`-tussenvariabele (`JsonVariant v = filter["result"]["views"][0]; v["title"] = true;`) is een losgekoppelde null-variant — schrijfacties komen niet in het filterdocument terecht, het filter bleef leeg en het hele antwoord werd weggefilterd ("Geen 'result' veld"). Fix: volledige ketting-toewijzingen in `ha_lovelace.cpp` én `ha_messages.cpp` (daar waren state_changed-attributen en get_states om dezelfde reden stuk).
Geverifieerd op hardware: auth geslaagd, 30 entiteiten geladen voor `default_view`, get_states ververst alle statussen. Sections-dashboards (HA 2024+, cards in `views[].sections[].cards[]`) worden nu ook geparsed.
- Firmware heeft géén mDNS: `homeassistant.local` werkt niet, alleen IP of publiek resolvebare hostname.

### 1. Hardware-test afmaken
Firmware boot sinds 2026-07-11 op het apparaat (bootlog schoon: PSRAM-memtest OK, ST7701S-init verstuurd, FT5X06 gevonden op 0x48, UI-taak rendert, WiFi scant). Nog te verifiëren:
- Visueel: klopt het beeld (kleuren, oriëntatie, geen drift naast WiFi-verkeer)? Bounce-buffer (`480*10` px) staat aan in `display.c`.
- Touch: reageert het scherm, klopt de oriëntatie?
- Factory reset: knop op GPIO38 5 s ingedrukt bij opstart → NVS gewist → setup-wizard.
- WebSocket-flow (auth, lovelace-parsing, get_states) tegen een echte HA-installatie.
- Klein: font mist glyph U+2014 (—) — LVGL-warnings in log; em-dash in UI-teksten vervangen of glyph toevoegen.

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
| 2026-07-11 | Hardware-test iteraties: touch bleek 180° gedraaid gemonteerd → X- en Y-spiegeling in touch.c read_cb; web-config gaf HTTP 431 bij token-POST → `CONFIG_HTTPD_MAX_REQ_HDR_LEN=2048`/`CONFIG_HTTPD_MAX_URI_LEN=1024` (let op: gegenereerde sdkconfig moest verwijderd worden, PlatformIO neemt defaults-wijzigingen niet over); instellingen-flow herzien: wizard via tandwiel springt direct naar stap 2 als WiFi verbonden is (WiFi-knop in titelbalk voor stap 1), huidige ha_url vooringevuld, WiFi-credentials worden niet meer overschreven bij overslaan stap 1; HA-events (connected/disconnected/entities_loaded) forceren geen schermwissel meer zolang de wizard openstaat (`ui_manager_setup_active()`) |
| 2026-07-11 | Eerste geslaagde boot op hardware. Eerste flash gaf een StoreProhibited-bootloop in `esp_psram_init` (vóór app_main); oorzaak: `CONFIG_SPIRAM_FETCH_INSTRUCTIONS`/`CONFIG_SPIRAM_RODATA` (flash-code naar PSRAM verhuizen) — op `=n` gezet in sdkconfig.defaults. Anti-drift wordt nu gedekt door bounce-buffer `480*10` px in display.c + `CONFIG_LCD_RGB_ISR_IRAM_SAFE`/`CONFIG_LCD_RGB_RESTART_IN_VSYNC`. Bootlog daarna schoon: PSRAM 8MB memtest OK, display + touch (0x48) geïnitialiseerd, UI rendert, WiFi scant. Research: crash zit in de ROM-kopieerroutine flash→PSRAM (familie van esp-idf #15263 / ROM-cache-bugs); ESPHome draait XIP op dit board wél, maar met 32B-cachelines — sterkste verdachte is de combinatie `DATA_CACHE_LINE_64B` + ROM-copy. Wil je XIP ooit terug: `DATA_CACHE_LINE_64B` weglaten en IDF ≥5.1.4 gebruiken |
| 2026-07-12 | HA-verbinding end-to-end werkend op hardware. Oorzaak 1: typefout in geconfigureerd adres (`duckns`→`duckdns`), gevonden via werkende seriële log + HA-adres op statuspagina. Oorzaak 2: ArduinoJson v7 — `JsonVariant`-tussenvariabelen bij filter-opbouw zijn losgekoppelde null-variants, filter bleef leeg en filterde het hele antwoord weg; ketting-toewijzingen in ha_lovelace.cpp + ha_messages.cpp. Bonus: sections-dashboards (HA 2024+) worden geparsed (`views[].sections[].cards[]`); diagnose-log toont eerste 200 tekens payload als 'result' ontbreekt. Log bevestigt: auth OK, 30 entiteiten geladen, get_states ververst |
| 2026-07-12 | Diagnose-iteratie HA-verbinding: statuspagina toont nu het geconfigureerde HA-adres én de verbindingsfase (Verbinden.../Authenticeren.../Verbonden/Token ongeldig/Verbroken); nieuw `HA_EVT_AUTH_FAILED`-event (auth_invalid → duidelijke melding op statuspagina i.p.v. alleen log + stille reconnect-lus); token en URL worden getrimd bij opslaan (web-formulier én touchscreen — geplakte `\r\n` achter het token gaf auth_invalid); token-leesbuffer 256→512 (kleiner dan opslagkant liet nvs_get_str falen → lege token verstuurd); `tools/serial_log.py` asserteert DTR/RTS niet meer (pyserial resette het board bij elke herverbinding, log bleef daardoor leeg); gegenereerde sdkconfig verwijderd zodat `CONFIG_MBEDTLS_EXTERNAL_MEM_ALLOC` (TLS-heap naar PSRAM) zeker in de build zit |
| 2026-07-07 | US-009 + US-010 afgerond: statuspagina herzien (titelbalk + terug-knop, RSSI-balkjes 0–4, IP via wifi_get_ip, HA-status groen/rood o.b.v. app_state, actieve view-naam via lovelace-lijst, Instellingen-knop); foutmelding bij STATE_DISCONNECTED met WiFi/HA-onderscheid + directe refresh via ui_status_refresh(); na herverbinding automatisch terug naar opgeslagen view (geen get_views ernaast — één pending-slot in ha_lovelace); mock: bij ha_available=0 komt HA na 8 s weer online (herstel-test); sim-stub esp_wifi geeft nu nep-AP terug. Herstel-scenario end-to-end geverifieerd in simulator |
