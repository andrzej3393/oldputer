#include "bme280_sensor.h"

#include <Arduino.h>
#include <Adafruit_BME280.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <Wire.h>
#include <math.h>

#include "../core/activity_led.h"
#include "../core/app_config.h"
#include "../ui/main_app.h"
#include "../net/wifi_settings.h"

namespace Bme280Sensor {

namespace {

constexpr uint32_t kReadIntervalMs = 60000;
constexpr uint32_t kMqttReconnectIntervalMs = 5000;

Adafruit_BME280 bme;
bool sensorReady = false;
uint32_t lastReadMs = 0;
float lastTemperatureC = NAN;
float lastHumidityPct = NAN;
float lastPressureHpa = NAN;

WiFiClient mqttWiFiClient;
PubSubClient mqttClient(mqttWiFiClient);
uint32_t lastMqttReconnectMs = 0;
String mqttConfigKey;
String mqttClientId;
bool mqttWasConnected = false;
bool lastUseInternalBmeForInside = false;

void disconnectMqtt() {
  if (!mqttClient.connected()) {
    return;
  }
  ActivityLed::beginNetworkActivity();
  mqttClient.disconnect();
  ActivityLed::endNetworkActivity();
}

String normalizeTopic(const String& input, const String& fallback) {
  String out = input;
  out.trim();
  if (out.isEmpty()) {
    out = fallback;
  }
  while (out.endsWith("/")) {
    out.remove(out.length() - 1);
  }
  return out;
}

String buildDeviceId() {
  const uint64_t chipId = ESP.getEfuseMac();
  char buffer[17];
  snprintf(buffer, sizeof(buffer), "%016llX", static_cast<unsigned long long>(chipId));
  return String("esp32-") + buffer;
}

String buildMqttConfigKey(const AppSettings& settings) {
  return settings.mqttHost + "|" + String(settings.mqttPort) + "|" + settings.mqttUser + "|" +
         settings.mqttPassword + "|" + settings.mqttBaseTopic + "|" + settings.mqttDiscoveryPrefix;
}

bool ensureMqttConnected() {
  if (!WifiSettings::isStationConnected()) {
    disconnectMqtt();
    return false;
  }

  const AppSettings& settings = WifiSettings::getAppSettings();
  if (settings.mqttHost.isEmpty()) {
    disconnectMqtt();
    return false;
  }

  const uint32_t now = millis();
  if (mqttClient.connected()) {
    return true;
  }

  if (lastMqttReconnectMs != 0 && now - lastMqttReconnectMs < kMqttReconnectIntervalMs) {
    return false;
  }
  lastMqttReconnectMs = now;

  mqttClient.setServer(settings.mqttHost.c_str(), settings.mqttPort);

  ActivityLed::beginNetworkActivity();
  bool connected = false;
  if (settings.mqttUser.isEmpty()) {
    connected = mqttClient.connect(mqttClientId.c_str());
  } else {
    connected = mqttClient.connect(mqttClientId.c_str(), settings.mqttUser.c_str(), settings.mqttPassword.c_str());
  }
  ActivityLed::endNetworkActivity();
  return connected;
}

void publishDiscovery() {
  const AppSettings& settings = WifiSettings::getAppSettings();
  if (settings.mqttHost.isEmpty()) {
    return;
  }

  const String discoveryPrefix = normalizeTopic(settings.mqttDiscoveryPrefix, "homeassistant");
  const String baseTopic = normalizeTopic(settings.mqttBaseTopic, "esp32/epaper");
  const String deviceId = buildDeviceId();
  const String stateTopic = baseTopic + "/state";

  const String deviceJson = String("{\"identifiers\":[\"") + deviceId + "\"]," +
      "\"name\":\"ESP32 ePaper\"," +
      "\"model\":\"ESP32 DevKit\"," +
      "\"manufacturer\":\"Espressif\"}";

  const String tempConfigTopic = discoveryPrefix + "/sensor/" + deviceId + "/inside_temperature/config";
  const String tempPayload = String("{") +
      "\"name\":\"Inside Temperature\"," +
      "\"unique_id\":\"" + deviceId + "_inside_temperature\"," +
      "\"state_topic\":\"" + stateTopic + "\"," +
      "\"unit_of_measurement\":\"°C\"," +
      "\"device_class\":\"temperature\"," +
      "\"value_template\":\"{{ value_json.temperature }}\"," +
      "\"device\":" + deviceJson + "}";

  const String humConfigTopic = discoveryPrefix + "/sensor/" + deviceId + "/inside_humidity/config";
  const String humPayload = String("{") +
      "\"name\":\"Inside Humidity\"," +
      "\"unique_id\":\"" + deviceId + "_inside_humidity\"," +
      "\"state_topic\":\"" + stateTopic + "\"," +
      "\"unit_of_measurement\":\"%\"," +
      "\"device_class\":\"humidity\"," +
      "\"value_template\":\"{{ value_json.humidity }}\"," +
      "\"device\":" + deviceJson + "}";

  ActivityLed::beginNetworkActivity();
  mqttClient.publish(tempConfigTopic.c_str(), tempPayload.c_str(), true);
  mqttClient.publish(humConfigTopic.c_str(), humPayload.c_str(), true);
  ActivityLed::endNetworkActivity();
}

void publishState() {
  const AppSettings& settings = WifiSettings::getAppSettings();
  if (settings.mqttHost.isEmpty()) {
    return;
  }

  const String baseTopic = normalizeTopic(settings.mqttBaseTopic, "esp32/epaper");
  const String stateTopic = baseTopic + "/state";

  if (isnan(lastTemperatureC) || isnan(lastHumidityPct)) {
    return;
  }

  char payload[128];
  snprintf(payload, sizeof(payload), "{\"temperature\":%.2f,\"humidity\":%.2f}",
           static_cast<double>(lastTemperatureC), static_cast<double>(lastHumidityPct));

  ActivityLed::beginNetworkActivity();
  mqttClient.publish(stateTopic.c_str(), payload, false);
  ActivityLed::endNetworkActivity();
}

void applyInsideFromCacheIfEnabled() {
  const AppSettings& settings = WifiSettings::getAppSettings();
  if (!settings.useInternalBmeForInside) {
    return;
  }

  if (isnan(lastTemperatureC)) {
    MainApp::setInsideTemperature("--");
  } else {
    MainApp::setInsideTemperature(String(lastTemperatureC, 1) + " °C");
  }

  if (isnan(lastHumidityPct)) {
    MainApp::setInsideHumidity("--");
  } else {
    MainApp::setInsideHumidity(String(static_cast<int>(roundf(lastHumidityPct))) + " %");
  }

  if (isnan(lastPressureHpa)) {
    MainApp::setInsidePressure("--");
  } else {
    MainApp::setInsidePressure(String(static_cast<int>(roundf(lastPressureHpa))) + " hPa");
  }
}

void printReadings() {
  const float temperatureC = bme.readTemperature();
  const float humidityPct = bme.readHumidity();
  const float pressureHpa = bme.readPressure() / 100.0f;

  if (isnan(temperatureC) || isnan(humidityPct) || isnan(pressureHpa)) {
    Serial.println("BME280: read failed");
    return;
  }

  lastTemperatureC = temperatureC;
  lastHumidityPct = humidityPct;
  lastPressureHpa = pressureHpa;

  applyInsideFromCacheIfEnabled();

  Serial.print("BME280 T=");
  Serial.print(temperatureC, 1);
  Serial.print(" C, H=");
  Serial.print(humidityPct, 1);
  Serial.print(" %, P=");
  Serial.print(pressureHpa, 1);
  Serial.println(" hPa");
}

}  // namespace

void begin() {
  Wire.begin(AppConfig::kI2cSdaPin, AppConfig::kI2cSclPin);

  mqttClient.setBufferSize(512);
  mqttClientId = buildDeviceId();
  mqttConfigKey = "";
  lastUseInternalBmeForInside = false;

  if (bme.begin(0x76, &Wire) || bme.begin(0x77, &Wire)) {
    sensorReady = true;
    Serial.println("BME280: initialized");
    return;
  }

  sensorReady = false;
  Serial.println("BME280: not found on 0x76/0x77");
}

void loop() {
  if (!sensorReady) {
    return;
  }

  mqttClient.loop();

  const AppSettings& settings = WifiSettings::getAppSettings();
  const String nextConfigKey = buildMqttConfigKey(settings);
  if (nextConfigKey != mqttConfigKey) {
    mqttConfigKey = nextConfigKey;
    lastMqttReconnectMs = 0;
    mqttWasConnected = false;
    disconnectMqtt();
  }

  const bool mqttConnected = ensureMqttConnected();
  if (mqttConnected && !mqttWasConnected) {
    publishDiscovery();
  }
  mqttWasConnected = mqttConnected;

  if (settings.useInternalBmeForInside != lastUseInternalBmeForInside) {
    lastUseInternalBmeForInside = settings.useInternalBmeForInside;
    if (lastUseInternalBmeForInside) {
      applyInsideFromCacheIfEnabled();
    }
  }

  if (lastReadMs != 0 && millis() - lastReadMs < kReadIntervalMs) {
    return;
  }

  lastReadMs = millis();
  printReadings();
  if (mqttConnected) {
    publishState();
  }
}

bool getTemperatureC(float* outValue) {
  if (isnan(lastTemperatureC)) {
    return false;
  }
  *outValue = lastTemperatureC;
  return true;
}

bool getHumidity(float* outValue) {
  if (isnan(lastHumidityPct)) {
    return false;
  }
  *outValue = lastHumidityPct;
  return true;
}

bool getPressureHpa(float* outValue) {
  if (isnan(lastPressureHpa)) {
    return false;
  }
  *outValue = lastPressureHpa;
  return true;
}

bool isMqttConnected() {
  return mqttClient.connected();
}

}  // namespace Bme280Sensor
