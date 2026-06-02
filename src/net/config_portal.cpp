#include "config_portal.h"

#include <WebServer.h>
#include <string.h>

#include "../core/app_config.h"
#include "../sensors/bme280_sensor.h"
#include "ha_fetch.h"
#include "wifi_settings.h"

namespace ConfigPortal {

namespace {

WebServer server(80);
Context context;
bool started = false;

String htmlEscape(const String& input) {
  String out;
  out.reserve(input.length() + 8);
  for (size_t i = 0; i < input.length(); ++i) {
    const char c = input[i];
    if (c == '&') {
      out += "&amp;";
    } else if (c == '<') {
      out += "&lt;";
    } else if (c == '>') {
      out += "&gt;";
    } else if (c == '"') {
      out += "&quot;";
    } else if (c == '\'') {
      out += "&#39;";
    } else {
      out += c;
    }
  }
  return out;
}

void sendRaw(const char* text) {
  server.sendContent(text, strlen(text));
}

void sendEscaped(const String& value) {
  if (value.isEmpty()) {
    return;
  }
  server.sendContent(htmlEscape(value));
}

void sendUIntValue(uint16_t value) {
  char buffer[8];
  snprintf(buffer, sizeof(buffer), "%u", static_cast<unsigned>(value));
  sendRaw(buffer);
}

void sendTextInput(const char* name, uint16_t maxLength, const String& value) {
  sendRaw("<input type='text' name='");
  sendRaw(name);
  sendRaw("' maxlength='");
  sendUIntValue(maxLength);
  sendRaw("' value='");
  sendEscaped(value);
  sendRaw("'>");
}

void sendTextInputRaw(const char* name, uint16_t maxLength, const char* value) {
  sendRaw("<input type='text' name='");
  sendRaw(name);
  sendRaw("' maxlength='");
  sendUIntValue(maxLength);
  sendRaw("' value='");
  sendRaw(value);
  sendRaw("'>");
}

void sendPasswordInput(const char* name, uint16_t maxLength) {
  sendRaw("<input type='password' name='");
  sendRaw(name);
  sendRaw("' maxlength='");
  sendUIntValue(maxLength);
  sendRaw("' value=''>");
}

void sendCheckbox(const char* name, bool checked, const char* label) {
  sendRaw("<label><input type='checkbox' name='");
  sendRaw(name);
  sendRaw("' value='1'");
  if (checked) {
    sendRaw(" checked");
  }
  sendRaw("> ");
  sendRaw(label);
  sendRaw("</label>");
}

void sendPage() {
  const AppSettings& settings = *context.settings;

  sendRaw("<!doctype html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'>");
  sendRaw("<title>ESP32 Configuration</title>");
  sendRaw("<style>body{font-family:Arial,sans-serif;max-width:720px;margin:2rem auto;padding:0 1rem;line-height:1.4;}");
  sendRaw("fieldset{margin:1rem 0;padding:1rem;border:1px solid #ccc;border-radius:6px;}");
  sendRaw("label{display:block;margin-top:0.8rem;}");
  sendRaw("input{display:block;width:100%;padding:0.6rem;margin:0.35rem 0 0;box-sizing:border-box;}");
  sendRaw("label input[type='checkbox']{display:inline-block;width:auto;padding:0;margin:0 0.5rem 0 0;vertical-align:middle;}");
  sendRaw("button{margin-top:1rem;padding:0.7rem 1rem;border:0;background:#0a6;color:#fff;border-radius:4px;cursor:pointer;}");
  sendRaw("small{color:#555;}");
  sendRaw("</style></head><body>");
  sendRaw("<h1>ESP32 Configuration</h1>");
  sendRaw("<form method='POST' action='/save'>");

  sendRaw("<fieldset><legend>WiFi</legend>");
  sendRaw("<p><small>Status: <strong>");
  sendRaw(WifiSettings::isStationConnected() ? "connected" : "disconnected");
  sendRaw("</strong></small></p>");
  sendRaw("<label>WiFi SSID</label>");
  sendTextInput("wifi_ssid", 32, settings.wifiSsid);
  sendRaw("<label>WiFi password</label>");
  sendPasswordInput("wifi_password", 64);
  sendRaw("<small>Leave blank to keep current password when SSID is unchanged.</small>");
  sendRaw("</fieldset>");

  sendRaw("<fieldset><legend>Display</legend>");
  sendCheckbox("invert_display", settings.invertDisplay, "Invert display colors");
  sendRaw("</fieldset>");

  sendRaw("<fieldset><legend>Home Assistant API</legend>");
  sendRaw("<p><small>Status: <strong>");
  sendRaw(HaFetch::isApiConnected() ? "connected" : "disconnected");
  sendRaw("</strong></small></p>");
  sendRaw("<label>Address</label>");
  sendTextInput("ha_address", 128, settings.homeAssistantAddress);
  sendRaw("<label>API token</label>");
  sendPasswordInput("ha_token", 256);
  sendRaw("<small>Leave blank to keep current token.</small>");
  sendCheckbox("clear_ha_token", false, "Clear stored token");
  sendRaw("</fieldset>");

  sendRaw("<fieldset><legend>MQTT</legend>");
  sendRaw("<p><small>Status: <strong>");
  sendRaw(Bme280Sensor::isMqttConnected() ? "connected" : "disconnected");
  sendRaw("</strong></small></p>");
  sendRaw("<label>MQTT host</label>");
  sendTextInput("mqtt_host", 128, settings.mqttHost);
  sendRaw("<label>MQTT port</label>");
  char mqttPort[8];
  snprintf(mqttPort, sizeof(mqttPort), "%u", static_cast<unsigned>(settings.mqttPort));
  sendTextInputRaw("mqtt_port", 6, mqttPort);
  sendRaw("<label>MQTT user</label>");
  sendTextInput("mqtt_user", 64, settings.mqttUser);
  sendRaw("<label>MQTT password</label>");
  sendPasswordInput("mqtt_pass", 64);
  sendRaw("<small>Leave blank to keep current password.</small>");
  sendCheckbox("clear_mqtt_pass", false, "Clear stored password");
  sendRaw("<label>MQTT base topic</label>");
  sendTextInput("mqtt_base", 128, settings.mqttBaseTopic);
  sendRaw("<label>MQTT discovery prefix</label>");
  sendTextInput("mqtt_disc", 64, settings.mqttDiscoveryPrefix);
  sendRaw("<small>Defaults: base topic esp32/epaper, discovery prefix homeassistant</small>");
  sendRaw("</fieldset>");

  sendRaw("<fieldset><legend>Inside</legend>");
  sendRaw("<label>Inside temperature entity id</label>");
  sendTextInput("inside_temp_entity", 128, settings.insideTemperatureEntityId);
  sendRaw("<label>Inside humidity entity id</label>");
  sendTextInput("inside_hum_entity", 128, settings.insideHumidityEntityId);
  sendRaw("<label>Inside pressure entity id</label>");
  sendTextInput("inside_pressure_entity", 128, settings.insidePressureEntityId);
  sendRaw("<label>Inside PM2.5 entity id</label>");
  sendTextInput("inside_pm25_entity", 128, settings.insidePm25EntityId);
  sendCheckbox("use_bme_internal", settings.useInternalBmeForInside, "Use internal BME280 for inside temp/humidity/pressure");
  sendRaw("</fieldset>");

  sendRaw("<fieldset><legend>Outside</legend>");
  sendRaw("<label>Outside temperature entity id</label>");
  sendTextInput("outside_temp_entity", 128, settings.outsideTemperatureEntityId);
  sendRaw("<label>Outside humidity entity id</label>");
  sendTextInput("outside_hum_entity", 128, settings.outsideHumidityEntityId);
  sendRaw("<label>Outside AQI entity id</label>");
  sendTextInput("outside_aqi_entity", 128, settings.outsideAqiEntityId);
  sendRaw("<label>Outside weather entity id</label>");
  sendTextInput("outside_weather_entity", 128, settings.outsideWeatherEntityId);
  sendRaw("</fieldset>");

  sendRaw("<fieldset><legend>Today</legend>");
  sendRaw("<label>Today max temp entity id</label>");
  sendTextInput("today_max_temp_entity", 128, settings.todayMaxTemperatureEntityId);
  sendRaw("<label>Today min temp entity id</label>");
  sendTextInput("today_min_temp_entity", 128, settings.todayMinTemperatureEntityId);
  sendRaw("<label>Today weather entity id</label>");
  sendTextInput("today_weather_entity", 128, settings.todayWeatherEntityId);
  sendRaw("</fieldset>");

  sendRaw("<fieldset><legend>Tomorrow</legend>");
  sendRaw("<label>Tomorrow max temp entity id</label>");
  sendTextInput("tomorrow_max_temp_entity", 128, settings.tomorrowMaxTemperatureEntityId);
  sendRaw("<label>Tomorrow min temp entity id</label>");
  sendTextInput("tomorrow_min_temp_entity", 128, settings.tomorrowMinTemperatureEntityId);
  sendRaw("<label>Tomorrow weather entity id</label>");
  sendTextInput("tomorrow_weather_entity", 128, settings.tomorrowWeatherEntityId);
  sendRaw("<small>Example: sensor.termometr_balkon_temperatura or sensor.weather#temperature</small>");
  sendRaw("</fieldset>");

  sendRaw("<button type='submit'>Save configuration</button>");
  sendRaw("</form>");
  sendRaw("<p><small>Setup AP SSID: ");
  sendEscaped(*context.apSsid);
  sendRaw("</small></p>");
  sendRaw("<p><small>Portal login user: admin (password is the current AP password)</small></p>");
  sendRaw("</body></html>");
}

bool ensureAuthorized() {
  if (server.authenticate(AppConfig::kPortalAuthUser, WifiSettings::getApPassword().c_str())) {
    return true;
  }
  server.requestAuthentication(BASIC_AUTH, "ESP32 Config", "Login required");
  return false;
}

void handleRoot() {
  if (!ensureAuthorized()) {
    return;
  }
  server.sendHeader("Cache-Control", "no-store");
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/html", "");
  sendPage();
  server.sendContent("");
}

String readArgWithFallback(const char* primary, const char* fallback) {
  String value = server.arg(primary);
  if (value.isEmpty()) {
    value = server.arg(fallback);
  }
  return value;
}

void trimConfigFields(AppSettings* settings) {
  settings->homeAssistantAddress.trim();
  settings->homeAssistantToken.trim();
  settings->outsideTemperatureEntityId.trim();
  settings->outsideHumidityEntityId.trim();
  settings->outsideAqiEntityId.trim();
  settings->outsideWeatherEntityId.trim();
  settings->insideTemperatureEntityId.trim();
  settings->insideHumidityEntityId.trim();
  settings->insidePressureEntityId.trim();
  settings->insidePm25EntityId.trim();
  settings->todayMaxTemperatureEntityId.trim();
  settings->todayMinTemperatureEntityId.trim();
  settings->todayWeatherEntityId.trim();
  settings->tomorrowMaxTemperatureEntityId.trim();
  settings->tomorrowMinTemperatureEntityId.trim();
  settings->tomorrowWeatherEntityId.trim();
  settings->mqttHost.trim();
  settings->mqttUser.trim();
  settings->mqttPassword.trim();
  settings->mqttBaseTopic.trim();
  settings->mqttDiscoveryPrefix.trim();
}

void handleSave() {
  if (!ensureAuthorized()) {
    return;
  }

  AppSettings next = *context.settings;
  next.wifiSsid = server.arg("wifi_ssid");
  const String wifiPasswordInput = server.arg("wifi_password");
  if (!wifiPasswordInput.isEmpty() || next.wifiSsid != context.settings->wifiSsid) {
    next.wifiPassword = wifiPasswordInput;
  }
  next.homeAssistantAddress = server.arg("ha_address");
  if (server.hasArg("clear_ha_token")) {
    next.homeAssistantToken = "";
  } else {
    const String haTokenInput = server.arg("ha_token");
    if (!haTokenInput.isEmpty()) {
      next.homeAssistantToken = haTokenInput;
    }
  }
  next.outsideTemperatureEntityId = server.arg("outside_temp_entity");
  next.outsideHumidityEntityId = server.arg("outside_hum_entity");
  next.outsideAqiEntityId = server.arg("outside_aqi_entity");
  next.outsideWeatherEntityId = server.arg("outside_weather_entity");
  next.insideTemperatureEntityId = server.arg("inside_temp_entity");
  next.insideHumidityEntityId = server.arg("inside_hum_entity");
  next.insidePressureEntityId = server.arg("inside_pressure_entity");
  next.insidePm25EntityId = server.arg("inside_pm25_entity");
  next.useInternalBmeForInside = server.hasArg("use_bme_internal");
  next.mqttHost = server.arg("mqtt_host");
  const uint16_t mqttPort = static_cast<uint16_t>(server.arg("mqtt_port").toInt());
  next.mqttPort = mqttPort == 0 ? 1883 : mqttPort;
  next.mqttUser = server.arg("mqtt_user");
  if (server.hasArg("clear_mqtt_pass")) {
    next.mqttPassword = "";
  } else {
    const String mqttPasswordInput = server.arg("mqtt_pass");
    if (!mqttPasswordInput.isEmpty()) {
      next.mqttPassword = mqttPasswordInput;
    }
  }
  next.mqttBaseTopic = server.arg("mqtt_base");
  next.mqttDiscoveryPrefix = server.arg("mqtt_disc");
  next.invertDisplay = server.hasArg("invert_display");
  next.todayMaxTemperatureEntityId = readArgWithFallback("today_max_temp_entity", "forecast_max_temp_entity");
  next.todayMinTemperatureEntityId = readArgWithFallback("today_min_temp_entity", "forecast_min_temp_entity");
  next.todayWeatherEntityId = readArgWithFallback("today_weather_entity", "forecast_weather_entity");
  next.tomorrowMaxTemperatureEntityId = server.arg("tomorrow_max_temp_entity");
  next.tomorrowMinTemperatureEntityId = server.arg("tomorrow_min_temp_entity");
  next.tomorrowWeatherEntityId = server.arg("tomorrow_weather_entity");
  trimConfigFields(&next);

  context.onSettingsSubmitted(next);
  server.sendHeader("Location", String("/"), true);
  server.send(303, "text/plain", "");
}

void handleNotFound() {
  if (!ensureAuthorized()) {
    return;
  }
  server.sendHeader("Location", String("/"), true);
  server.send(302, "text/plain", "");
}

}  // namespace

void begin(const Context& ctx) {
  if (started) {
    context = ctx;
    return;
  }

  context = ctx;

  server.on("/", HTTP_GET, handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.onNotFound(handleNotFound);
  server.begin();
  started = true;
}

void loop() {
  if (!started) {
    return;
  }
  server.handleClient();
}

}  // namespace ConfigPortal
