# Technische Verkenning — SenseCap Indicator HA Controller

**Datum:** 2026-06-27  
**Status:** Afgerond  
**Conclusie:** Project is technisch haalbaar met één belangrijke nuancering (zie §7)

---

## 1. Hardware Specificaties SenseCap Indicator

### Dual-MCU architectuur

Het apparaat bevat twee onafhankelijke microcontrollers die via UART/COBS communiceren:

**ESP32-S3 (primaire MCU — Wi-Fi, display, applicatielogica)**

| Kenmerk         | Waarde                                   |
|-----------------|------------------------------------------|
| CPU             | Dual-core Xtensa LX7, 240 MHz            |
| Intern SRAM     | 512 KB (~350 KB vrij heap in de praktijk)|
| PSRAM           | **8 MB OPI PSRAM** (extern, essentieel!) |
| Flash           | 8 MB intern                              |
| Wi-Fi           | 2.4 GHz 802.11 b/g/n                     |
| Bluetooth       | BLE 5.0                                  |

**RP2040 (secundaire MCU — sensoren, SD-kaart)**  
Niet relevant voor dit project; communiceert via UART met de ESP32-S3 voor sensordata.

### Scherm

| Kenmerk          | Waarde                      |
|------------------|-----------------------------|
| Diagonaal        | 3.95" / 4"                  |
| Resolutie        | **480 × 480 px**            |
| Display driver   | ST7701S (RGB parallel 16-bit)|
| Touch controller | FT5X06 (capacitief, I2C)    |
| Backlight        | PWM via GPIO45               |

### GPIO-gebruik (ESP32-S3)

De RGB-displayinterface bezet GPIO 0–21 volledig. Vrij beschikbaar:
- GPIO38 — user-knop
- Grove-connectoren worden beheerd door de RP2040

**IO-expander:** PCA9535PW (I2C, adres 0x20) beheert CS-pin van ST7701, touch INT/RST en optionele LoRa.

---

## 2. UI-framework: LVGL

### Beschikbaarheid

LVGL is volledig beschikbaar via drie routes:

| Framework          | LVGL versie | Status                         |
|--------------------|-------------|--------------------------------|
| Seeed ESP-IDF SDK  | 8.3.3       | Stabiel, aanbevolen            |
| Arduino IDE        | 9.2.2       | Ondersteund, handmatige setup  |
| ESPHome            | 9.5.0       | Nieuwste, YAML-gestuurd        |

### Geheugen — kritisch punt

- **Framebuffer** 480×480×2 bytes (16-bit kleur) = **~460 KB** per buffer
- Dit past **niet** in het interne SRAM (512 KB totaal)
- **De 8 MB PSRAM is verplicht** — in Arduino IDE moet "OPI PSRAM" expliciet geselecteerd worden, anders crasht LVGL

### Beschikbare widgets voor dit project

| Widget            | Toepassing                    |
|-------------------|-------------------------------|
| `lv_switch`       | Schakelaar aan/uit (US-004)   |
| `lv_slider`       | Dimmer / thermostaat (US-005) |
| `lv_arc`          | Ronde slider voor thermostaat |
| `lv_btn`          | Script / scene knop (US-007)  |
| `lv_label`        | Sensor read-only (US-006)     |
| `lv_list`         | View-selectiemenu (US-002)    |
| `lv_textarea`     | Configuratiewizard (US-001)   |
| `lv_msgbox`       | Foutmeldingen (US-009)        |

---

## 3. WebSocket + Home Assistant Connectiviteit

### Authenticatieflow HA WebSocket API

Eindpunt: `ws://[HA-IP]:8123/api/websocket`

```
Client verbindt →
Server:  {"type": "auth_required", "ha_version": "2024.x.x"}
Client:  {"type": "auth", "access_token": "LONG_LIVED_ACCESS_TOKEN"}
Server:  {"type": "auth_ok", ...}  OF  {"type": "auth_invalid", ...}
```

Na `auth_ok` is de verbinding volledig operationeel. Alle berichten gebruiken een numeriek `id`-veld voor request-response koppeling.

### Veelgebruikte commando's

```jsonc
// State-changed events abonneren
{"id": 1, "type": "subscribe_events", "event_type": "state_changed"}

// Schakelaar omzetten
{"id": 2, "type": "call_service", "domain": "switch",
 "service": "turn_on", "target": {"entity_id": "switch.woonkamer"}}

// Lovelace config ophalen
{"id": 3, "type": "lovelace/config", "url_path": null}
// null = standaard dashboard; specifiek: "url_path": "mijn-dashboard"

// Alle entiteitsstatussen ophalen (na reconnect)
{"id": 4, "type": "get_states"}
```

