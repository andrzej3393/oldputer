#ifndef WIFI_SETTINGS_H
#define WIFI_SETTINGS_H

#include <Arduino.h>

#include "../core/app_settings.h"

namespace WifiSettings {

enum class State {
  CONNECTING,
  PORTAL,
  CONNECTED,
};

void begin();
void loop();

State getState();

String getApSsid();
String getApPassword();
String getApIp();

bool hasStoredCredentials();
String getStoredSsid();

bool isStationConnected();
String getConnectedSsid();
String getStationIp();

String getStatusMessage();
const AppSettings& getAppSettings();

bool consumeDisplayUpdateFlag();

}  // namespace WifiSettings


#endif
