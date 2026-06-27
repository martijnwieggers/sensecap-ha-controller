# SenseCap Indicator — Home Assistant Controller

Bidirectionele Home Assistant controller voor de [Seeed Studio SenseCap Indicator](https://wiki.seeedstudio.com/SenseCAP_Indicator_Get_Started/) (4" touchscreen, ESP32-S3 + RP2040).

## Wat doet het?

- Haalt een Lovelace-view op uit Home Assistant via de WebSocket API
- Rendert entiteiten op het touchscreen op basis van hun type:
  - **Schakelaars** (`switch`, `input_boolean`, `light`) → toggle
  - **Dimmers** (`light` met brightness) → slider
  - **Thermostaten** (`climate`) → slider met °C
  - **Sensoren** (`sensor`, `binary_sensor`) → read-only label
- Realtime synchronisatie beide kanten op
- Swipe om te navigeren bij meer dan 6 entiteiten
- On-device setup wizard (WiFi, HA URL, token)

## Projectstructuur

```
firmware/       PlatformIO project — draait op de SenseCap Indicator
simulator/      CMake project — draait op de pc voor ontwikkeling en testen
documentatie/   User stories, technisch ontwerp, status
```

## Snel starten

### Simulator (pc)

**Vereisten:** CMake ≥ 3.20, een C-compiler (MSVC / MinGW / GCC), SDL2

**SDL2 installeren (Windows via vcpkg):**
```
git clone https://github.com/microsoft/vcpkg.git
cd vcpkg && bootstrap-vcpkg.bat
vcpkg install sdl2:x64-windows
```

**Bouwen:**
```bash
cd simulator
cmake -B build -DCMAKE_TOOLCHAIN_FILE=[vcpkg-root]/scripts/buildsystems/vcpkg.cmake
cmake --build build
./build/sensecap_sim
```

**Zonder echte HA:** de simulator start met nep-entiteiten (mock HA). Configureer `simulator/config.ini` voor verbinding met echte HA.

### Firmware (apparaat)

**Vereisten:** PlatformIO, ESP-IDF v5.1.x

```bash
cd firmware
pio run --target upload
```

## Technische stack

| Component        | Keuze                          |
|------------------|--------------------------------|
| UI-framework     | LVGL 8.3.3                     |
| HA-verbinding    | WebSocket API (`ws://`)        |
| JSON             | ArduinoJson v7                 |
| Opslag           | NVS (Preferences API)          |
| Build (apparaat) | PlatformIO + ESP-IDF v5.1.x    |
| Build (pc)       | CMake + SDL2                   |

## Documentatie

Zie de map [`documentatie/`](documentatie/) voor:
- [User Stories](documentatie/userstories.md)
- [Technisch Ontwerp](documentatie/technisch_ontwerp.md)
- [Technische Verkenning](documentatie/technische_verkenning.md)
- [Status](documentatie/status.md)
