# ESP32 ePaper Weather Station

ESP32 DevKit + WeAct 4.2" BW ePaper weather station firmware.

Note: this project is **fully vibe coded** (along with this readme). Nevertheless, it runs flawlessly for a few weeks already.

![Startup GIF](/images/startup.gif)

## What It Does

- WiFi provisioning via ESP32 AP + web config portal.
- Pulls weather/time data from Home Assistant.
- Optional inside data source from onboard BME280 (temp/humidity/pressure).
- Publishes BME280 temp/humidity via MQTT with Home Assistant auto-discovery.
- Renders a 2x2 dashboard layout with day/night OUTSIDE weather icons.
- Shows next sun event in time panel (`SUNSET HH:MM` during day, `SUNRISE HH:MM` at night).

## Hardware

- Board: ESP32 DevKit
- Display: WeAct 4.2" black/white ePaper (`GDEY042T81` via GxEPD2)
- Sensor: BME280 (I2C)
- Activity LED: active-low (to GND through GPIO pin)

### Pin Map

Configured in `src/core/app_config.h`:

- ePaper:
  - `CS`: GPIO5
  - `BUSY`: GPIO4
  - `RST`: GPIO17
  - `DC`: GPIO16
- I2C (BME280):
  - `SDA`: GPIO21
  - `SCL`: GPIO22
- Activity LED:
  - `GPIO25` (active-low)

## Tooling And Libraries

- Build tool: `arduino-cli`
- Language server: Arduino Language Server (see `AGENTS.md`)
- Libraries:
  - `GxEPD2`
  - `Adafruit GFX Library`
  - `Adafruit BusIO`
  - `Adafruit Unified Sensor`
  - `Adafruit BME280 Library`
  - `PubSubClient`

`sketch.yaml` contains the default profile: `weather_station`.

## One-Time Setup

```bash
arduino-cli core update-index
arduino-cli lib update-index
arduino-cli core install esp32:esp32
arduino-cli lib install "GxEPD2" "Adafruit GFX Library" "Adafruit BusIO" "Adafruit Unified Sensor" "Adafruit BME280 Library" "PubSubClient"
```

## Compile

Recommended (uses pinned dependencies from `sketch.yaml` profile `weather_station`):

```bash
arduino-cli compile --profile weather_station
```

Manual fallback (explicit huge app partition):

```bash
arduino-cli compile --fqbn esp32:esp32:esp32:PartitionScheme=huge_app,CPUFreq=160,FlashMode=qio,FlashFreq=80,PSRAM=disabled,DebugLevel=none
```

## Flash

Find board port:

```bash
arduino-cli board list
```

Upload (replace `PORT`):

```bash
arduino-cli upload --fqbn esp32:esp32:esp32:PartitionScheme=huge_app,CPUFreq=160,FlashMode=qio,FlashFreq=80,PSRAM=disabled,DebugLevel=none --port PORT
```

Serial monitor:

```bash
arduino-cli monitor -p PORT -c baudrate=115200
```

## First Boot And Provisioning

- If WiFi credentials are missing or connection fails, ESP starts setup AP.
- Connect to AP, then open `http://<AP_IP>` in browser (HTTP only, not HTTPS).
- Portal uses HTTP Basic Auth:
  - user: `admin`
  - password: current setup AP password
- Save config in web portal. The form redirects back to `/` after save.
- If WiFi SSID/password changed, firmware reconnects WiFi; otherwise it keeps current connection.

## Web Config Sections

Order in portal:

1. WiFi
2. Display
3. Home Assistant API
4. MQTT
5. Inside
6. Outside
7. Today
8. Tomorrow

Status lines (`connected` / `disconnected`) are shown in:

- WiFi
- Home Assistant API
- MQTT

Sensitive fields behavior:

- WiFi password / HA token / MQTT password are never pre-filled in the form.
- Leaving these fields empty keeps the currently stored value.
- HA token and MQTT password can be explicitly cleared with checkboxes.

## Data Sources And Behavior

### Home Assistant Pull

- Fetch interval: every 10 minutes (`AppConfig::kHaFetchIntervalMs`).
- Uses a single Home Assistant `/api/template` snapshot request per fetch cycle for all displayed values.
- Between fetches, displayed time advances locally every minute.
- Forecast sections are `Today` and `Tomorrow`.
- Sunrise/sunset scheduling comes from `sun.sun` (`next_rising`, `next_setting`, state).

### Inside Values

- Checkbox in `Inside` section: `Use internal BME280 for inside temp/humidity/pressure`.
- When enabled:
  - INSIDE temp/humidity/pressure come from BME280 cache.
  - HA inside temp/humidity/pressure entities are ignored.
- When disabled:
  - INSIDE values come from configured HA entities.

### Outside Values

- OUTSIDE panel displays: temperature, humidity, AQI, weather icon.
- Pressure is no longer shown in OUTSIDE panel.

## MQTT (BME280)

- BME280 sampling interval: 60 seconds.
- MQTT is optional; enable by setting `MQTT host`.
- Defaults (if empty):
  - Base topic: `esp32/epaper`
  - Discovery prefix: `homeassistant`
- Publishes discovery for two sensors:
  - Inside Temperature
  - Inside Humidity
- State payload topic: `<base_topic>/state` with JSON:

```json
{"temperature": 23.45, "humidity": 46.78}
```

## Display/Refresh Behavior

- Setup screens: always negative (white on black).
- Splash image: always positive (black on white), shown before main screen.
- Main screen: follows `Invert display` setting from web config.
- Setup refresh policy:
  - full refresh once at setup start,
  - then partial refresh.
- Main refresh policy:
  - partial refreshes with periodic full refresh (`AppConfig::kMainFullRefreshEveryN`).

## Activity LED Behavior

- Active-low LED on GPIO25.
- Solid ON until WiFi connects.
- Aggressive jittered HDD-style blinking on network activity.
- Also blinks during main-screen refreshes (partial/full), not on setup screens.

## Config Knobs

Board-level and runtime constants live in `src/core/app_config.h`, including:

- pins,
- setup/full refresh behavior,
- HA fetch interval,
- splash duration,
- LED activity timing/jitter parameters.

## Home Assistant Forecast Template Sensors

Use `docs/HA_TEMPLATE_SENSORS.md` for `Today` / `Tomorrow` forecast template entities.

## Project Layout

- `epaper-esp32.ino` - sketch entrypoint and screen state machine.
- `src/core/` - core config/settings/activity LED.
- `src/net/` - WiFi state, config portal, Home Assistant fetch.
- `src/ui/` - main dashboard and setup screens.
- `src/sensors/` - BME280 + MQTT publishing module.
- `src/assets/` - bitmap/icon headers.
- `docs/` - supporting documentation (HA templates).

## Attribution

- Weather icons are derived from `manifestinteractive/weather-underground-icons` (MIT):
  - `https://github.com/manifestinteractive/weather-underground-icons`
- Setup screen logo source image:
  - `https://manzdev.github.io/twitch-manzdev-bios/assets/epa.png`

## Useful Recovery Note

If you switch partition scheme on an already flashed board, do one full erase upload once:

```bash
arduino-cli upload --fqbn esp32:esp32:esp32:PartitionScheme=huge_app,CPUFreq=160,FlashMode=qio,FlashFreq=80,PSRAM=disabled,DebugLevel=none,EraseFlash=all --port PORT
```

Then return to normal uploads without `EraseFlash=all`.
