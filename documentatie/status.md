# Status — SenseCap Indicator Home Assistant Controller

**Laatste update:** 2026-07-12 (US-015 geïmplementeerd)
**Fase:** Werkend product op hardware — HA-verbinding (wss/TLS), views, bediening, scherm-timeout en performance-optimalisatie allemaal end-to-end geverifieerd. US-015 (keuzepopup climate-modes) is gebouwd en in de simulator end-to-end getest op alle acceptatiecriteria; op hardware geflasht — korte bevestigingstest door gebruiker nog open.

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
| US-012 | Actieve views kiezen in instellingen | **Gerealiseerd** ✅ | Views-knop op statuspagina → checkbox-scherm (NVS `enabled_views`); menu gefilterd; bij 1 actieve view menu overgeslagen bij opstarten |
| US-013 | Scherm automatisch uit (energie)      | **Gerealiseerd** ✅ | Dropdown op statuspagina (Nooit/15s/30s/1m/2m/5m, standaard 30 s, NVS `screen_timeout`); backlight uit bij inactiviteit, wek-tik bedient geen widget |
| US-014 | Helderheids-slider bij dimbare lampen | **Gerealiseerd** ✅ | WIDGET_LIGHT: schakelaar altijd, slider zichtbaar bij aan+dimbaar (`supported_color_modes`); twee-regel-layout, live show/hide bij state-updates |
| US-015 | Keuzepopup climate-mode/ventilatie    | **Gerealiseerd** ✅ | Mode-/fan-knop opent modale popup (overlay + lijst, actuele stand gemarkeerd); alle criteria in simulator geverifieerd; geflasht op hardware |
| US-016 | Verticaal per pagina bladeren + scrollbalk | **Niet gestart** 📋 | Story + implementatieplan goedgekeurd (zie userstories.md); veeg omhoog/omlaag = directe paginawissel, permanente scrollbalk rechts vervangt dots |

---

## Volgende stappen (prioriteits­volgorde)

### 1. US-016 implementeren — verticaal per pagina bladeren met scrollbalk
Story + implementatieplan goedgekeurd (2026-07-12). Kern: in `ui_entities.c` reageert `gesture_cb` voortaan op verticale i.p.v. horizontale veegrichting (directe wissel blijft — geen animatie, dus geen judder); de dots-balk wordt vervangen door een permanent zichtbare verticale scrollbalk rechts (hoogte/positie = actieve pagina in totaal, bijwerken in `show_page()`); vrijgekomen `DOTS_H` gaat naar de rijhoogte. Slider-guard richtingsbewust maken: verticale veeg op een slider moet wisselen zonder HA-commando te sturen.

### 2. US-015 op hardware bevestigen (kort)
Firmware met de popup is geflasht en boot schoon (WiFi + HA verbonden, get_states OK). Nog even op het apparaat zelf tikken: mode-popup openen, stand kiezen, tik-buiten-sluit en veeg-op-popup controleren. In de simulator zijn alle zes acceptatiecriteria al end-to-end geverifieerd (geautomatiseerde kliktest via `tools/sim_drive.ps1`).

### 3. Klein / opruimpunten
- Font mist glyph U+2014 (—): LVGL-warnings in log; em-dash in UI-teksten vervangen of glyph toevoegen.
- View-menu ververst de view-lijst niet automatisch na herverbinding via saved view (handmatige refresh-knop werkt).
- Diagnose-logging in `ha_lovelace.cpp` (per-view titel/pad + payload-head bij ontbrekend result) kan t.z.t. omlaag naar LOGD.
- Overwegen: `#<index>`-paden voor views zonder URL zijn positie-gebonden; advies aan gebruiker was om views in HA een URL te geven.

### Naslag: opgeloste hardware/verbindingsproblemen (2026-07-12)
- **HA-verbinding**: typefout in adres (`duckns`→`duckdns`) + ArduinoJson v7-valkuil (JsonVariant-tussenvariabele = losgekoppelde null-variant → leeg filter → alles weggefilterd). Fix: ketting-toewijzingen in ha_lovelace.cpp + ha_messages.cpp.
- **Geen mDNS**: `homeassistant.local` werkt niet — IP of publiek resolvebare hostname gebruiken.
- **Seriële log**: SenseCap zit op de CH340-COM-poort (nummer kan verschuiven na re-enumeratie; "Serieel USB-apparaat" = RP2040). `tools/serial_log.py` asserteert bewust geen DTR/RTS. Logger stoppen vóór flashen.
- **Judder bij full-screen animaties**: LVGL-render is niet vsync-gekoppeld aan het RGB-panel; daarom alle slide-animaties vervangen door directe wissel.

