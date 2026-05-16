#include "ui_screens.h"

#include <Adafruit_GFX.h>
#include <Fonts/FreeMonoBold9pt7b.h>
#include <GxEPD2_BW.h>

#include "../core/app_config.h"
#include "../assets/splash_image.h"
#include "../assets/setup_logo.h"
#include "../net/wifi_settings.h"

extern GxEPD2_BW<GxEPD2_420_GDEY042T81, GxEPD2_420_GDEY042T81::HEIGHT> display;

namespace UiScreens {

namespace {

bool hasSetupFullRefresh = false;

void drawLine(int16_t x, int16_t y, const String& text) {
  display.setCursor(x, y);
  display.print(text);
}

void beginScreenWithMode(bool usePartial) {
  if (usePartial) {
    display.setPartialWindow(0, 0, display.width(), display.height());
  } else {
    display.setFullWindow();
  }
  display.firstPage();
}

void beginScreen() {
  const bool usePartial = hasSetupFullRefresh || !AppConfig::kSetupFullRefreshOnStart;
  beginScreenWithMode(usePartial);
  if (!usePartial) {
    hasSetupFullRefresh = true;
  }
}

void setupTextStyle() {
  const uint16_t fgColor = GxEPD_WHITE;
  const uint16_t bgColor = GxEPD_BLACK;
  display.fillScreen(bgColor);
  display.setTextColor(fgColor);
  display.setFont(&FreeMonoBold9pt7b);
  display.setTextSize(1);
}

void drawLogo() {
  const uint16_t fgColor = GxEPD_WHITE;
  const int16_t x = static_cast<int16_t>(display.width() - SetupLogo::kWidth - 2);
  const int16_t y = 5;
  display.drawBitmap(x, y, SetupLogo::kBitmap, SetupLogo::kWidth, SetupLogo::kHeight, fgColor);
}

}  // namespace

void renderProvisioning() {
  beginScreen();
  do {
    setupTextStyle();
    drawLogo();

    const String apAddress = String("http://") + WifiSettings::getApIp();

    int16_t y = 14;
    drawLine(10, y, "ESP32 WiFi Setup");
    y += 22;
    drawLine(10, y, "1) Connect to AP");
    y += 18;
    drawLine(10, y, "SSID:");
    y += 18;
    drawLine(20, y, WifiSettings::getApSsid());
    y += 18;
    drawLine(10, y, "2) Open browser");
    y += 18;
    drawLine(10, y, "Address:");
    y += 18;
    drawLine(20, y, apAddress);
    y += 18;
    drawLine(10, y, "Portal user:");
    y += 18;
    drawLine(20, y, AppConfig::kPortalAuthUser);
    y += 18;
    drawLine(10, y, "Portal pass:");
    y += 18;
    drawLine(20, y, WifiSettings::getApPassword());
    y += 18;
    drawLine(10, y, "Saved SSID:");
    y += 18;
    if (WifiSettings::hasStoredCredentials()) {
      drawLine(20, y, WifiSettings::getStoredSsid());
    } else {
      drawLine(20, y, "<none>");
    }
    y += 20;
    drawLine(10, y, WifiSettings::getStatusMessage());
  } while (display.nextPage());
}

void renderConnecting() {
  beginScreen();
  do {
    setupTextStyle();
    drawLogo();

    int16_t y = 80;
    drawLine(10, y, "Connecting to WiFi");
    y += 28;
    drawLine(10, y, WifiSettings::getStatusMessage());
  } while (display.nextPage());
}

void renderConnected() {
  beginScreen();
  do {
    setupTextStyle();
    drawLogo();

    const String address = String("http://") + WifiSettings::getStationIp();

    int16_t y = 34;
    drawLine(10, y, "WiFi Connected");
    y += 28;
    drawLine(10, y, "SSID:");
    y += 20;
    drawLine(20, y, WifiSettings::getConnectedSsid());
    y += 24;
    drawLine(10, y, "Address:");
    y += 20;
    drawLine(20, y, address);
    y += 24;
    drawLine(10, y, "Portal user:");
    y += 20;
    drawLine(20, y, AppConfig::kPortalAuthUser);
    y += 24;
    drawLine(10, y, "Portal pass:");
    y += 20;
    drawLine(20, y, WifiSettings::getApPassword());
  } while (display.nextPage());
}

void renderSplash() {
  beginScreenWithMode(true);
  do {
    const uint16_t fgColor = GxEPD_BLACK;
    const uint16_t bgColor = GxEPD_WHITE;
    display.fillScreen(bgColor);
    display.drawBitmap(5, 5, SplashImage::kBitmap, SplashImage::kWidth, SplashImage::kHeight, fgColor);
  } while (display.nextPage());
}

}  // namespace UiScreens
