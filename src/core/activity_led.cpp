#include "activity_led.h"

#include "app_config.h"

namespace ActivityLed {

namespace {

bool initialized = false;
uint8_t ledPin = 255;
bool wifiConnected = false;
uint16_t networkActivityDepth = 0;
uint8_t pulseBudget = 0;
uint32_t activityTailUntilMs = 0;
uint32_t phaseEndsAtMs = 0;
bool phaseOn = false;

bool timeReached(uint32_t now, uint32_t target) {
  return static_cast<int32_t>(now - target) >= 0;
}

uint16_t randomRangeMs(uint16_t minMs, uint16_t maxMs) {
  if (maxMs <= minMs) {
    return minMs;
  }
  return static_cast<uint16_t>(random(minMs, static_cast<long>(maxMs) + 1));
}

void writeLed(bool on) {
  if (!initialized) {
    return;
  }
  digitalWrite(ledPin, on ? LOW : HIGH);
}

bool shouldBlink(uint32_t now) {
  if (!wifiConnected) {
    return false;
  }
  return networkActivityDepth > 0 || pulseBudget > 0 || static_cast<int32_t>(activityTailUntilMs - now) > 0;
}

void startOnPhase(uint32_t now) {
  phaseOn = true;
  phaseEndsAtMs = now + randomRangeMs(AppConfig::kLedOnMinMs, AppConfig::kLedOnMaxMs);
  writeLed(true);
}

void startOffPhase(uint32_t now) {
  phaseOn = false;
  phaseEndsAtMs = now + randomRangeMs(AppConfig::kLedOffMinMs, AppConfig::kLedOffMaxMs);
  writeLed(false);
}

void clearBlinkState() {
  pulseBudget = 0;
  activityTailUntilMs = 0;
  phaseEndsAtMs = 0;
  phaseOn = false;
}

void updateOutput() {
  if (!initialized) {
    return;
  }

  const uint32_t now = millis();
  if (!wifiConnected) {
    writeLed(true);
    return;
  }

  if (shouldBlink(now)) {
    if (phaseEndsAtMs == 0) {
      startOnPhase(now);
    } else {
      writeLed(phaseOn);
    }
    return;
  }

  phaseOn = false;
  phaseEndsAtMs = 0;
  writeLed(false);
}

}  // namespace

void begin(uint8_t pin) {
  ledPin = pin;
  wifiConnected = false;
  networkActivityDepth = 0;
  clearBlinkState();
  pinMode(ledPin, OUTPUT);
  initialized = true;
  updateOutput();
}

void setWifiConnected(bool connected) {
  wifiConnected = connected;
  if (!wifiConnected) {
    clearBlinkState();
  }
  updateOutput();
}

void beginNetworkActivity() {
  if (networkActivityDepth < 65535) {
    networkActivityDepth++;
  }
  const uint16_t nextBudget = static_cast<uint16_t>(pulseBudget) + AppConfig::kLedPulsesPerActivity;
  pulseBudget = static_cast<uint8_t>(nextBudget > AppConfig::kLedMaxPulseBudget ? AppConfig::kLedMaxPulseBudget : nextBudget);
  const uint32_t now = millis();
  activityTailUntilMs = now + AppConfig::kLedActivityTailMs;
  if (wifiConnected) {
    startOnPhase(now);
  }
  updateOutput();
}

void endNetworkActivity() {
  if (networkActivityDepth > 0) {
    networkActivityDepth--;
  }
  activityTailUntilMs = millis() + AppConfig::kLedActivityTailMs;
  updateOutput();
}

void loop() {
  if (!initialized) {
    return;
  }

  const uint32_t now = millis();
  if (!wifiConnected) {
    writeLed(true);
    return;
  }

  if (!shouldBlink(now)) {
    phaseOn = false;
    phaseEndsAtMs = 0;
    writeLed(false);
    return;
  }

  if (phaseEndsAtMs == 0) {
    startOnPhase(now);
    return;
  }

  if (!timeReached(now, phaseEndsAtMs)) {
    return;
  }

  if (phaseOn) {
    if (pulseBudget > 0) {
      pulseBudget--;
    }
    startOffPhase(now);
    return;
  }

  if (shouldBlink(now)) {
    startOnPhase(now);
  } else {
    phaseOn = false;
    phaseEndsAtMs = 0;
    writeLed(false);
  }
}

ScopedNetworkActivity::ScopedNetworkActivity() : active(true) {
  beginNetworkActivity();
}

ScopedNetworkActivity::~ScopedNetworkActivity() {
  if (active) {
    endNetworkActivity();
  }
}

}  // namespace ActivityLed