---

## Architectuur-snapshot

```
firmware/src/
├── main.c                 task_ui (Core 0) + task_ha_ws (Core 1); factory reset GPIO38;
│                          scherm-timeout-timer (US-013, 1 s, leest NVS screen_timeout)
├── app/
│   ├── app_state.c/h      toestandsmachine (STATE_BOOT … STATE_DISCONNECTED)
│   ├── app_entities.c/h   entity_t (incl. dimmable/name_custom), widget-selectie, build_pages
│   └── app_events.c/h     HA-event → UI-actie; 1-actieve-view = menu overslaan (US-012);
│                          HA_EVT_AUTH_FAILED → statuspagina-melding
├── ha/
│   ├── ha_client.c/h      WebSocket + reconnect/backoff + SNTP + CA-bundel (wss) +
│                          fragment-reassembly + service-aanroepen
│   ├── ha_messages.cpp/h  JSON filter-parsen (LET OP: ketting-toewijzingen, geen
│                          JsonVariant-tussenvariabelen!), auth-flow, get_states,
│                          friendly_name/supported_color_modes
│   └── ha_lovelace.cpp/h  Lovelace-config → views + entiteiten; sections-dashboards;
│                          kaartnamen (name:); synthetisch #<index>-pad bij views zonder URL
├── ui/
│   ├── ui_manager.c/h     schermwisseling — directe wissel, GEEN animaties (judder RGB-panel)
│   ├── ui_setup.c/h       setup-wizard 3 stappen (US-001); trim van geplakte invoer
│   ├── ui_view_menu.c/h   view-lijst, gefilterd op enabled_views (US-002/012)
│   ├── ui_view_settings.c/h  checkbox-scherm actieve views → NVS enabled_views (US-012)
│   ├── ui_entities.c/h    gestapelde pagina's + veeg-gesture = directe paginawissel (US-003)
│   ├── ui_widgets.c/h     toggle / light (switch+slider, US-014) / climate / label /
│   │                      actieknop; registry met naam-labels voor live updates;
│   │                      modale keuzepopup hvac-/fan-mode (US-015)
│   └── ui_status.c/h      statuspagina: HA-adres + verbindingsfase, Views/Instellingen-
│                          knoppen, scherm-timeout-dropdown (US-010/012/013)
└── platform/
    ├── storage.c/h        NVS-abstractie (string/u8/u32)
    ├── board_io.c/h       gedeelde I2C-bus (SDA=39/SCL=40, 400 kHz) + TCA9535 (0x20)
    ├── display.c/h        ST7701S-init + esp_lcd RGB-panel + LVGL; backlight-API —
    │                      gekoppeld aan wifi_set_low_latency (scherm aan = WIFI_PS_NONE)
    ├── touch.c/h          FT5X06 (0x48/0x38); wek-tik bij donker scherm wordt opgeslokt
    ├── wifi.c/h           scan/connect/events + wifi_set_low_latency (modem-slaap aan/uit)
    └── web_config.c/h     HTTP-server HA-URL/token (voorinvulling, trim van invoer)
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
| Schermanimatie          | Geen (directe wissel) — full-screen slides oogden schokkerig op het RGB-panel (geen vsync-koppeling); paginering idem via veeg-gesture |
| CPU / compiler          | 240 MHz + -O2 (IDF-defaults waren 160 MHz + -Og — halveerde de UI-snelheid) |
| Core-verdeling          | Core 0 exclusief UI; WiFi-, TCP/IP-, websocket- en HA-taken op core 1     |
| WiFi power save         | Gekoppeld aan backlight: scherm aan = WIFI_PS_NONE (directe HA-respons), scherm uit = MIN_MODEM |
| LVGL-timing             | Refresh + input-polling 15 ms (`LV_DISP_DEF_REFR_PERIOD`/`LV_INDEV_DEF_READ_PERIOD`) |
| NVS-sleutels            | `ha-cfg`: configured, wifi_ssid/pass, ha_url, ha_token, selected_view, enabled_views (csv), screen_timeout (u32 s) |
| Kleurthema              | Donker — achtergrond `#1A1A2E`, accent `#4FC3F7`            |
| Factory reset           | GPIO38 5 s ingedrukt bij opstart → NVS wissen (geïmplementeerd in main.c) |
| Display-init            | ST7701S via 9-bit software-SPI: CS=expander-pin 4, SCK=GPIO41, MOSI=GPIO48 |
| RGB-panel               | 16-bit parallel, pclk 18 MHz, framebuffer in PSRAM, LVGL dubbel gebufferd 480×48 intern DMA-RAM |
| IO-expander             | TCA9535 op I2C 0x20 (fallback 0x39): LCD_CS=4, LCD_RESET=5, TP_RESET=7 |

