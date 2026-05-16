#ifndef CONFIG_PORTAL_H
#define CONFIG_PORTAL_H

#include <Arduino.h>

#include "../core/app_settings.h"

namespace ConfigPortal {

struct Context {
  AppSettings* settings;
  String* statusMessage;
  String* apSsid;
  void (*onSettingsSubmitted)(const AppSettings& settings);
};

void begin(const Context& context);
void loop();

}  // namespace ConfigPortal

#endif
