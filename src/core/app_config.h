#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include <Arduino.h>

namespace AppConfig {

constexpr uint8_t kEpaperCsPin = 5; // ePaper SPI CS pin.
constexpr uint8_t kEpaperBusyPin = 4; // ePaper BUSY pin (panel busy signal).
constexpr uint8_t kEpaperResetPin = 17; // ePaper reset pin.
constexpr uint8_t kEpaperDcPin = 16; // ePaper D/C pin.

constexpr uint8_t kI2cSdaPin = 21; // I2C SDA pin (BME280).
constexpr uint8_t kI2cSclPin = 22; // I2C SCL pin (BME280).

constexpr uint8_t kActivityLedPin = 25; // Activity LED pin (active-low).

constexpr bool kSetupFullRefreshOnStart = true; // Full refresh once before setup screens use partial.
constexpr uint8_t kMainFullRefreshEveryN = 5; // Full refresh every N main renders (0 = always partial).
constexpr uint32_t kHaFetchIntervalMs = 600000; // Home Assistant fetch interval in ms.
constexpr uint32_t kMainEnterImageDurationMs = 3000; // Splash image duration before main screen.
constexpr const char* kPortalAuthUser = "admin"; // Basic auth username for config portal.

constexpr uint16_t kLedOnMinMs = 20; // Activity blink ON duration minimum.
constexpr uint16_t kLedOnMaxMs = 85; // Activity blink ON duration maximum.
constexpr uint16_t kLedOffMinMs = 15; // Activity blink OFF duration minimum.
constexpr uint16_t kLedOffMaxMs = 140; // Activity blink OFF duration maximum.
constexpr uint8_t kLedPulsesPerActivity = 4; // Pulse budget added per activity event.
constexpr uint8_t kLedMaxPulseBudget = 32; // Maximum queued pulse budget.
constexpr uint16_t kLedActivityTailMs = 320; // Keep blinking this long after activity ends.


}  // namespace AppConfig

#endif
