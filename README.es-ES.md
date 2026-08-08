

# Estación Meteorológica ESP32 ePaper

Firmware para estación meteorológica ESP32 DevKit + pantalla ePaper B/N de 4.2" WeAct.

Nota: este proyecto está **totalmente programado con vibes** (junto con este readme). Aun así, ha funcionado a la perfección durante varias semanas.

![Startup GIF](/images/startup.gif)

## Funcionalidades

- Configuración de WiFi mediante AP de ESP32 + portal web de configuración.
- Obtención de datos meteorológicos y de tiempo desde Home Assistant.
- Fuente de datos interior opcional desde el BME280 integrado (temp./humedad/presión).
- Publicación de temp./humedad del BME280 vía MQTT con auto-descubrimiento de Home Assistant.
- Renderiza un panel de control en disposición 2x2 con iconos meteorológicos EXTERIORES para día/noche.
- Muestra el próximo evento solar en el panel de tiempo (`SUNSET HH:MM` durante el día, `SUNRISE HH:MM` por la noche).

## Carcasa

El modelo de la carcasa listo para imprimir, así como el proyecto fuente de FreeCAD, están disponibles en [Makerworld](https://makerworld.com/en/models/2810592-oldputer).

## Hardware

- Placa: ESP32 DevKit
- Pantalla: ePaper B/N de 4.2" WeAct (`GDEY042T81` vía GxEPD2)
- Sensor: BME280 (I2C)
- LED de actividad: activo en bajo (a GND a través de pin GPIO)

### Mapa de Pines

Configurado en `src/core/app_config.h`:

- ePaper:
  - `CS`: GPIO5
  - `BUSY`: GPIO4
  - `RST`: GPIO17
  - `DC`: GPIO16
- I2C (BME280):
  - `SDA`: GPIO21
  - `SCL`: GPIO22
- LED de actividad:
  - `GPIO25` (activo en bajo)

## Herramientas Y Bibliotecas

- Herramienta de compilación: `arduino-cli`
- Servidor de lenguaje: Arduino Language Server (ver `AGENTS.md`)
- Bibliotecas:
  - `GxEPD2`
  - `Adafruit GFX Library`
  - `Adafruit BusIO`
  - `Adafruit Unified Sensor`
  - `Adafruit BME280 Library`
  - `PubSubClient`

`sketch.yaml` contiene el perfil predeterminado: `weather_station`.

## Configuración Inicial (Única)

```bash
arduino-cli core update-index
arduino-cli lib update-index
arduino-cli core install esp32:esp32
arduino-cli lib install "GxEPD2" "Adafruit GFX Library" "Adafruit BusIO" "Adafruit Unified Sensor" "Adafruit BME280 Library" "PubSubClient"
```

## Compilar

Recomendado (usa dependencias fijas del perfil `weather_station` de `sketch.yaml`):

```bash
arduino-cli compile --profile weather_station
```

Alternativa manual (partición de aplicación grande explícita):

```bash
arduino-cli compile --fqbn esp32:esp32:esp32:PartitionScheme=huge_app,CPUFreq=160,FlashMode=qio,FlashFreq=80,PSRAM=disabled,DebugLevel=none
```

## Grabar / Flash

Buscar puerto de la placa:

```bash
arduino-cli board list
```

Subir (reemplazar `PORT`):

```bash
arduino-cli upload --fqbn esp32:esp32:esp32:PartitionScheme=huge_app,CPUFreq=160,FlashMode=qio,FlashFreq=80,PSRAM=disabled,DebugLevel=none --port PORT
```

Monitor serial:

```bash
arduino-cli monitor -p PORT -c baudrate=115200
```

## Primer Inicio Y Configuración

- Si faltan las credenciales de WiFi o falla la conexión, el ESP inicia el AP de configuración.
- Conéctate al AP y luego abre `http://<AP_IP>` en el navegador (solo HTTP, no HTTPS).
- El portal usa Autenticación Básica HTTP:
  - usuario: `admin`
  - contraseña: contraseña actual del AP de configuración
- Guarda la configuración en el portal web. El formulario redirige a `/` después de guardar.
- Si el SSID/contraseña de WiFi cambiaron, el firmware reconecta el WiFi; de lo contrario, mantiene la conexión actual.

## Secciones De Configuración Web

Orden en el portal:

1. WiFi
2. Pantalla
3. API de Home Assistant
4. MQTT
5. Interior
6. Exterior
7. Hoy
8. Mañana

Las líneas de estado (`connected` / `disconnected`) se muestran en:

- WiFi
- API de Home Assistant
- MQTT

Comportamiento de campos sensibles:

- La contraseña de WiFi / token de HA / contraseña de MQTT nunca se prellenarán en el formulario.
- Dejar estos campos vacíos conserva el valor almacenado actualmente.
- El token de HA y la contraseña de MQTT pueden borrarse explícitamente con casillas de verificación.

## Fuentes De Datos Y Comportamiento

### Obtención Desde Home Assistant

- Intervalo de obtención: cada 10 minutos (`AppConfig::kHaFetchIntervalMs`).
- Usa una única solicitud de instantánea `/api/template` de Home Assistant por ciclo de obtención para todos los valores mostrados.
- Entre obtenciones, el tiempo mostrado avanza localmente cada minuto.
- Las secciones de pronóstico son `Today` y `Tomorrow`.
- La programación de amanecer/atardecer proviene de `sun.sun` (`next_rising`, `next_setting`, estado).

### Valores De Interior

- Casilla en sección `Inside`: `Usar BME280 interno para temp./humedad/presión interior`.
- Cuando está habilitado:
  - Los datos de temp./humedad/presión INTERIORES provienen de la caché del BME280.
  - Las entidades de HA de temp./humedad/presión interior se ignoran.
- Cuando está deshabilitado:
  - Los valores INTERIORES provienen de las entidades de HA configuradas.

### Valores De Exterior

- El panel EXTERIOR muestra: temperatura, humedad, ICA (AQI), icono meteorológico.
- La presión ya no se muestra en el panel EXTERIOR.

## MQTT (BME280)

- Intervalo de muestreo BME280: 60 segundos.
- MQTT es opcional; habilítalo estableciendo `MQTT host`.
- Predeterminados (si están vacíos):
  - Tema base: `esp32/epaper`
  - Prefijo de descubrimiento: `homeassistant`
- Publica descubrimiento para dos sensores:
  - Temperatura Interior
  - Humedad Interior
- Tema de carga de estado: `<base_topic>/state` con JSON:

```json
{"temperature": 23.45, "humidity": 46.78}
```

## Comportamiento De Pantalla/Actualización

- Pantallas de configuración: siempre en modo negativo (blanco sobre negro).
- Imagen de bienvenida: siempre en modo positivo (negro sobre blanco), se muestra antes de la pantalla principal.
- Pantalla principal: sigue la configuración `Invert display` del panel web.
- Política de actualización de configuración:
  - actualización completa una vez al inicio de la configuración,
  - luego actualización parcial.
- Política de actualización principal:
  - actualizaciones parciales con actualización completa periódica (`AppConfig::kMainFullRefreshEveryN`).

## Comportamiento Del LED De Actividad

- LED activo en bajo en GPIO25.
- Encendido fijo hasta que se conecte el WiFi.
- Parpadeo estilo HDD con jitter agresivo ante actividad de red.
- También parpadea durante las actualizaciones de la pantalla principal (parcial/completa), no en las pantallas de configuración.

## Controles De Configuración

Las constantes a nivel de placa y de tiempo de ejecución están en `src/core/app_config.h`, incluyendo:

- pines,
- comportamiento de actualización de configuración/completa,
- intervalo de obtención de HA,
- duración de la bienvenida,
- parámetros de temporización/jitter de actividad del LED.

## Sensores De Plantilla De Pronóstico De Home Assistant

Usa `docs/HA_TEMPLATE_SENSORS.md` para las entidades de plantilla de pronóstico `Today` / `Tomorrow`.

## Estructura Del Proyecto

- `oldputer.ino` - punto de entrada del sketch y máquina de estados de pantalla.
- `src/core/` - configuración/ajustes centrales y LED de actividad.
- `src/net/` - estado de WiFi, portal de configuración, obtención de Home Assistant.
- `src/ui/` - panel principal y pantallas de configuración.
- `src/sensors/` - módulo BME280 + publicación MQTT.
- `src/assets/` - encabezados de mapas de bits/iconos.
- `docs/` - documentación de soporte (plantillas HA).

## Atribución

- Los iconos meteorológicos provienen de `manifestinteractive/weather-underground-icons` (MIT):
  - `https://github.com/manifestinteractive/weather-underground-icons`
- Imagen fuente del logotipo de la pantalla de configuración:
  - `https://manzdev.github.io/twitch-manzdev-bios/assets/epa.png`

## Nota Útil De Recuperación

Si cambias el esquema de particiones en una placa ya grabada, realiza una subida con borrado completo una vez:

```bash
arduino-cli upload --fqbn esp32:esp32:esp32:PartitionScheme=huge_app,CPUFreq=160,FlashMode=qio,FlashFreq=80,PSRAM=disabled,DebugLevel=none,EraseFlash=all --port PORT
```

Luego regresa a las subidas normales sin `EraseFlash=all`.
