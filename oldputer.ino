#define ENABLE_GxEPD2_GFX 0

#include <GxEPD2_BW.h>

#include "src/core/activity_led.h"
#include "src/sensors/bme280_sensor.h"
#include "src/core/app_config.h"
#include "src/net/ha_fetch.h"
#include "src/ui/main_app.h"
#include "src/ui/ui_screens.h"
#include "src/net/wifi_settings.h"

constexpr uint32_t kConnectedScreenDurationMs = 10000;

GxEPD2_BW<GxEPD2_420_GDEY042T81, GxEPD2_420_GDEY042T81::HEIGHT> display(
    GxEPD2_420_GDEY042T81(
        AppConfig::kEpaperCsPin,
        AppConfig::kEpaperDcPin,
        AppConfig::kEpaperResetPin,
        AppConfig::kEpaperBusyPin));

enum class Screen {
  CONNECTING,
  PROVISIONING,
  CONNECTED_INFO,
  MAIN_APP,
};

Screen currentScreen = Screen::CONNECTING;
uint32_t connectedScreenStartMs = 0;

void renderCurrentScreen() {
  if (currentScreen == Screen::CONNECTING) {
    UiScreens::renderConnecting();
    return;
  }

  if (currentScreen == Screen::PROVISIONING) {
    UiScreens::renderProvisioning();
    return;
  }

  if (currentScreen == Screen::CONNECTED_INFO) {
    UiScreens::renderConnected();
    return;
  }

  MainApp::render();
}

void setScreen(Screen nextScreen) {
  if (currentScreen == nextScreen) {
    return;
  }

  currentScreen = nextScreen;

  if (currentScreen == Screen::CONNECTED_INFO) {
    connectedScreenStartMs = millis();
  }

  if (currentScreen == Screen::MAIN_APP) {
    UiScreens::renderSplash();
    delay(AppConfig::kMainEnterImageDurationMs);
    MainApp::begin();
  }

  renderCurrentScreen();
}

void refreshStatusScreenIfNeeded() {
  if (!WifiSettings::consumeDisplayUpdateFlag()) {
    return;
  }

  if (currentScreen == Screen::CONNECTING || currentScreen == Screen::PROVISIONING) {
    renderCurrentScreen();
  }
}

void updateScreenState() {
  if (WifiSettings::isStationConnected()) {
    if (currentScreen == Screen::MAIN_APP) {
      return;
    }

    if (currentScreen != Screen::CONNECTED_INFO) {
      setScreen(Screen::CONNECTED_INFO);
      return;
    }

    if (millis() - connectedScreenStartMs >= kConnectedScreenDurationMs) {
      setScreen(Screen::MAIN_APP);
    }
    return;
  }

  if (WifiSettings::getState() == WifiSettings::State::PORTAL) {
    setScreen(Screen::PROVISIONING);
  } else {
    setScreen(Screen::CONNECTING);
  }

  refreshStatusScreenIfNeeded();
}

void setup() {
  Serial.begin(115200);
  ActivityLed::begin(AppConfig::kActivityLedPin);
  display.init(115200, true, 50, false);
  display.setRotation(0);

  Bme280Sensor::begin();
  WifiSettings::begin();
  HaFetch::begin();
  renderCurrentScreen();
}

void loop() {
  ActivityLed::loop();
  WifiSettings::loop();
  HaFetch::loop();
  Bme280Sensor::loop();
  updateScreenState();

  if (currentScreen == Screen::MAIN_APP) {
    MainApp::loop();
  }
}
