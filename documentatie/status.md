# Status — SenseCap Indicator Home Assistant Controller

**Laatste update:** 2026-06-27  
**Fase:** Technische verkenning afgerond — klaar voor ontwerp

---

## Projectoverzicht

| Kenmerk         | Waarde                                           |
|-----------------|--------------------------------------------------|
| Hardware        | Seeed Studio SenseCap Indicator                  |
| Scherm          | 4" touchscreen, 480×480 px, ST7701S + FT5X06     |
| MCU             | ESP32-S3 (240 MHz, 8 MB PSRAM, 8 MB Flash) + RP2040 |
| Verbinding      | Home Assistant WebSocket API (`ws://host:8123/api/websocket`) |
| UI-framework    | **LVGL 8.3.3** (Seeed ESP-IDF SDK) — aanbevolen |
| Build-systeem   | **PlatformIO + ESP-IDF v5.1.x** — aanbevolen    |
| JSON-parser     | ArduinoJson v7 met PSRAM-allocator               |
| Config-opslag   | NVS via Preferences API                          |

---

## Fasestatus

| Fase                      | Status         | Opmerkingen                                              |
|---------------------------|----------------|----------------------------------------------------------|
| Analyse & interviews      | Afgerond       | 2026-06-27                                               |
| User stories              | Afgerond       | 10 stories in userstories.md                             |
| Technische verkenning     | **Afgerond**   | 2026-06-27 — zie technische_verkenning.md                |
| Technisch ontwerp         | **Afgerond**   | 2026-06-27 — zie technisch_ontwerp.md                    |
| Implementatie             | Niet gestart   |                                                          |
| Testen                    | Niet gestart   |                                                          |
| Oplevering                | Niet gestart   |                                                          |

---

## User Story Status

| ID      | Titel                                  | Status         | Haalbaarheid    | Prioriteit  |
|---------|----------------------------------------|----------------|-----------------|-------------|
| US-001  | Initiële configuratie instellen        | Niet gestart   | Volledig        | Must have   |
| US-002  | View selecteren via on-device menu     | Niet gestart   | Volledig        | Must have   |
| US-003  | Entiteiten uit HA-view weergeven       | Niet gestart   | Volledig        | Must have   |
| US-004  | Schakelaar bedienen (aan/uit)          | Niet gestart   | Volledig        | Must have   |
| US-005  | Schuifregelaar bedienen                | Niet gestart   | Volledig        | Must have   |
| US-006  | Sensorwaarden bekijken                 | Niet gestart   | Volledig        | Must have   |
| US-007  | Knoppen / scripts / scenes triggeren   | Niet gestart   | Volledig        | Must have   |
| US-008  | Realtime updates ontvangen vanuit HA   | Niet gestart   | Volledig        | Must have   |
| US-009  | Verbindingsfout tonen en herstellen    | Niet gestart   | Volledig        | Must have   |
| US-010  | Statuspagina bekijken                  | Niet gestart   | Volledig        | Should have |

**US-003 herzien (2026-06-27):** Entiteitstype-gebaseerde rendering, 6 entiteiten per pagina. Bij meer dan 6 entiteiten: horizontaal swipen naar volgende pagina, paginering-indicator (dots) onderaan. Domein bepaalt widget: switch/input_boolean → toggle, light met brightness → slider, climate → slider °C, sensor → label, script/scene → knop. Lovelace-config bepaalt welke entiteiten en in welke volgorde; de app bepaalt de layout.

---

## Technische beslissingen (genomen op basis van verkenning)

| Beslissing                          | Keuze                              | Reden                                              |
|-------------------------------------|------------------------------------|----------------------------------------------------|
| Build-systeem                       | PlatformIO + ESP-IDF v5.1.x        | Volledige controle, Seeed officieel ondersteund    |
| UI-framework                        | LVGL 8.3.3                         | Stabiele versie in Seeed SDK, alle widgets aanwezig|
| WebSocket-bibliotheek               | `esp_websocket_client` (ingebouwd) | Laagste overhead, native ESP-IDF                   |
| JSON-parser                         | ArduinoJson v7                     | PSRAM-support, zero-copy, excellente docs          |
| Configuratie-opslag                 | NVS Preferences API                | Persistent na reboot, eenvoudige API               |
| Token-beveiliging (v1)              | Plaintext NVS (geen encryptie)     | Voldoende voor prototyping; HMAC later toevoegen   |
| Lovelace-rendering strategie        | Entiteitstype-gebaseerd layout     | Geen enkel bestaand project doet volledige rendering|
| Startpunt referentie                | `sensecap-indicator-ha` (GitHub Love4yzp) | Bewezen ESP-IDF + LVGL + HA basis           |

---

## Genomen ontwerppbeslissingen

| Beslissing              | Keuze                                                                   |
|-------------------------|-------------------------------------------------------------------------|
| Verbindingsprotocol     | WS (plaintext), lokaal netwerk                                          |
| Max pagina's per view   | 5 pagina's (30 entiteiten)                                              |
| Animatie schermwissel   | Slide, 200 ms                                                           |
| Factory reset           | GPIO38-knop 5 s ingedrukt bij opstart                                   |
| Kleurthema              | Donker (`#1A1A2E` achtergrond, `#4FC3F7` accent)                        |
| Entiteitstypen v1       | switch, input_boolean, light, sensor, binary_sensor, climate            |
| Entiteitstypen v2       | script, scene, button, automation (bewust uitgesteld)                   |
| Token-beveiliging v1    | Plaintext NVS — aanvaardbaar voor thuisproject, HMAC-NVS voorzien in v2 |
| Behuizing               | Vrijstaand op tafel/plank — geen extra software-eisen                   |
| PSRAM OPI               | Altijd verplicht inschakelen in PlatformIO build-config (technisch feit) |

## Openstaande beslispunten

Geen — alle ontwerpbeslissingen zijn afgestemd. Het project is klaar voor implementatie.

---

## Bekende risico's

| Risico                               | Ernst  | Maatregel                                |
|--------------------------------------|--------|------------------------------------------|
| OPI PSRAM niet ingeschakeld → crash  | Hoog   | Eerste stap in build-setup: PSRAM testen |
| Lovelace-config te groot voor SRAM   | Hoog   | Alleen geselecteerde view ophalen        |
| LVGL versie-conflict met ander libs  | Middel | Versie vastpinnen in platformio.ini      |
| HA zonder SSL moeilijk bereikbaar    | Middel | Intern netwerk: WS voldoende voor nu     |

---

## Wijzigingslog

| Datum       | Omschrijving                                                        | Door         |
|-------------|---------------------------------------------------------------------|--------------|
| 2026-06-27  | Project gestart, user stories aangemaakt (US-001 t/m US-010)        | Claude Code  |
| 2026-06-27  | Technische verkenning afgerond, stack gekozen, US-003 genuanceerd   | Claude Code  |
| 2026-06-27  | Technisch ontwerp v1.0 geschreven (architectuur, taken, UI, data)   | Claude Code  |
| 2026-06-27  | Ontwerpbeslissingen verwerkt in ontwerp v1.1 (WS, 5 pagina's, donker thema, slide, knop-reset) | Gebruiker + Claude Code |
| 2026-06-27  | Alle beslispunten afgestemd — klaar voor implementatie (v1.2)           | Gebruiker + Claude Code |
