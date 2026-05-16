#ifndef APP_SETTINGS_H
#define APP_SETTINGS_H

#include <Arduino.h>

struct AppSettings {
  String wifiSsid;
  String wifiPassword;
  String homeAssistantAddress;
  String homeAssistantToken;
  String outsideTemperatureEntityId;
  String outsideHumidityEntityId;
  String outsideAqiEntityId;
  String outsideWeatherEntityId;
  String insideTemperatureEntityId;
  String insideHumidityEntityId;
  String insidePressureEntityId;
  String insidePm25EntityId;
  String todayMaxTemperatureEntityId;
  String todayMinTemperatureEntityId;
  String todayWeatherEntityId;
  String tomorrowMaxTemperatureEntityId;
  String tomorrowMinTemperatureEntityId;
  String tomorrowWeatherEntityId;
  bool invertDisplay = false;
  String mqttHost;
  uint16_t mqttPort = 1883;
  String mqttUser;
  String mqttPassword;
  String mqttBaseTopic;
  String mqttDiscoveryPrefix;
  bool useInternalBmeForInside = false;
};

namespace AppSettingsStore {

AppSettings load();
void save(const AppSettings& settings);

}  // namespace AppSettingsStore

#endif