### Aanbevolen bibliotheken

| Aanpak       | Bibliotheek                          | Opmerkingen                    |
|--------------|--------------------------------------|--------------------------------|
| ESP-IDF      | `esp_websocket_client` (ingebouwd)   | Laagste overhead, aanbevolen   |
| Arduino      | `ArduinoWebsockets` (gilmaimon)      | Moderne API, WSS/TLS support   |
| Arduino      | `WebSockets` (Links2004)             | Breed gebruikt alternatief     |

Eén WebSocket-verbinding verbruikt ca. 4–8 KB RAM — verwaarloosbaar met 8 MB PSRAM.

**Geen kant-en-klare Arduino-bibliotheek voor HA WebSocket** gevonden. Eigen implementatie van de auth-flow is nodig, maar is eenvoudig en goed gedocumenteerd.

---

## 4. Lovelace Config Parsen

### Omvang van een typische Lovelace-config

| Installatietype                    | JSON-grootte    |
|------------------------------------|-----------------|
| Klein (5–10 entiteiten)            | 5–20 KB         |
| Middel (20–50 entiteiten)          | 50–150 KB       |
| Groot (100+ entiteiten, meerdere views) | 300 KB – 1 MB+ |

### Past dit in het geheugen?

- Intern SRAM alleen: **niet** voor middelgrote en grote configs
- Met 8 MB PSRAM en ArduinoJson v7 PSRAM-allocator: **ja**, ook grote configs passen
- ArduinoJson v7 vereist ca. 2× documentgrootte (100 KB JSON → ~200 KB RAM)

### Aanbevolen aanpak JSON

**ArduinoJson v7** met PSRAM-allocator:
- Zero-copy parser
- Excellent gedocumenteerd
- Expliciete PSRAM-ondersteuning via `heap_caps_malloc`

**Realistische parsingstrategie:**  
Niet het hele dashboard parsen — alleen de geselecteerde view ophalen via `lovelace/config` en dan filteren op ondersteunde card-types. Onbekende types worden genegeerd (geen crash).

---

## 5. NVS — Persistente Opslag

Opslaan van WiFi SSID/wachtwoord, HA-URL, HA-token en geselecteerd view-pad via de NVS Preferences API:

```cpp
Preferences prefs;
prefs.begin("ha-config", false);
prefs.putString("ha_url",   "http://192.168.1.10:8123");
prefs.putString("ha_token", "eyJhbG...");
prefs.putString("view_path", "woonkamer");
prefs.end();
```

### Beveiliging van de access token

| Methode               | Beschrijving                                        | Aanbeveling         |
|-----------------------|-----------------------------------------------------|---------------------|
| Geen encryptie        | Plaintext in flash — leesbaar met flash-dump tool   | Alleen voor prototyping |
| Flash Encryption      | XTS-AES, sleutels in aparte partitie                | Goed                |
| HMAC-gebaseerde NVS   | Sleutels afgeleid van eFuse HMAC — geen flash-dump  | Beste (productie)   |

---

## 6. Bestaande Vergelijkbare Projecten

| Project                     | Aanpak                              | Relevantie                                      |
|-----------------------------|-------------------------------------|-------------------------------------------------|
| `Love4yzp/sensecap-indicator-ha` | ESP-IDF v5.4 + LVGL 9 + **MQTT**   | Directe HA-integratie, maar via MQTT niet WebSocket |
| HomeDicator                 | ESPHome + LVGL YAML                 | Statische UI, geen dynamische Lovelace rendering |
| `ril3y/sensecap-indicator-d1l`  | PlatformIO + LVGL + Arduino_GFX    | Goede startbasis voor PlatformIO-aanpak          |
| openHASP                    | openHASP-firmware                   | Rijpe oplossing maar vaste UI in JSON-templates  |

**Belangrijke observatie:** Geen enkel bestaand project haalt de Lovelace-config op en rendert die dynamisch. Alle bestaande projecten gebruiken een statische UI (YAML/JSON templates). Dat is een bewuste keuze.

---

## 7. Haalbaarheidsanalyse

### Per user story

