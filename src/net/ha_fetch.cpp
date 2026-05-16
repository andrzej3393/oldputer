#include "ha_fetch.h"

#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <math.h>

#include "../core/activity_led.h"
#include "../core/app_config.h"
#include "../sensors/bme280_sensor.h"
#include "../ui/main_app.h"
#include "wifi_settings.h"

namespace HaFetch {

namespace {

constexpr uint32_t kRequestTimeoutMs = 6000;
constexpr const char* kSnapshotDelimiter = "|~|";
constexpr int kSnapshotDelimiterLength = 3;
constexpr size_t kSnapshotFieldCount = 19;

enum SnapshotFieldIndex : size_t {
  kFieldTime = 0,
  kFieldDate,
  kFieldNextRising,
  kFieldNextSetting,
  kFieldSunState,
  kFieldOutTemp,
  kFieldOutHum,
  kFieldOutAqi,
  kFieldOutWeather,
  kFieldInTemp,
  kFieldInHum,
  kFieldInPressure,
  kFieldInPm25,
  kFieldTodayMax,
  kFieldTodayMin,
  kFieldTodayWeather,
  kFieldTomorrowMax,
  kFieldTomorrowMin,
  kFieldTomorrowWeather,
};

uint32_t lastFetchMs = 0;
String configKey;
bool apiConnected = false;

String stripTrailingSlash(const String& input) {
  String out = input;
  out.trim();
  while (out.endsWith("/")) {
    out.remove(out.length() - 1);
  }
  return out;
}

String escapeForTemplateSingleQuotes(const String& input) {
  String out;
  out.reserve(input.length() + 8);
  for (size_t i = 0; i < input.length(); ++i) {
    const char c = input.charAt(i);
    if (c == '\\' || c == '\'') {
      out += '\\';
    }
    out += c;
  }
  return out;
}

String escapeForJson(const String& input) {
  String out;
  out.reserve(input.length() + 16);
  for (size_t i = 0; i < input.length(); ++i) {
    const char c = input.charAt(i);
    if (c == '\\' || c == '"') {
      out += '\\';
      out += c;
      continue;
    }
    if (c == '\n') {
      out += "\\n";
      continue;
    }
    if (c == '\r') {
      out += "\\r";
      continue;
    }
    if (c == '\t') {
      out += "\\t";
      continue;
    }
    out += c;
  }
  return out;
}

String unescapeJsonString(const String& input) {
  String out;
  out.reserve(input.length());

  bool escaped = false;
  for (size_t i = 0; i < input.length(); ++i) {
    const char c = input.charAt(i);
    if (!escaped) {
      if (c == '\\') {
        escaped = true;
      } else {
        out += c;
      }
      continue;
    }

    escaped = false;
    if (c == 'n') {
      out += '\n';
    } else if (c == 'r') {
      out += '\r';
    } else if (c == 't') {
      out += '\t';
    } else if (c == '"' || c == '\\' || c == '/') {
      out += c;
    } else {
      out += c;
    }
  }

  if (escaped) {
    out += '\\';
  }

  return out;
}

void parseEntitySelector(const String& selector, String* outEntityId, String* outAttribute) {
  String trimmed = selector;
  trimmed.trim();

  const int hashIndex = trimmed.indexOf('#');
  if (hashIndex < 0) {
    *outEntityId = trimmed;
    *outAttribute = String();
    return;
  }

  *outEntityId = trimmed.substring(0, hashIndex);
  *outAttribute = trimmed.substring(hashIndex + 1);
  outEntityId->trim();
  outAttribute->trim();
}

String buildSelectorTemplateExpression(const String& selector) {
  String entity;
  String attribute;
  parseEntitySelector(selector, &entity, &attribute);
  if (entity.isEmpty()) {
    return "''";
  }

  const String escapedEntity = escapeForTemplateSingleQuotes(entity);
  if (attribute.isEmpty()) {
    return "states('" + escapedEntity + "')";
  }

  const String escapedAttribute = escapeForTemplateSingleQuotes(attribute);
  return "state_attr('" + escapedEntity + "', '" + escapedAttribute + "')";
}

String buildConfigKey(const AppSettings& s) {
  return s.homeAssistantAddress + "|" + s.homeAssistantToken + "|" +
         s.outsideTemperatureEntityId + "|" + s.outsideHumidityEntityId + "|" +
         s.outsideAqiEntityId + "|" + s.outsideWeatherEntityId + "|" +
         s.insideTemperatureEntityId + "|" + s.insideHumidityEntityId + "|" + s.insidePressureEntityId + "|" + s.insidePm25EntityId + "|" +
         s.todayMaxTemperatureEntityId + "|" + s.todayMinTemperatureEntityId + "|" +
         s.todayWeatherEntityId + "|" +
         s.tomorrowMaxTemperatureEntityId + "|" + s.tomorrowMinTemperatureEntityId + "|" +
         s.tomorrowWeatherEntityId + "|" +
         (s.useInternalBmeForInside ? "1" : "0");
}

bool isInvalidValue(const String& value) {
  String v = value;
  v.trim();
  v.toLowerCase();
  return v.isEmpty() || v == "unknown" || v == "unavailable" || v == "none";
}

bool postTemplate(const String& baseUrl, const String& token, const String& templateExpression, String* outPayload) {
  ActivityLed::ScopedNetworkActivity networkActivity;

  WiFiClientSecure secureClient;
  secureClient.setInsecure();

  HTTPClient http;
  const String url = baseUrl + "/api/template";
  if (!http.begin(secureClient, url)) {
    return false;
  }

  const String body = String("{\"template\":\"") + escapeForJson(templateExpression) + "\"}";

  http.setTimeout(kRequestTimeoutMs);
  http.addHeader("Authorization", String("Bearer ") + token);
  http.addHeader("Content-Type", "application/json");

  const int statusCode = http.POST(body);
  if (statusCode != 200) {
    http.end();
    return false;
  }

  String payload = http.getString();
  http.end();

  payload.trim();
  if (payload.length() >= 2 && payload.startsWith("\"") && payload.endsWith("\"")) {
    payload.remove(payload.length() - 1);
    payload.remove(0, 1);
    payload = unescapeJsonString(payload);
  }

  *outPayload = payload;
  return true;
}

void appendTemplateField(String* outExpression, const String& fieldExpression, bool* firstField) {
  if (!*firstField) {
    *outExpression += kSnapshotDelimiter;
  }
  *firstField = false;
  *outExpression += "{{ ";
  *outExpression += fieldExpression;
  *outExpression += " }}";
}

String buildSnapshotTemplate(const AppSettings& settings) {
  String expression;
  expression.reserve(2600);

  bool firstField = true;
  appendTemplateField(&expression, "now().strftime('%H:%M')", &firstField);
  appendTemplateField(&expression, "now().strftime('%Y-%m-%d')", &firstField);
  appendTemplateField(
      &expression,
      "(as_timestamp(state_attr('sun.sun','next_rising')) | timestamp_custom('%Y-%m-%d %H:%M', true)) if state_attr('sun.sun','next_rising') else ''",
      &firstField);
  appendTemplateField(
      &expression,
      "(as_timestamp(state_attr('sun.sun','next_setting')) | timestamp_custom('%Y-%m-%d %H:%M', true)) if state_attr('sun.sun','next_setting') else ''",
      &firstField);
  appendTemplateField(&expression, "states('sun.sun')", &firstField);

  appendTemplateField(&expression, buildSelectorTemplateExpression(settings.outsideTemperatureEntityId), &firstField);
  appendTemplateField(&expression, buildSelectorTemplateExpression(settings.outsideHumidityEntityId), &firstField);
  appendTemplateField(&expression, buildSelectorTemplateExpression(settings.outsideAqiEntityId), &firstField);
  appendTemplateField(&expression, buildSelectorTemplateExpression(settings.outsideWeatherEntityId), &firstField);

  appendTemplateField(&expression, buildSelectorTemplateExpression(settings.insideTemperatureEntityId), &firstField);
  appendTemplateField(&expression, buildSelectorTemplateExpression(settings.insideHumidityEntityId), &firstField);
  appendTemplateField(&expression, buildSelectorTemplateExpression(settings.insidePressureEntityId), &firstField);
  appendTemplateField(&expression, buildSelectorTemplateExpression(settings.insidePm25EntityId), &firstField);

  appendTemplateField(&expression, buildSelectorTemplateExpression(settings.todayMaxTemperatureEntityId), &firstField);
  appendTemplateField(&expression, buildSelectorTemplateExpression(settings.todayMinTemperatureEntityId), &firstField);
  appendTemplateField(&expression, buildSelectorTemplateExpression(settings.todayWeatherEntityId), &firstField);

  appendTemplateField(&expression, buildSelectorTemplateExpression(settings.tomorrowMaxTemperatureEntityId), &firstField);
  appendTemplateField(&expression, buildSelectorTemplateExpression(settings.tomorrowMinTemperatureEntityId), &firstField);
  appendTemplateField(&expression, buildSelectorTemplateExpression(settings.tomorrowWeatherEntityId), &firstField);

  return expression;
}

bool parseSnapshotPayload(const String& payload, String* outValues, size_t valueCount) {
  int start = 0;

  for (size_t i = 0; i < valueCount; ++i) {
    const int separator = payload.indexOf(kSnapshotDelimiter, start);
    if (separator < 0) {
      if (i != valueCount - 1) {
        return false;
      }
      outValues[i] = payload.substring(start);
      return true;
    }

    outValues[i] = payload.substring(start, separator);
    start = separator + kSnapshotDelimiterLength;
  }

  return payload.indexOf(kSnapshotDelimiter, start) < 0;
}

String formatTemperature(const String& state) {
  if (isInvalidValue(state)) return "--";
  return String(state.toFloat(), 1) + " °C";
}

String formatHumidity(const String& state) {
  if (isInvalidValue(state)) return "--";
  return String(static_cast<int>(roundf(state.toFloat()))) + " %";
}

String formatPressure(const String& state) {
  if (isInvalidValue(state)) return "--";
  return String(static_cast<int>(roundf(state.toFloat()))) + " hPa";
}

String formatAqi(const String& state) {
  if (isInvalidValue(state)) return "--";
  const int value = static_cast<int>(roundf(state.toFloat()));
  char buffer[5];
  snprintf(buffer, sizeof(buffer), "%3d", value);
  return String(buffer);
}

String formatPm25(const String& state) {
  if (isInvalidValue(state)) return "--";
  return state;
}

String formatWeather(const String& state) {
  if (isInvalidValue(state)) {
    return "unknown";
  }
  String weather = state;
  weather.trim();
  return weather;
}

void setInsideFromBmeCacheIfEnabled(const AppSettings& settings) {
  if (!settings.useInternalBmeForInside) {
    return;
  }

  float bmeTemp = NAN;
  float bmeHum = NAN;
  float bmePressure = NAN;

  if (Bme280Sensor::getTemperatureC(&bmeTemp)) {
    MainApp::setInsideTemperature(formatTemperature(String(bmeTemp, 1)));
  } else {
    MainApp::setInsideTemperature("--");
  }

  if (Bme280Sensor::getHumidity(&bmeHum)) {
    MainApp::setInsideHumidity(formatHumidity(String(bmeHum, 1)));
  } else {
    MainApp::setInsideHumidity("--");
  }

  if (Bme280Sensor::getPressureHpa(&bmePressure)) {
    MainApp::setInsidePressure(formatPressure(String(bmePressure, 1)));
  } else {
    MainApp::setInsidePressure("--");
  }
}

void applySnapshot(const String* fields, const AppSettings& settings) {
  MainApp::setTimeAndDate(fields[kFieldTime], fields[kFieldDate]);
  MainApp::setSunSchedule(fields[kFieldNextRising], fields[kFieldNextSetting], fields[kFieldSunState]);

  MainApp::setOutsideTemperature(formatTemperature(fields[kFieldOutTemp]));
  MainApp::setOutsideHumidity(formatHumidity(fields[kFieldOutHum]));
  MainApp::setOutsideAqi(formatAqi(fields[kFieldOutAqi]));
  MainApp::setOutsideWeather(formatWeather(fields[kFieldOutWeather]));

  if (!settings.useInternalBmeForInside) {
    MainApp::setInsideTemperature(formatTemperature(fields[kFieldInTemp]));
    MainApp::setInsideHumidity(formatHumidity(fields[kFieldInHum]));
    MainApp::setInsidePressure(formatPressure(fields[kFieldInPressure]));
  }
  MainApp::setInsidePm25(formatPm25(fields[kFieldInPm25]));

  MainApp::setTodayMaxTemperature(formatTemperature(fields[kFieldTodayMax]));
  MainApp::setTodayMinTemperature(formatTemperature(fields[kFieldTodayMin]));
  MainApp::setTodayWeather(formatWeather(fields[kFieldTodayWeather]));

  MainApp::setTomorrowMaxTemperature(formatTemperature(fields[kFieldTomorrowMax]));
  MainApp::setTomorrowMinTemperature(formatTemperature(fields[kFieldTomorrowMin]));
  MainApp::setTomorrowWeather(formatWeather(fields[kFieldTomorrowWeather]));
}

void fetchAndApply() {
  const AppSettings& settings = WifiSettings::getAppSettings();
  setInsideFromBmeCacheIfEnabled(settings);

  const String baseUrl = stripTrailingSlash(settings.homeAssistantAddress);
  const String token = settings.homeAssistantToken;
  if (baseUrl.isEmpty() || token.isEmpty()) {
    apiConnected = false;
    return;
  }

  const String templateExpression = buildSnapshotTemplate(settings);

  String payload;
  if (!postTemplate(baseUrl, token, templateExpression, &payload)) {
    apiConnected = false;
    return;
  }

  String fields[kSnapshotFieldCount];
  if (!parseSnapshotPayload(payload, fields, kSnapshotFieldCount)) {
    apiConnected = false;
    return;
  }

  applySnapshot(fields, settings);
  apiConnected = true;
}

}  // namespace

void begin() {
  lastFetchMs = 0;
  configKey = "";
  apiConnected = false;
}

void loop() {
  if (!WifiSettings::isStationConnected()) {
    apiConnected = false;
    return;
  }

  const AppSettings& settings = WifiSettings::getAppSettings();
  const String nextConfigKey = buildConfigKey(settings);
  if (nextConfigKey != configKey) {
    configKey = nextConfigKey;
    lastFetchMs = 0;
  }

  if (lastFetchMs != 0 && millis() - lastFetchMs < AppConfig::kHaFetchIntervalMs) {
    return;
  }

  fetchAndApply();
  lastFetchMs = millis();
}

bool isApiConnected() {
  return apiConnected;
}

}  // namespace HaFetch
