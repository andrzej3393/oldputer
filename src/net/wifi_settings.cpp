#include "wifi_settings.h"

#include <WiFi.h>

#include "../core/activity_led.h"
#include "../core/app_settings.h"
#include "config_portal.h"

namespace WifiSettings {

namespace {

constexpr uint32_t kConnectTimeoutMs = 20000;

State state = State::CONNECTING;

AppSettings appSettings;
String apSsid;
String apPassword;
String statusMessage = "Waiting for configuration";

bool stationConnected = false;
bool stationConnecting = false;
bool apRunning = false;
bool displayUpdatePending = true;
bool connectRequested = false;
uint32_t connectStartMs = 0;

void markDisplayDirty() {
  displayUpdatePending = true;
}

void setState(State nextState) {
  if (state != nextState) {
    state = nextState;
    markDisplayDirty();
  }
}

bool hasWifiCredentials(const AppSettings& settings) {
  return !settings.wifiSsid.isEmpty();
}

void setApEnabled(bool enabled) {
  if (enabled) {
    if (apRunning) {
      return;
    }
    WiFi.softAP(apSsid.c_str(), apPassword.c_str());
    apRunning = true;
    return;
  }

  if (!apRunning) {
    return;
  }
  WiFi.softAPdisconnect(true);
  apRunning = false;
}

void connectToConfiguredWifi() {
  if (!hasWifiCredentials(appSettings)) {
    stationConnecting = false;
    stationConnected = false;
    ActivityLed::setWifiConnected(false);
    statusMessage = "No WiFi configured yet";
    setApEnabled(true);
    setState(State::PORTAL);
    return;
  }

  setApEnabled(true);
  WiFi.disconnect(false, true);
  WiFi.begin(appSettings.wifiSsid.c_str(), appSettings.wifiPassword.c_str());
  connectStartMs = millis();
  stationConnecting = true;
  stationConnected = false;
  ActivityLed::setWifiConnected(false);
  statusMessage = "Connecting to WiFi...";
  setState(State::CONNECTING);
}

void onSettingsSubmitted(const AppSettings& settings) {
  const bool wifiChanged = appSettings.wifiSsid != settings.wifiSsid ||
                           appSettings.wifiPassword != settings.wifiPassword;

  appSettings = settings;
  AppSettingsStore::save(appSettings);

  statusMessage = wifiChanged ? "Configuration saved, reconnecting WiFi..." : "Configuration saved";
  markDisplayDirty();
  connectRequested = wifiChanged;
}

void handleConnected() {
  if (stationConnected) {
    return;
  }

  stationConnected = true;
  stationConnecting = false;
  ActivityLed::setWifiConnected(true);
  statusMessage = "Connected to " + appSettings.wifiSsid;
  setApEnabled(false);
  setState(State::CONNECTED);
}

void handleConnectTimeout() {
  if (!stationConnecting) {
    return;
  }
  if (millis() - connectStartMs <= kConnectTimeoutMs) {
    return;
  }

  stationConnecting = false;
  stationConnected = false;
  ActivityLed::setWifiConnected(false);
  statusMessage = "Failed to connect to " + appSettings.wifiSsid;
  setApEnabled(true);
  setState(State::PORTAL);
}

void handleDisconnected() {
  if (stationConnected) {
    stationConnected = false;
    ActivityLed::setWifiConnected(false);
    statusMessage = "WiFi disconnected";
    connectToConfiguredWifi();
    return;
  }

  if (!hasWifiCredentials(appSettings)) {
    statusMessage = "No WiFi configured yet";
    setApEnabled(true);
    setState(State::PORTAL);
    return;
  }

  handleConnectTimeout();
}

void updateStationState() {
  if (connectRequested) {
    connectRequested = false;
    connectToConfiguredWifi();
  }

  if (WiFi.status() == WL_CONNECTED) {
    handleConnected();
    return;
  }

  handleDisconnected();
}

void createApCredentials() {
  const uint64_t chipId = ESP.getEfuseMac();
  char apSuffix[7];
  snprintf(apSuffix, sizeof(apSuffix), "%06X", static_cast<uint32_t>(chipId & 0xFFFFFF));
  apSsid = String("ESP32-Setup-") + apSuffix;

  char passSuffix[5];
  snprintf(passSuffix, sizeof(passSuffix), "%04X", static_cast<uint16_t>(chipId & 0xFFFF));
  apPassword = String("setup") + passSuffix;
}

}  // namespace

void begin() {
  ActivityLed::setWifiConnected(false);
  createApCredentials();
  WiFi.setSleep(false);
  WiFi.mode(WIFI_AP_STA);

  appSettings = AppSettingsStore::load();

  ConfigPortal::Context portalContext = {
      &appSettings,
      &statusMessage,
      &apSsid,
      onSettingsSubmitted,
  };
  ConfigPortal::begin(portalContext);

  connectRequested = true;
}

void loop() {
  ConfigPortal::loop();
  updateStationState();
}

State getState() {
  return state;
}

String getApSsid() {
  return apSsid;
}

String getApPassword() {
  return apPassword;
}

String getApIp() {
  if (!apRunning) {
    return String();
  }
  return WiFi.softAPIP().toString();
}

bool hasStoredCredentials() {
  return hasWifiCredentials(appSettings);
}

String getStoredSsid() {
  return appSettings.wifiSsid;
}

bool isStationConnected() {
  return stationConnected;
}

String getConnectedSsid() {
  if (!stationConnected) {
    return String();
  }
  return WiFi.SSID();
}

String getStationIp() {
  if (!stationConnected) {
    return String();
  }
  return WiFi.localIP().toString();
}

String getStatusMessage() {
  return statusMessage;
}

const AppSettings& getAppSettings() {
  return appSettings;
}

bool consumeDisplayUpdateFlag() {
  const bool dirty = displayUpdatePending;
  displayUpdatePending = false;
  return dirty;
}

}  // namespace WifiSettings