---

## Wijzigingslog

| Datum      | Omschrijving                                                                             |
|------------|------------------------------------------------------------------------------------------|
| 2026-07-12 | US-016 geschreven en na review aangescherpt (story + acceptatiecriteria + implementatieplan, goedgekeurd): verticaal per pagina bladeren — veeg omhoog/omlaag = directe wissel van de hele zichtbare pagina (geen vrij scrollen, geen animatie i.v.m. judder RGB-panel), permanent zichtbare scrollbalk rechts vervangt de dots |
| 2026-07-12 | US-015 keuzepopup: `hvac_mode_btn_cb`/`fan_mode_btn_cb` openen nu `mode_popup_open()` i.p.v. cyclen — modale overlay (kind van actief scherm, dimt achtergrond, `GESTURE_BUBBLE` uit zodat vegen geen pagina wisselt, `LV_EVENT_DELETE` reset de pointer bij schermwissel) met paneel: titel Mode/Ventilatie + scrollbare lijst, actuele stand in accentkleur; keuze → bestaande set_hvac_mode/set_fan_mode, popup dicht, label volgt state-update; tik buiten paneel sluit zonder wijziging; geen popup bij lege modeslijst. Alle 6 criteria in simulator geverifieerd met geautomatiseerde kliktest (`tools/sim_drive.ps1` nieuw: klik/veeg/screenshot naar SDL-venster); geflasht op hardware, bootlog schoon |
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
| 2026-07-12 | Pagina-vegen zonder schuiven: tileview vervangen door gestapelde pagina-containers (alleen actieve zichtbaar) + `LV_EVENT_GESTURE` op het scherm — veeg links/rechts vervangt de pagina direct; veeg die op een slider begint wordt genegeerd (slider-drag); dots-indicator blijft; widget-registry ongemoeid (alle pagina's blijven bestaan) |
| 2026-07-12 | Schermwissel-animatie uit (slide 200 ms → directe wissel): meting op hardware gaf 66 fps/lage CPU maar full-screen slides bleven schokkerig ogen — judder doordat de LVGL-render niet vsync-gekoppeld is aan de paneelverversing; instant wisselen voelt sneller. LV_USE_PERF_MONITOR weer uit |
| 2026-07-12 | Performance-ronde: CPU 160→240 MHz + compiler -Og→-O2 (IDF-defaults; LVGL rendert in software dus dit halveerde effectief de UI-snelheid) — sdkconfig.defaults, vergde schone build; WiFi- en TCP/IP-taken naar core 1 zodat core 0 exclusief voor de UI is; `wifi_set_low_latency()` gekoppeld aan de backlight (scherm aan = WIFI_PS_NONE voor directe HA-respons, scherm uit = modem-slaap terug); LVGL refresh/input-polling 30→15 ms; -O2 activeerde -Werror=stringop/format-truncation → strncpy-terminators en snprintf-precisies gefixt (web_config, ha_lovelace, ui_entities, ui_setup). Meetbaar: boot-tot-verbonden 8,4→6,0 s, statussen ververst 16,2→10,2 s. LV_USE_PERF_MONITOR tijdelijk aan voor de meting |
| 2026-07-12 | US-014 lamp-slider: nieuw `WIDGET_LIGHT` (schakelaar + helderheids-slider, twee-regel-layout à la climate); slider zichtbaar bij aan+dimbaar, `light_apply_layout()` herpositioneert naam/schakelaar en toont/verbergt slider bij render én update; dimbaarheid uit `supported_color_modes` (filter + parse in ha_messages, tri-state in ha_event_t zodat events zonder attribuut niets overschrijven); registry uitgebreid met slider-veld (drag-lookup); bijvangst: oude WIDGET_SLIDER_BRIGHTNESS-keuze voor lampen werkte nooit (keuze vóór eerste statusupdate); mock: dimmer_bank dimbaar + light.gang niet-dimbaar |
| 2026-07-12 | US-013 scherm-timeout: `display_set_backlight()/display_backlight_on()` in display.c (GPIO45); 1 s LVGL-timer in main.c bewaakt `lv_disp_get_inactive_time()` tegen NVS `screen_timeout` (u32, standaard 30 s, 0 = nooit; scherm blijft aan zolang de wizard open is); wek-aanraking wordt in touch.c opgeslokt tot loslaten + `lv_disp_trig_activity()`; dropdown "Scherm uit na" op statuspagina (Nooit/15s/30s/1m/2m/5m); `storage_get_u32/set_u32` nieuw in storage + simulator-mock |
| 2026-07-12 | Naamgeving als Lovelace + pathless views. (1) Entiteitsnamen: kaartnaam (`name:` in card of entities-item) heeft voorrang, anders friendly_name uit HA (via get_states/state_changed, live label-update via naam-label in widget-registry; entity_t.name_custom bewaakt de voorrang; ha_event_t.friendly_name nieuw). (2) View 'Configuratie' ontbrak overal: HA stuurt views zonder pad als het URL-veld leeg is; die krijgen nu synthetisch pad `#<index>` (parsen, laden, aanvinken werken; index-match in PENDING_ENTITIES). Diagnose-log toont nu per view titel+pad. US-012 op hardware bevestigd: 'Eén actieve view (sensecap) — menu overgeslagen' in bootlog |
| 2026-07-12 | US-012: view-instellingen — nieuw scherm `ui_view_settings.c/h` (checkbox per HA-view, direct opslaan in NVS `enabled_views` als kommagescheiden paden, leeg = geen filter); statuspagina heeft nu Views- én Instellingen-knop naast elkaar; view-menu filtert op aangevinkte views met terugval naar alles als het filter niets oplevert; bij precies één aangevinkte view slaat `app_events.c` het keuzemenu over en laadt die view direct na verbinden (wordt ook `selected_view`); `ui_manager_show_view_menu` herbouwt de lijst i.v.m. gewijzigd filter; simulator-CMakeLists uitgebreid |
| 2026-07-12 | HA-verbinding end-to-end werkend op hardware. Oorzaak 1: typefout in geconfigureerd adres (`duckns`→`duckdns`), gevonden via werkende seriële log + HA-adres op statuspagina. Oorzaak 2: ArduinoJson v7 — `JsonVariant`-tussenvariabelen bij filter-opbouw zijn losgekoppelde null-variants, filter bleef leeg en filterde het hele antwoord weg; ketting-toewijzingen in ha_lovelace.cpp + ha_messages.cpp. Bonus: sections-dashboards (HA 2024+) worden geparsed (`views[].sections[].cards[]`); diagnose-log toont eerste 200 tekens payload als 'result' ontbreekt. Log bevestigt: auth OK, 30 entiteiten geladen, get_states ververst |
| 2026-07-12 | Diagnose-iteratie HA-verbinding: statuspagina toont nu het geconfigureerde HA-adres én de verbindingsfase (Verbinden.../Authenticeren.../Verbonden/Token ongeldig/Verbroken); nieuw `HA_EVT_AUTH_FAILED`-event (auth_invalid → duidelijke melding op statuspagina i.p.v. alleen log + stille reconnect-lus); token en URL worden getrimd bij opslaan (web-formulier én touchscreen — geplakte `\r\n` achter het token gaf auth_invalid); token-leesbuffer 256→512 (kleiner dan opslagkant liet nvs_get_str falen → lege token verstuurd); `tools/serial_log.py` asserteert DTR/RTS niet meer (pyserial resette het board bij elke herverbinding, log bleef daardoor leeg); gegenereerde sdkconfig verwijderd zodat `CONFIG_MBEDTLS_EXTERNAL_MEM_ALLOC` (TLS-heap naar PSRAM) zeker in de build zit |
| 2026-07-07 | US-009 + US-010 afgerond: statuspagina herzien (titelbalk + terug-knop, RSSI-balkjes 0–4, IP via wifi_get_ip, HA-status groen/rood o.b.v. app_state, actieve view-naam via lovelace-lijst, Instellingen-knop); foutmelding bij STATE_DISCONNECTED met WiFi/HA-onderscheid + directe refresh via ui_status_refresh(); na herverbinding automatisch terug naar opgeslagen view (geen get_views ernaast — één pending-slot in ha_lovelace); mock: bij ha_available=0 komt HA na 8 s weer online (herstel-test); sim-stub esp_wifi geeft nu nep-AP terug. Herstel-scenario end-to-end geverifieerd in simulator |
