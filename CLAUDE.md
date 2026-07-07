# CLAUDE.md — SenseCap Indicator HA Controller

## Status bijhouden

**Na elke user story of significante codewijziging:** werk `documentatie/status.md` bij.
Lees dit bestand aan het begin van een sessie — het vervangt het volledige codebase-scan en geeft direct de actuele stand van zaken.

`status.md` bevat altijd:
- Per US: status (Gerealiseerd / Deels / Niet gestart) + korte toelichting
- **Volgende stappen** — concreet per bestand en aanpak
- Architectuur-snapshot (welk bestand doet wat)
- Wijzigingslog (datum + wat er gedaan is)

## Project

SenseCap Indicator (ESP32-S3 + RP2040, 480×480 touchscreen) — bidirectionele Home Assistant controller via WebSocket API.

**Stack:** PlatformIO + ESP-IDF v5.1.x · LVGL 8.3.3 · esp_websocket_client · ArduinoJson v7 · NVS

**Documentatie:** `documentatie/` — userstories.md, technisch_ontwerp.md, status.md

## Werkwijze

- Implementeer één user story per sessie, tenzij anders gevraagd.
- Na implementatie: vink acceptatiecriteria af in `documentatie/userstories.md`.
- Na implementatie: update `documentatie/status.md` (status, volgende stappen, wijzigingslog).
- Alle LVGL-aanroepen uitsluitend vanuit `task_ui` (Core 0) — nooit vanuit `task_ha_ws`.
- Communicatie tussen taken via `ha_event_queue` of `cmd_queue` — nooit directe LVGL-aanroepen vanuit `task_ha_ws`.
