# User Stories — SenseCap Indicator Home Assistant Controller

**Project:** SenseCap HA Controller  
**Hardware:** Seeed Studio SenseCap Indicator (4" touchscreen, ESP32-S3 + RP2040)  
**Verbinding:** Home Assistant WebSocket API  
**Doelgroep:** Bezoekers / publiek  
**Datum:** 2026-06-27

---

## Overzicht

| ID      | Titel                                  | Prioriteit |
|---------|----------------------------------------|------------|
| US-001  | Initiële configuratie instellen        | Must have  |
| US-002  | View selecteren via on-device menu     | Must have  |
| US-003  | Entiteiten uit HA-view weergeven       | Must have  |
| US-004  | Schakelaar bedienen (aan/uit)          | Must have  |
| US-005  | Schuifregelaar bedienen                | Must have  |
| US-006  | Sensorwaarden bekijken                 | Must have  |
| US-007  | Knoppen / scripts / scenes triggeren   | Must have  |
| US-008  | Realtime updates ontvangen vanuit HA   | Must have  |
| US-009  | Verbindingsfout tonen en herstellen    | Must have  |
| US-010  | Statuspagina bekijken                  | Should have|
| US-011  | Airco bedienen: mode en ventilatie     | Should have|

---

## US-001 — Initiële configuratie instellen

**Als** beheerder van het apparaat  
**wil ik** bij de eerste opstart WiFi-inloggegevens, het HA-adres en een Long-Lived Access Token kunnen invoeren op het scherm  
**zodat** het apparaat verbinding kan maken met mijn Home Assistant-installatie zonder dat ik code hoef te aanpassen.

### Gedetailleerde beschrijving

Bij de allereerste opstart (of na een factory reset) toont het apparaat een setup-wizard. De wizard loopt stap voor stap door de benodigde configuratie: WiFi-netwerk selecteren en wachtwoord invoeren, het lokale HA-adres invoeren (bijv. `http://192.168.1.10:8123`) en een HA Long-Lived Access Token invoeren. Alle ingevulde waarden worden opgeslagen in het permanente geheugen (NVS/flash) van het apparaat zodat ze bij herstart bewaard blijven.

### Acceptatiecriteria

- [ ] Bij eerste opstart wordt automatisch de setup-wizard getoond.
- [ ] De gebruiker kan een WiFi-netwerk kiezen uit een gescande lijst.
- [ ] WiFi-wachtwoord kan ingevoerd worden via een on-screen toetsenbord.
- [ ] HA-URL en Access Token kunnen ingevoerd worden via een on-screen toetsenbord.
- [ ] Ingevulde configuratie wordt opgeslagen in niet-vluchtig geheugen (NVS).
- [ ] Na succesvolle configuratie gaat de app automatisch door naar het view-keuzemenu.
- [ ] Vanuit de instellingenpagina kan de configuratie later opnieuw worden ingesteld.
- [ ] Foutieve configuratie (bijv. verkeerde URL) geeft een duidelijke foutmelding.

### Technische opmerkingen

- NVS (Non-Volatile Storage) via ESP-IDF of Arduino Preferences-library.
- On-screen keyboard kan gerealiseerd worden via LVGL keyboard widget.
- Access Token mag na opslaan niet meer in plaintext op scherm getoond worden.

---

## US-002 — View selecteren via on-device menu

**Als** bezoeker of gebruiker van het apparaat  
**wil ik** via een menu op het scherm kunnen kiezen welke Home Assistant-view weergegeven wordt  
**zodat** ik de voor mij relevante ruimte of functies kan selecteren zonder in HA zelf te hoeven navigeren.

### Gedetailleerde beschrijving

Na succesvolle verbinding met HA haalt de app via de WebSocket API de lijst van beschikbare dashboards en views op. Deze lijst wordt gepresenteerd als een selecteerbaar menu op het SenseCap-scherm. De gebruiker tikt op een view-naam en de app laadt die view. De geselecteerde view wordt ook opgeslagen zodat deze bij de volgende opstart automatisch geladen wordt (tenzij de gebruiker iets anders kiest).

### Acceptatiecriteria

- [ ] De app haalt automatisch de lijst van beschikbare HA-dashboards en views op via de WebSocket API.
- [ ] De lijst wordt weergegeven als een scrollbaar menu op het scherm.
- [ ] De gebruiker kan een view selecteren met een enkele tik.
- [ ] Na selectie wordt de view direct geladen en weergegeven.
- [ ] De laatste geselecteerde view wordt opgeslagen en bij herstart automatisch geladen.
- [ ] Vanuit de weergegeven view kan de gebruiker terugkeren naar het selectiemenu (bijv. via een hamburgerpictogram of veeg-gebaar).
- [ ] Als er geen views beschikbaar zijn, toont het menu een duidelijke melding.

### Technische opmerkingen

- HA WebSocket-commando: `get_lovelace_config` of `lovelace/config` ophalen.
- Views zijn genest in het dashboard-configuratieobject; parse `views[].title` en `views[].path`.
- Geselecteerde view opslaan in NVS (sleutel: `selected_view_path`).

---

## US-003 — Entiteiten uit HA-view weergeven op scherm

> **Herzien op 2026-06-27** — volledige Lovelace card-rendering is niet haalbaar op embedded hardware.  
> Afgestemde aanpak: entiteitstype-gebaseerde rendering, 6 entiteiten per pagina, swipen bij meer.

**Als** gebruiker van het apparaat  
**wil ik** dat de entiteiten uit de geselecteerde Home Assistant-view automatisch weergegeven worden op het SenseCap-scherm, waarbij elk entiteitstype een passende widget krijgt en ik bij meer dan 6 entiteiten horizontaal kan swipen naar een volgende pagina  
**zodat** ik alle entiteiten in een view kan bereiken en bedienen, ongeacht hoeveel er zijn.

### Gedetailleerde beschrijving

De app haalt via de WebSocket API de lijst van entiteiten in de geselecteerde HA-view op (`lovelace/config` of `get_states` gefilterd op de view). Per entiteit bepaalt de app welke LVGL-widget getoond wordt op basis van het entiteitsdomein en de beschikbare attributen:

| Entiteitsdomein / attribuut              | Widget op scherm              |
|------------------------------------------|-------------------------------|
| `switch`, `input_boolean`, `automation`  | Toggle (aan/uit)              |
| `light` zonder `brightness`              | Toggle (aan/uit)              |
| `light` met `brightness`                 | Slider (0–100%)               |
| `climate` (thermostaat)                  | Slider met °C-waarde          |
| `sensor`, `binary_sensor`                | Read-only label               |
| `script`, `scene`, `button`              | Knop (eenmalige actie)        |
| Onbekend domein                          | Read-only label (naam + staat)|

Het scherm toont **6 entiteiten per pagina** in een verticale lijst. Bij meer dan 6 entiteiten worden de entiteiten verdeeld over meerdere pagina's van elk 6. De gebruiker navigeert tussen pagina's door **horizontaal te swipen** (links/rechts). Onderaan het scherm is een paginering-indicator zichtbaar (dots of nummers) die aangeeft op welke pagina de gebruiker zich bevindt en hoeveel pagina's er zijn.

De lay-out is vast: elke rij toont entiteitnaam links, widget rechts, rijhoogte 80 px, exact 6 rijen per pagina = 480 px schermhoogte.

### Acceptatiecriteria

- [ ] De app haalt de entiteiten van de geselecteerde HA-view op via de WebSocket API.
- [ ] Per entiteit wordt automatisch de juiste widget bepaald op basis van het domein (zie tabel hierboven).
- [ ] Maximaal 6 entiteiten worden per pagina weergegeven in een verticale lijst.
- [ ] Bij meer dan 6 entiteiten worden de resterende entiteiten op een volgende pagina geplaatst (elke 6 entiteiten = één pagina).
- [ ] De gebruiker kan horizontaal swipen om tussen pagina's te navigeren.
- [ ] Een paginering-indicator (bijv. dots) onderaan het scherm toont het totale aantal pagina's en de huidige positie.
- [ ] De swipe-animatie is vloeiend (geen harde sprong).
- [ ] Elke rij toont minimaal: naam van de entiteit en de bijbehorende widget.
- [ ] Onbekende entiteitstypen tonen een read-only label met naam en huidige staat (geen crash).
- [ ] De rendering is voltooid binnen 2 seconden na view-selectie.
- [ ] Widgets zijn groot genoeg om eenvoudig te bedienen door bezoekers (minimaal raakoppervlak: 44×44 px).
- [ ] Realtime updates (state_changed) worden ook bijgewerkt op pagina's die op dat moment niet zichtbaar zijn.

### Niet in scope (bewuste keuze)

- Volledige Lovelace card-rendering (zoals in een browser) — niet haalbaar op embedded hardware.
- Verticaal scrollen binnen een pagina — paginering via swipe is de navigatiemethode.
- Custom layout per entiteit — de lay-out is altijd verticale lijstweergave.
- Kaarttype-gebaseerde rendering (`entities card`, `glance card`, enz.) — niet van toepassing.

### Hoe de gebruiker dit instelt in HA

De gebruiker maakt in Home Assistant een Lovelace-view aan met ondersteunde entiteitstypen (switch, light, sensor, climate, script, scene). De volgorde in de HA-view bepaalt de volgorde op het apparaat en daarmee de paginaindeling. Entiteiten 1–6 verschijnen op pagina 1, entiteiten 7–12 op pagina 2, enzovoort.

### Technische opmerkingen

- `lovelace/config` retourneert het volledige dashboard-object; filter op `views[].path === geselecteerde_view`, lees dan `views[].cards[].entities` of `views[].cards[].entity`.
- Alternatief: `get_states` voor de entiteiten van de view (robuster als Lovelace-structuur varieert).
- LVGL paginering: gebruik `lv_tabview` of een `lv_obj` met `lv_scroll_snap` en horizontale scrollrichting voor de swipe-navigatie.
- Paginering-indicator: `lv_label` of rij `lv_obj` met dots, gepositioneerd in de onderste 20 px van het scherm (rijhoogte dan 76 px × 6 = 456 px + 24 px indicator).
- LVGL-widgets: `lv_switch`, `lv_slider`, `lv_label`, `lv_btn` — allemaal standaard aanwezig in LVGL 8.3.3.
- Realtime updates: entiteiten in de achtergrondpagina's worden in geheugen bijgewerkt; widget wordt gerenderd zodra de gebruiker naar die pagina swipt (of altijd als geheugen het toelaat).

---

## US-004 — Schakelaar bedienen (aan/uit)

**Als** gebruiker van het apparaat  
**wil ik** schakelaars (zoals lampen of stopcontacten) kunnen aan- en uitzetten via het touchscreen  
**zodat** ik apparaten in huis kan bedienen zonder de Home Assistant-app op mijn telefoon te openen.

### Gedetailleerde beschrijving

Schakelaars worden weergegeven als toggle-elementen (aan/uit-knop of schakelaar). Bij aanraking stuurt de app direct een `call_service`-bericht via de WebSocket API naar HA (`homeassistant.turn_on` of `homeassistant.turn_off`). De UI geeft direct visuele feedback (optimistic update): de schakelaar toont de nieuwe staat, en zodra HA de bevestiging terugstuurt, wordt de definitieve staat bevestigd. Als HA een fout teruggeeft, keert de schakelaar terug naar de vorige staat en toont een foutmelding.

### Acceptatiecriteria

- [ ] Schakelaars worden weergegeven als duidelijk herkenbare toggle-elementen.
- [ ] Één tik op een schakelaar stuurt onmiddellijk een commando naar HA.
- [ ] De UI toont direct een optimistische statuswijziging na het tikken.
- [ ] De definitieve staat wordt bijgewerkt zodra HA de bevestiging stuurt.
- [ ] Bij een fout van HA wordt de schakelaar teruggedraaid naar de vorige staat.
- [ ] De huidige staat (aan = groen/actief, uit = grijs/inactief) is visueel duidelijk onderscheidbaar.
- [ ] Ondersteunde domeinen: `light`, `switch`, `input_boolean`, `automation`.

### Technische opmerkingen

- WebSocket-commando: `call_service` met `domain`, `service` en `service_data.entity_id`.
- State-updates komen binnen via het `state_changed`-event (WebSocket subscription).

---

## US-005 — Schuifregelaar bedienen

**Als** gebruiker van het apparaat  
**wil ik** dimmers en temperatuurregelaars kunnen instellen via een schuifregelaar op het scherm  
**zodat** ik nauwkeurig de gewenste lichtsterkte of temperatuur kan instellen.

### Gedetailleerde beschrijving

Dimbare lampen en thermostaten worden weergegeven met een horizontale of verticale schuifregelaar (slider). De slider toont de huidige waarde als getal naast de regelaar. Bij het slepen van de slider wordt de waarde lokaal bijgewerkt; bij loslaten wordt het commando naar HA gestuurd. Om onnodige netwerkverkeer te voorkomen, worden commando's niet bij elke pixel-beweging verstuurd maar pas na het loslaten (of maximaal 1x per 200 ms bij continu slepen). Voor thermostaten wordt de eenheid (°C) naast de waarde getoond.

### Acceptatiecriteria

- [ ] Dimbare lampen tonen een schuifregelaar voor helderheid (0–100%).
- [ ] Thermostaten tonen een schuifregelaar voor de gewenste temperatuur met min/max grenzen uit HA.
- [ ] De huidige waarde staat zichtbaar naast of op de slider.
- [ ] Commando's worden verstuurd na loslaten (throttle: max. 1x per 200 ms bij continu gebruik).
- [ ] De UI past zich aan als HA de waarde buiten de slider beweegt (bijv. via een automatisering).
- [ ] Ondersteunde domeinen: `light` (brightness), `climate` (temperature), `input_number`, `cover` (position).

### Technische opmerkingen

- `light.turn_on` met `brightness_pct` voor dimmen.
- `climate.set_temperature` voor thermostaten.
- LVGL `lv_slider` widget is geschikt voor deze functie.
- Throttle implementeren in de touch event handler.

---

## US-006 — Sensorwaarden bekijken

**Als** bezoeker of gebruiker van het apparaat  
**wil ik** live sensorwaarden zien op het scherm (zoals temperatuur, luchtvochtigheid of bewegingsdetectie)  
**zodat** ik zonder interactie een actueel beeld heb van de situatie in huis.

### Gedetailleerde beschrijving

Sensoren worden weergegeven als read-only tegels met de naam, huidige waarde en eenheid. De waarden worden realtime bijgewerkt via de WebSocket-subscription op `state_changed`-events. Numerieke sensoren tonen de waarde groot en centraal; binaire sensoren (bijv. beweging: ja/nee) tonen een pictogram en/of tekst. Verouderde waarden (ouder dan een configureerbaar interval, standaard 10 minuten) worden gemarkeerd met een indicator.

### Acceptatiecriteria

- [ ] Sensorwaarden worden weergegeven met naam, waarde en eenheid (bijv. "Woonkamer: 21,5 °C").
- [ ] Waarden worden realtime bijgewerkt zonder dat de gebruiker iets hoeft te doen.
- [ ] Binaire sensoren tonen een pictogram voor actief/inactief staat.
- [ ] Numerieke sensoren tonen de waarde afgerond op 1 decimaal.
- [ ] Sensor-tegels zijn niet interactief (geen swipe of tap-actie vereist voor gebruik).
- [ ] Verouderde waarden (> 10 min geen update) tonen een visuele indicator.

### Technische opmerkingen

- `subscribe_events` op `state_changed` filterd op relevante entiteit-IDs.
- Timestamp van laatste update bewaren per entiteit om verouderde data te detecteren.

---

## US-007 — Knoppen, scripts en scenes triggeren

**Als** gebruiker van het apparaat  
**wil ik** met één tik een HA-script, scene of button-entiteit kunnen activeren  
**zodat** ik complexe automatiseringen of sfeerinstellingen direct kan starten.

### Gedetailleerde beschrijving

Scripts, scenes en button-entiteiten worden weergegeven als grote, duidelijke knoppen op het scherm. Bij aanraking wordt direct het bijbehorende HA-service-commando verstuurd (`script.turn_on`, `scene.turn_on`, `button.press`). De knop geeft korte visuele feedback (kleurwijziging of animatie) om de activering te bevestigen. Er is geen "terug"-mogelijkheid: de actie is eenmalig en onomkeerbaar (conform HA-gedrag).

### Acceptatiecriteria

- [x] Scripts worden weergegeven als knoppen met de naam van het script.
- [x] Scenes worden weergegeven als knoppen met de naam van de scene.
- [x] Button-entiteiten gedragen zich als directe actieknoppen.
- [x] Na aanraking geeft de knop visuele feedback (minimaal 300 ms kleurwijziging).
- [x] Het commando wordt maximaal één keer per tik verstuurd (geen dubbelklik-probleem).
- [x] De knop toont een laad-indicator als HA langer dan 500 ms nodig heeft om te reageren.

### Technische opmerkingen

- `script.turn_on`, `scene.turn_on`, `button.press` via `call_service`.
- Debounce instellen op minimaal 500 ms per knop om dubbele activering te voorkomen.

---

## US-008 — Realtime updates ontvangen vanuit HA

**Als** gebruiker van het apparaat  
**wil ik** dat wijzigingen die in Home Assistant plaatsvinden (door andere gebruikers, automatiseringen of fysieke schakelaars) onmiddellijk zichtbaar zijn op het SenseCap-scherm  
**zodat** het scherm altijd de actuele staat van het huis toont en niet verouderde informatie.

### Gedetailleerde beschrijving

Na het ophalen van de initiële staat subscribet de app op `state_changed`-events via de WebSocket-verbinding. Elk inkomend event wordt verwerkt en de bijbehorende UI-component wordt direct bijgewerkt — zonder dat de volledige view opnieuw geladen hoeft te worden. Dit garandeert dat het scherm altijd gesynchroniseerd is met HA, ongeacht hoe de statuswijziging tot stand is gekomen.

### Acceptatiecriteria

- [x] De app ontvangt `state_changed`-events via een actieve WebSocket-subscription.
- [x] UI-componenten worden bijgewerkt binnen 500 ms na ontvangst van een event.
- [x] Schakelaarstatus, sliderwaarden en sensorwaarden worden allemaal realtime bijgewerkt.
- [x] Updates veroorzaken geen volledige herlaad van de view (alleen het gewijzigde element wordt bijgewerkt).
- [x] Bij herverbinding na verbindingsverlies worden alle entiteitsstatussen opnieuw opgehaald via `get_states`.

### Technische opmerkingen

- WebSocket-bericht: `subscribe_events` met `event_type: "state_changed"`.
- Filter op entiteit-IDs die in de actieve view aanwezig zijn om onnodige updates te minimaliseren.
- Na reconnect: `get_states` aanroepen voor alle entiteiten in de view.

---

## US-009 — Verbindingsfout tonen en automatisch herstellen

**Als** gebruiker van het apparaat  
**wil ik** een duidelijke melding zien als de verbinding met Home Assistant wegvalt, en dat het apparaat zichzelf automatisch herverbindt zodra HA weer beschikbaar is  
**zodat** ik niet handmatig het apparaat hoef te herstarten na een netwerk- of HA-storing.

### Gedetailleerde beschrijving

Als de WebSocket-verbinding verbroken wordt (netwerk weggevallen, HA herstart, time-out), toont het scherm een overlay of een duidelijke statusbalk met de melding "Verbinding verbroken — proberen te herstellen...". Op de achtergrond probeert de app met exponentiële back-off opnieuw verbinding te maken (eerste poging na 5 s, daarna 10 s, 20 s, max. 60 s interval). Zodra de verbinding hersteld is, verdwijnt de melding, worden alle entiteitsstatussen ververst en hervat de app normaal werking.

### Acceptatiecriteria

- [x] Bij verbindingsverlies verschijnt binnen 3 seconden een duidelijke foutmelding op het scherm.
- [x] De foutmelding toont minimaal: "Verbinding verbroken" en een indicator dat er geprobeerd wordt te herstellen.
- [x] De app probeert automatisch opnieuw verbinding te maken met exponentiële back-off (5 s → 10 s → 20 s → max. 60 s).
- [x] Na succesvolle herverbinding worden alle entiteitsstatussen opnieuw opgehaald.
- [x] Na herverbinding verdwijnt de foutmelding en is de app direct bruikbaar.
- [x] De app onderscheidt WiFi-problemen van HA-problemen en toont een passende melding.
- [x] Het apparaat herverbindt ook na een HA-herstart zonder handmatige interventie.

### Technische opmerkingen

- WebSocket `on_close`- en `on_error`-callbacks triggeren de foutmelding en herverbindingslogica.
- WiFi-status bewaken via `WiFi.status()` (Arduino) of `esp_wifi_sta_get_ap_info()` (ESP-IDF).
- Exponentiële back-off: gebruik een timer met verdubbeling van interval tot een maximum van 60 s.

---

## US-010 — Statuspagina bekijken

**Als** beheerder of gebruiker van het apparaat  
**wil ik** een statuspagina kunnen openen die de verbindingsstatus, firmware-versie en apparaatinformatie toont  
**zodat** ik snel kan controleren of alles correct werkt en welke versie van de software actief is.

### Gedetailleerde beschrijving

De app heeft naast de HA-viewpagina en het selectiemenu ook een statuspagina. Deze is bereikbaar via een knop in het navigatiemenu (bijv. een tandwiel- of info-pictogram). De statuspagina toont: WiFi-signaalsterkte (RSSI), HA-verbindingsstatus (verbonden / verbroken), IP-adres van het apparaat, firmware-versie, uptime van het apparaat en de naam van de actief geladen HA-view. Vanuit de statuspagina kan de beheerder ook naar de configuratiepagina (US-001) navigeren.

### Acceptatiecriteria

- [x] De statuspagina is bereikbaar via één tik vanuit het navigatiemenu.
- [x] De pagina toont: WiFi-SSID, signaalsterkte (dBm of balkjes), IP-adres, HA-verbindingsstatus, firmware-versie, uptime en actieve view-naam.
- [x] WiFi-signaalsterkte wordt visueel weergegeven (bijv. 0–4 balkjes).
- [x] HA-verbindingsstatus toont "Verbonden" (groen) of "Verbroken" (rood).
- [x] Uptime wordt weergegeven in leesbaar formaat (bijv. "2d 4u 12m").
- [x] Vanuit de statuspagina is de configuratiepagina bereikbaar via een knop "Instellingen".
- [x] De statuspagina ververst de getoonde waarden automatisch elke 5 seconden.

### Technische opmerkingen

- Uptime via `esp_timer_get_time()` (ESP-IDF) of `millis()` (Arduino).
- WiFi RSSI via `WiFi.RSSI()`.
- IP-adres via `WiFi.localIP()`.

---

## US-011 — Airco bedienen: mode en ventilatiesnelheid

**Als** gebruiker van het apparaat
**wil ik** van een climate-entiteit (airco) naast de doeltemperatuur ook de HVAC-mode en de ventilatiesnelheid kunnen instellen
**zodat** ik de airco volledig vanaf het scherm kan bedienen zonder de Home Assistant-app te openen.

### Gedetailleerde beschrijving

Een airco is in Home Assistant een `climate.*`-entiteit. De HVAC-mode (bijv. `off`, `cool`, `heat`, `dry`, `fan_only`) is de *state* van de entiteit; de beschikbare modes staan in het attribuut `hvac_modes`. De ventilatiesnelheid staat in het attribuut `fan_mode` met de beschikbare standen in `fan_modes` (bijv. `auto`, `low`, `medium`, `high`). De climate-rij toont naast de temperatuur-slider twee compacte cycle-knoppen die de huidige waarde tonen; een tik springt naar de volgende stand uit de lijst. Instellen gaat via de services `climate.set_hvac_mode` en `climate.set_fan_mode`.

### Acceptatiecriteria

- [x] Een climate-rij toont twee cycle-knoppen: HVAC-mode en ventilatiesnelheid, naast de temperatuur-slider.
- [x] Een tik op de mode-knop schakelt naar de volgende mode uit `hvac_modes` via `climate.set_hvac_mode`.
- [x] Een tik op de fan-knop schakelt naar de volgende stand uit `fan_modes` via `climate.set_fan_mode`.
- [x] De knoppen tonen altijd de actuele waarde, ook na een wijziging vanuit HA zelf (realtime update).
- [x] `hvac_modes`, `fan_modes` en `fan_mode` worden geparsed uit de `get_states`- en `state_changed`-berichten.
- [x] Een climate-entiteit zonder `fan_modes` toont een neutrale fan-knop die niets doet.

### Technische opmerkingen

- Services: `climate.set_hvac_mode` (param `hvac_mode`) en `climate.set_fan_mode` (param `fan_mode`) — string-parameters, vereist uitbreiding van `ha_messages_call_service`.
- `entity_t` uitbreiden met `fan_mode`, `fan_modes[]` en `hvac_modes[]`; events dragen deze mee.
- Standaard fan modes in HA: `on`, `off`, `auto`, `low`, `medium`, `high`, `middle`, `focus`, `diffuse`; integraties mogen eigen waarden toevoegen — de lijst dus altijd uit het attribuut lezen, nooit hardcoden.

---

---

## US-012 — Actieve views kiezen in de instellingen

**Als** gebruiker van het apparaat
**wil ik** in de instellingen alle views uit Home Assistant zien en kunnen aanvinken welke ik op het apparaat wil gebruiken
**zodat** het keuzemenu alleen relevante views toont en het apparaat bij één actieve view direct die view opent na het opstarten.

### Gedetailleerde beschrijving

Op de statuspagina staat naast "Instellingen" een knop "Views" die een instellingenscherm opent met alle views uit de Lovelace-config, elk met een checkbox. De selectie wordt direct opgeslagen in NVS (`enabled_views`, kommagescheiden paden). Het view-keuzemenu toont alleen aangevinkte views; is er niets aangevinkt (of bestaan de aangevinkte views niet meer in HA) dan worden alle views getoond. Is er precies één view aangevinkt, dan laadt het apparaat die view direct na het verbinden en wordt het keuzemenu overgeslagen.

### Acceptatiecriteria

- [x] De statuspagina heeft een knop "Views" die het view-instellingenscherm opent.
- [x] Het scherm toont alle views uit Home Assistant met een checkbox per view.
- [x] Een wijziging van de selectie wordt direct opgeslagen (NVS `enabled_views`).
- [x] Het view-keuzemenu toont alleen aangevinkte views; geen (geldige) selectie betekent alle views.
- [x] Bij precies één aangevinkte view wordt na het opstarten direct die view geladen en het keuzemenu overgeslagen.
- [x] Bij meerdere aangevinkte views verschijnt het keuzemenu met alleen die views.

### Technische opmerkingen

- Nieuw scherm `ui_view_settings.c/h`; helpers `view_settings_is_enabled()` en `view_settings_enabled_count()` lezen alleen NVS (geen LVGL).
- Views zonder pad (URL-veld leeg in HA) krijgen bij het parsen een synthetisch pad `#<index>` en zijn daarmee gewoon kiesbaar, aanvinkbaar en laadbaar. Let op: de index verschuift als views in HA herordend worden; een URL instellen in HA is stabieler.
- De boot-logica zit in `app_events.c` (HA_EVT_CONNECTED): bij één actieve view wordt die als `selected_view` opgeslagen en direct geladen.

---

*Gegenereerd op: 2026-06-27 | Status: concept*
