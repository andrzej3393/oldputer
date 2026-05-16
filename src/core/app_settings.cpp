#include "app_settings.h"

#include <Preferences.h>

namespace {

constexpr const char* kPrefsNamespace = "app_cfg";
constexpr const char* kWifiSsidKey = "wifi_ssid";
constexpr const char* kWifiPassKey = "wifi_pass";
constexpr const char* kHaAddrKey = "ha_addr";
constexpr const char* kHaTokenKey = "ha_token";
constexpr const char* kOutsideTempEntityKey = "out_temp_ent";
constexpr const char* kOutsideHumEntityKey = "out_hum_ent";
constexpr const char* kOutsideAqiEntityKey = "out_aqi_ent";
constexpr const char* kOutsideWeatherEntityKey = "out_wx_ent";
constexpr const char* kInsideTempEntityKey = "in_temp_ent";
constexpr const char* kInsideHumEntityKey = "in_hum_ent";
constexpr const char* kInsidePressureEntityKey = "in_press_ent";
constexpr const char* kInsidePm25EntityKey = "in_pm25_ent";
constexpr const char* kTodayMaxTempEntityKey = "fc_max_temp_ent";
constexpr const char* kTodayMinTempEntityKey = "fc_min_temp_ent";
constexpr const char* kTodayWeatherEntityKey = "fc_wx_ent";
constexpr const char* kTomorrowMaxTempEntityKey = "om_max_temp_ent";
constexpr const char* kTomorrowMinTempEntityKey = "om_min_temp_ent";
constexpr const char* kTomorrowWeatherEntityKey = "om_wx_ent";
constexpr const char* kInvertDisplayKey = "invert_display";
constexpr const char* kMqttHostKey = "mqtt_host";
constexpr const char* kMqttPortKey = "mqtt_port";
constexpr const char* kMqttUserKey = "mqtt_user";
constexpr const char* kMqttPasswordKey = "mqtt_pass";
constexpr const char* kMqttBaseTopicKey = "mqtt_base";
constexpr const char* kMqttDiscoveryPrefixKey = "mqtt_disc";
constexpr const char* kUseInternalBmeKey = "use_bme_int";

}  // namespace

namespace AppSettingsStore {

AppSettings load() {
  Preferences prefs;
  prefs.begin(kPrefsNamespace, true);

  AppSettings settings;
  settings.wifiSsid = prefs.getString(kWifiSsidKey, "");
  settings.wifiPassword = prefs.getString(kWifiPassKey, "");
  settings.homeAssistantAddress = prefs.getString(kHaAddrKey, "");
  settings.homeAssistantToken = prefs.getString(kHaTokenKey, "");
  settings.outsideTemperatureEntityId = prefs.getString(kOutsideTempEntityKey, "");
  settings.outsideHumidityEntityId = prefs.getString(kOutsideHumEntityKey, "");
  settings.outsideAqiEntityId = prefs.getString(kOutsideAqiEntityKey, "");
  settings.outsideWeatherEntityId = prefs.getString(kOutsideWeatherEntityKey, "");
  settings.insideTemperatureEntityId = prefs.getString(kInsideTempEntityKey, "");
  settings.insideHumidityEntityId = prefs.getString(kInsideHumEntityKey, "");
  settings.insidePressureEntityId = prefs.getString(kInsidePressureEntityKey, "");
  settings.insidePm25EntityId = prefs.getString(kInsidePm25EntityKey, "");
  settings.todayMaxTemperatureEntityId = prefs.getString(kTodayMaxTempEntityKey, "");
  settings.todayMinTemperatureEntityId = prefs.getString(kTodayMinTempEntityKey, "");
  settings.todayWeatherEntityId = prefs.getString(kTodayWeatherEntityKey, "");
  settings.tomorrowMaxTemperatureEntityId = prefs.getString(kTomorrowMaxTempEntityKey, "");
  settings.tomorrowMinTemperatureEntityId = prefs.getString(kTomorrowMinTempEntityKey, "");
  settings.tomorrowWeatherEntityId = prefs.getString(kTomorrowWeatherEntityKey, "");
  settings.invertDisplay = prefs.getBool(kInvertDisplayKey, false);
  settings.mqttHost = prefs.getString(kMqttHostKey, "");
  settings.mqttPort = static_cast<uint16_t>(prefs.getUInt(kMqttPortKey, 1883));
  settings.mqttUser = prefs.getString(kMqttUserKey, "");
  settings.mqttPassword = prefs.getString(kMqttPasswordKey, "");
  settings.mqttBaseTopic = prefs.getString(kMqttBaseTopicKey, "");
  settings.mqttDiscoveryPrefix = prefs.getString(kMqttDiscoveryPrefixKey, "");
  settings.useInternalBmeForInside = prefs.getBool(kUseInternalBmeKey, false);

  prefs.end();
  return settings;
}

void save(const AppSettings& settings) {
  Preferences prefs;
  prefs.begin(kPrefsNamespace, false);

  prefs.putString(kWifiSsidKey, settings.wifiSsid);
  prefs.putString(kWifiPassKey, settings.wifiPassword);
  prefs.putString(kHaAddrKey, settings.homeAssistantAddress);
  prefs.putString(kHaTokenKey, settings.homeAssistantToken);
  prefs.putString(kOutsideTempEntityKey, settings.outsideTemperatureEntityId);
  prefs.putString(kOutsideHumEntityKey, settings.outsideHumidityEntityId);
  prefs.putString(kOutsideAqiEntityKey, settings.outsideAqiEntityId);
  prefs.putString(kOutsideWeatherEntityKey, settings.outsideWeatherEntityId);
  prefs.putString(kInsideTempEntityKey, settings.insideTemperatureEntityId);
  prefs.putString(kInsideHumEntityKey, settings.insideHumidityEntityId);
  prefs.putString(kInsidePressureEntityKey, settings.insidePressureEntityId);
  prefs.putString(kInsidePm25EntityKey, settings.insidePm25EntityId);
  prefs.putString(kTodayMaxTempEntityKey, settings.todayMaxTemperatureEntityId);
  prefs.putString(kTodayMinTempEntityKey, settings.todayMinTemperatureEntityId);
  prefs.putString(kTodayWeatherEntityKey, settings.todayWeatherEntityId);
  prefs.putString(kTomorrowMaxTempEntityKey, settings.tomorrowMaxTemperatureEntityId);
  prefs.putString(kTomorrowMinTempEntityKey, settings.tomorrowMinTemperatureEntityId);
  prefs.putString(kTomorrowWeatherEntityKey, settings.tomorrowWeatherEntityId);
  prefs.putBool(kInvertDisplayKey, settings.invertDisplay);
  prefs.putString(kMqttHostKey, settings.mqttHost);
  prefs.putUInt(kMqttPortKey, settings.mqttPort);
  prefs.putString(kMqttUserKey, settings.mqttUser);
  prefs.putString(kMqttPasswordKey, settings.mqttPassword);
  prefs.putString(kMqttBaseTopicKey, settings.mqttBaseTopic);
  prefs.putString(kMqttDiscoveryPrefixKey, settings.mqttDiscoveryPrefix);
  prefs.putBool(kUseInternalBmeKey, settings.useInternalBmeForInside);

  prefs.end();
}

}  // namespace AppSettingsStore
