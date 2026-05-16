#ifndef ACTIVITY_LED_H
#define ACTIVITY_LED_H

#include <Arduino.h>

namespace ActivityLed {

void begin(uint8_t pin);
void loop();
void setWifiConnected(bool connected);
void beginNetworkActivity();
void endNetworkActivity();

class ScopedNetworkActivity {
 public:
  ScopedNetworkActivity();
  ~ScopedNetworkActivity();

 private:
  bool active;
};

}  // namespace ActivityLed

#endif