| US    | Haalbaarheid          | Opmerkingen                                                      |
|-------|-----------------------|------------------------------------------------------------------|
| US-001 | Volledig haalbaar    | NVS Preferences + LVGL keyboard + WiFi scan AP-mode              |
| US-002 | Volledig haalbaar    | `lovelace/config` ophalen, view-titels parsen, LVGL list         |
| US-003 | **Gedeeltelijk**     | Zie kritische nuancering hieronder                               |
| US-004 | Volledig haalbaar    | `call_service` + WebSocket state_changed                         |
| US-005 | Volledig haalbaar    | LVGL slider + `brightness_pct` / `set_temperature`               |
| US-006 | Volledig haalbaar    | Read-only LVGL label, bijgewerkt via state_changed               |
| US-007 | Volledig haalbaar    | `script.turn_on`, `scene.turn_on`, `button.press`                |
| US-008 | Volledig haalbaar    | `subscribe_events` + `get_states` na reconnect                   |
| US-009 | Volledig haalbaar    | WebSocket `on_close` callback + exponential backoff              |
| US-010 | Volledig haalbaar    | `WiFi.RSSI()`, `millis()`, NVS-uitlezing                         |

### Kritische nuancering: US-003 Lovelace-rendering

**Volledig dynamische rendering** van elke mogelijke Lovelace card-type (zoals in een browser) is **niet realistisch** op embedded hardware. Er zijn tientallen card-types en elk heeft eigen layout-logica.

**Realistische aanpak voor US-003:**  
De Lovelace-config wordt opgehaald via WebSocket om te weten *welke entiteiten* in de view zitten. De rendering zelf gebruikt een eigen, vaste lay-out op basis van het entiteitstype (switch → toggle, sensor → label, light met brightness → slider, script → button). De gebruiker configureert de HA-view met ondersteunde entity-types; de app rendert die automatisch correct.

**Dit is de aanpak van alle bestaande vergelijkbare projecten.**

### Risico-overzicht

| Risico                               | Ernst  | Maatregel                                            |
|--------------------------------------|--------|------------------------------------------------------|
| OPI PSRAM niet ingeschakeld → crash  | Hoog   | Build-config documenteren, altijd controleren        |
| Lovelace-config te groot voor SRAM   | Hoog   | Alleen geselecteerde view ophalen + PSRAM-allocator  |
| LVGL versie-incompatibiliteit        | Middel | Eén versie kiezen en vaststellen in het project      |
| WebSocket reconnect-logica           | Middel | Exponential backoff + `get_states` na reconnect      |
| SSL/TLS voor WSS (HTTPS HA)          | Middel | Start met `ws://`; TLS later toevoegen               |
| ESP-IDF versie-pin (exact v5.1.x)    | Laag   | Versie vastpinnen in `idf_component.yml`             |

---

## 8. Aanbevolen Technologiestack

### Optie A — Aanbevolen (volledige controle)

| Component          | Keuze                                    |
|--------------------|------------------------------------------|
| Build-systeem      | **PlatformIO + ESP-IDF v5.1.x**          |
| UI-framework       | **LVGL 8.3.3** (Seeed officieel SDK)     |
| WebSocket          | `esp_websocket_client` (ingebouwd)       |
| JSON parsing       | **ArduinoJson v7** met PSRAM-allocator   |
| Config opslag      | **NVS via Preferences**                  |
| Startpunt SDK      | `SenseCAP_Indicator_ESP32` + `sensecap-indicator-ha` als referentie |

### Optie B — Snelste prototyping

| Component          | Keuze                                    |
|--------------------|------------------------------------------|
| Build-systeem      | **Arduino IDE**                          |
| UI-framework       | **LVGL 9.2.2**                           |
| WebSocket          | `ArduinoWebsockets` (gilmaimon)          |
| JSON parsing       | ArduinoJson v7                           |
| Config opslag      | NVS via Preferences                      |

### Optie C — Geen code schrijven (statische UI)

| Component          | Keuze                                    |
|--------------------|------------------------------------------|
| Platform           | **ESPHome** + LVGL YAML                  |
| Beperking          | Geen dynamische Lovelace-rendering, UI handmatig configureren in YAML |
| Voordeel           | Zero C++ code, directe HA-integratie     |

---

## 9. Conclusie

**Het project is technisch haalbaar.** De hardware (8 MB PSRAM, 8 MB Flash, 480×480 touchscreen, Wi-Fi) is ruimschoots voldoende.

**Aanbeveling:** Kies PlatformIO + ESP-IDF v5.1.x voor maximale controle. Gebruik de bestaande Seeed SDK als startpunt en bouw de HA WebSocket-client bovenop de `sensecap-indicator-ha` referentie-implementatie.

**Pas US-003** aan: de app rendert entiteiten op basis van hun type (switch/slider/sensor/button), niet op basis van de volledige Lovelace card-configuratie. De Lovelace-config bepaalt welke entiteiten getoond worden; de lay-out is eigen en vaste implementatie.

---

*Gegenereerd op: 2026-06-27*  
*Bronnen: Seeed Wiki, Espressif docs, HA Developer docs, GitHub projecten (zie tekst)*
