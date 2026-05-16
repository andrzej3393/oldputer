#include "main_app.h"

#include <Adafruit_GFX.h>
#include <Fonts/FreeMonoBold12pt7b.h>
#include <Fonts/FreeMonoBold18pt7b.h>
#include <Fonts/FreeMonoBold24pt7b.h>
#include <Fonts/FreeMonoBold9pt7b.h>
#include <GxEPD2_BW.h>
#include <string.h>
#include <time.h>

#include "../core/activity_led.h"
#include "../core/app_config.h"
#include "../net/wifi_settings.h"
#include "../assets/weather_icons_custom.h"
#include "weather_icon_mapper.h"

extern GxEPD2_BW<GxEPD2_420_GDEY042T81, GxEPD2_420_GDEY042T81::HEIGHT> display;

namespace MainApp {

namespace {

String timeText = "--:--";
String dateText = "---- -- --";

String outTempText = "--";
String outHumText = "--";
String outAqiText = "--";
String outWeatherText = "unknown";

String inTempText = "--";
String inHumText = "--";
String inPressureText = "--";
String inPm25Text = "--";

String todayMaxText = "--";
String todayMinText = "--";
String todayWeatherText = "unknown";
String tomorrowMaxText = "--";
String tomorrowMinText = "--";
String tomorrowWeatherText = "unknown";

bool outsideIsNight = false;
String sunStateText = "";
String nextSunLabelText = "SUNSET";
String nextSunTimeText = "--:--";
tm nextRisingClock = {};
tm nextSettingClock = {};
bool nextRisingValid = false;
bool nextSettingValid = false;

bool invertDisplay = false;
uint16_t fgColor = GxEPD_BLACK;
uint16_t bgColor = GxEPD_WHITE;
uint32_t renderCount = 0;

enum class RenderRequest : uint8_t {
  NONE = 0,
  CLOCK_ONLY = 1,
  FULL = 2,
};

constexpr uint32_t kMinuteMs = 60000;
constexpr int16_t kDividerXShift = -30;
constexpr int16_t kDividerYShift = -20;
constexpr int16_t kOutIconSize = 100;
constexpr int16_t kForecastIconSize = 62;
constexpr int16_t kOutIconRightMargin = 0;
constexpr int16_t kForecastIconRightMargin = 15;
constexpr int16_t kContentYOffset = 2;
constexpr int16_t kOutSectionTopPadding = 5;
constexpr int16_t kSectionValueSpacing = 26;
constexpr int16_t kClockPartialDividerMargin = 8;
constexpr uint8_t kIconCacheSlots = 4;
constexpr size_t kIconKeyBufferSize = 24;
constexpr uint16_t kOutIconRowBytes = (static_cast<uint16_t>(kOutIconSize) + 7) / 8;
constexpr uint16_t kOutIconBytes = kOutIconRowBytes * static_cast<uint16_t>(kOutIconSize);
constexpr uint16_t kForecastIconRowBytes = (static_cast<uint16_t>(kForecastIconSize) + 7) / 8;
constexpr uint16_t kForecastIconBytes = kForecastIconRowBytes * static_cast<uint16_t>(kForecastIconSize);

template <size_t BitmapBytes>
struct IconCacheEntry {
  bool ready = false;
  char key[kIconKeyBufferSize] = {0};
  uint32_t stamp = 0;
  uint8_t bitmap[BitmapBytes] = {0};
};

IconCacheEntry<kOutIconBytes> outIconCache[kIconCacheSlots];
IconCacheEntry<kForecastIconBytes> forecastIconCache[kIconCacheSlots];
uint32_t iconCacheStamp = 1;

bool localClockValid = false;
uint32_t lastClockTickMs = 0;
tm localClock;
RenderRequest pendingRender = RenderRequest::FULL;

void requestRender(RenderRequest request) {
  if (static_cast<uint8_t>(request) > static_cast<uint8_t>(pendingRender)) {
    pendingRender = request;
  }
}

void syncThemeFromSettings() {
  const bool nextInvert = WifiSettings::getAppSettings().invertDisplay;
  if (nextInvert != invertDisplay) {
    invertDisplay = nextInvert;
    requestRender(RenderRequest::FULL);
  }
  fgColor = invertDisplay ? GxEPD_WHITE : GxEPD_BLACK;
  bgColor = invertDisplay ? GxEPD_BLACK : GxEPD_WHITE;
}

void drawTextAt(int16_t x, int16_t y, const String& text) {
  display.setCursor(x, y);
  display.print(text);
}

void drawCentered(int16_t centerX, int16_t baselineY, const String& text) {
  int16_t tbx;
  int16_t tby;
  uint16_t tbw;
  uint16_t tbh;
  display.getTextBounds(text, 0, 0, &tbx, &tby, &tbw, &tbh);
  const int16_t x = centerX - static_cast<int16_t>(tbw / 2) - tbx;
  drawTextAt(x, baselineY, text);
}

bool bitmapPixelIsSet(const uint8_t* bitmap, uint16_t width, uint16_t px, uint16_t py) {
  const uint16_t rowBytes = (width + 7) / 8;
  const uint8_t byteVal = pgm_read_byte(bitmap + py * rowBytes + (px / 8));
  return (byteVal & (0x80 >> (px & 0x7))) != 0;
}

void bitmapSetPixel(uint8_t* bitmap, uint16_t width, uint16_t px, uint16_t py) {
  const uint16_t rowBytes = (width + 7) / 8;
  const uint16_t byteIndex = py * rowBytes + (px / 8);
  bitmap[byteIndex] |= static_cast<uint8_t>(0x80 >> (px & 0x7));
}

void rasterizeIconNearest(
    const uint8_t* sourceBitmap,
    uint16_t sourceWidth,
    uint16_t sourceHeight,
    uint8_t* targetBitmap,
    uint16_t targetWidth,
    uint16_t targetHeight) {
  const uint16_t targetRowBytes = (targetWidth + 7) / 8;
  memset(targetBitmap, 0, static_cast<size_t>(targetRowBytes) * targetHeight);

  if (targetWidth == 0 || targetHeight == 0) {
    return;
  }

  for (uint16_t y = 0; y < targetHeight; ++y) {
    uint16_t sourceY = static_cast<uint16_t>((static_cast<uint32_t>(y) * sourceHeight + targetHeight / 2) / targetHeight);
    if (sourceY >= sourceHeight) {
      sourceY = sourceHeight - 1;
    }

    for (uint16_t x = 0; x < targetWidth; ++x) {
      uint16_t sourceX = static_cast<uint16_t>((static_cast<uint32_t>(x) * sourceWidth + targetWidth / 2) / targetWidth);
      if (sourceX >= sourceWidth) {
        sourceX = sourceWidth - 1;
      }

      if (bitmapPixelIsSet(sourceBitmap, sourceWidth, sourceX, sourceY)) {
        bitmapSetPixel(targetBitmap, targetWidth, x, y);
      }
    }
  }
}

template <size_t BitmapBytes>
int findCacheEntry(IconCacheEntry<BitmapBytes>* cache, const String& iconKey) {
  for (uint8_t i = 0; i < kIconCacheSlots; ++i) {
    if (cache[i].ready && iconKey == cache[i].key) {
      return i;
    }
  }
  return -1;
}

template <size_t BitmapBytes>
int selectCacheSlot(IconCacheEntry<BitmapBytes>* cache) {
  for (uint8_t i = 0; i < kIconCacheSlots; ++i) {
    if (!cache[i].ready) {
      return i;
    }
  }

  uint8_t oldestIndex = 0;
  uint32_t oldestStamp = cache[0].stamp;
  for (uint8_t i = 1; i < kIconCacheSlots; ++i) {
    if (cache[i].stamp < oldestStamp) {
      oldestStamp = cache[i].stamp;
      oldestIndex = i;
    }
  }
  return oldestIndex;
}

template <size_t BitmapBytes>
const uint8_t* getCachedIconBitmap(IconCacheEntry<BitmapBytes>* cache, uint16_t targetSize, const String& iconKey) {
  int index = findCacheEntry(cache, iconKey);
  if (index < 0) {
    index = selectCacheSlot(cache);
    const uint8_t* sourceBitmap = WeatherIconsCustom::byName(iconKey);
    rasterizeIconNearest(
        sourceBitmap,
        WeatherIconsCustom::kIconWidth,
        WeatherIconsCustom::kIconHeight,
        cache[index].bitmap,
        targetSize,
        targetSize);
    strncpy(cache[index].key, iconKey.c_str(), sizeof(cache[index].key) - 1);
    cache[index].key[sizeof(cache[index].key) - 1] = '\0';
    cache[index].ready = true;
  }

  cache[index].stamp = iconCacheStamp++;
  return cache[index].bitmap;
}

void drawBitmapNearestScaled(
    int16_t x,
    int16_t y,
    const uint8_t* bitmap,
    uint16_t srcWidth,
    uint16_t srcHeight,
    int16_t dstWidth,
    int16_t dstHeight) {
  if (dstWidth <= 0 || dstHeight <= 0) {
    return;
  }

  for (int16_t dy = 0; dy < dstHeight; ++dy) {
    uint16_t sy = static_cast<uint16_t>((static_cast<uint32_t>(dy) * srcHeight + dstHeight / 2) / dstHeight);
    if (sy >= srcHeight) {
      sy = srcHeight - 1;
    }

    for (int16_t dx = 0; dx < dstWidth; ++dx) {
      uint16_t sx = static_cast<uint16_t>((static_cast<uint32_t>(dx) * srcWidth + dstWidth / 2) / dstWidth);
      if (sx >= srcWidth) {
        sx = srcWidth - 1;
      }

      if (bitmapPixelIsSet(bitmap, srcWidth, sx, sy)) {
        display.drawPixel(x + dx, y + dy, fgColor);
      }
    }
  }
}

void drawWeatherIconByKey(int16_t x, int16_t y, const String& iconKey, int16_t size) {
  if (size == kOutIconSize) {
    const uint8_t* bitmap = getCachedIconBitmap(outIconCache, static_cast<uint16_t>(kOutIconSize), iconKey);
    display.drawBitmap(x, y, bitmap, kOutIconSize, kOutIconSize, fgColor);
    return;
  }

  if (size == kForecastIconSize) {
    const uint8_t* bitmap = getCachedIconBitmap(forecastIconCache, static_cast<uint16_t>(kForecastIconSize), iconKey);
    display.drawBitmap(x, y, bitmap, kForecastIconSize, kForecastIconSize, fgColor);
    return;
  }

  const uint8_t* bitmap = WeatherIconsCustom::byName(iconKey);
  drawBitmapNearestScaled(x, y, bitmap, WeatherIconsCustom::kIconWidth, WeatherIconsCustom::kIconHeight, size, size);
}

void drawWeatherIcon(int16_t x, int16_t y, const String& weather, int16_t size) {
  drawWeatherIconByKey(x, y, WeatherIconMapper::dayIconKey(weather), size);
}

void drawTemperatureValue(int16_t x, int16_t baselineY, const String& text, bool compactDegree = false) {
  if (text.endsWith(" °C") || text.endsWith(" C")) {
    String numeric = text;
    numeric.replace(" °C", "");
    numeric.replace(" C", "");
    drawTextAt(x, baselineY, numeric);

    int16_t tbx;
    int16_t tby;
    uint16_t tbw;
    uint16_t tbh;
    display.getTextBounds(numeric, 0, 0, &tbx, &tby, &tbw, &tbh);
    const int16_t ux = x + static_cast<int16_t>(tbw) + 6;
    if (compactDegree) {
      display.drawCircle(ux + 3, baselineY - 9, 3, fgColor);
      display.drawCircle(ux + 3, baselineY - 9, 2, fgColor);
      drawTextAt(ux + 9, baselineY, "C");
    } else {
      display.drawCircle(ux + 6, baselineY - 12, 6, fgColor);
      display.drawCircle(ux + 6, baselineY - 12, 5, fgColor);
      display.drawCircle(ux + 6, baselineY - 12, 4, fgColor);
      drawTextAt(ux + 16, baselineY, "C");
    }
    return;
  }

  drawTextAt(x, baselineY, text);
}

void updateDisplayedTimeFromClock() {
  char timeBuffer[6];
  strftime(timeBuffer, sizeof(timeBuffer), "%H:%M", &localClock);
  timeText = timeBuffer;

  char dateBuffer[11];
  strftime(dateBuffer, sizeof(dateBuffer), "%Y-%m-%d", &localClock);
  dateText = dateBuffer;
}

bool parseTimeDate(const String& hhmm, const String& yyyyMmDd, tm* outClock) {
  if (hhmm.length() != 5 || hhmm.charAt(2) != ':') return false;
  if (yyyyMmDd.length() != 10 || yyyyMmDd.charAt(4) != '-' || yyyyMmDd.charAt(7) != '-') return false;

  const int hour = hhmm.substring(0, 2).toInt();
  const int minute = hhmm.substring(3, 5).toInt();
  const int year = yyyyMmDd.substring(0, 4).toInt();
  const int month = yyyyMmDd.substring(5, 7).toInt();
  const int day = yyyyMmDd.substring(8, 10).toInt();

  if (hour < 0 || hour > 23 || minute < 0 || minute > 59) return false;
  if (year < 2000 || month < 1 || month > 12 || day < 1 || day > 31) return false;

  tm parsed = {};
  parsed.tm_year = year - 1900;
  parsed.tm_mon = month - 1;
  parsed.tm_mday = day;
  parsed.tm_hour = hour;
  parsed.tm_min = minute;

  const time_t epoch = mktime(&parsed);
  if (epoch == static_cast<time_t>(-1)) return false;

  localtime_r(&epoch, &parsed);
  *outClock = parsed;
  return true;
}

bool parseDateTimeYmdHm(const String& yyyyMmDdHhMm, tm* outClock) {
  if (yyyyMmDdHhMm.length() != 16 || yyyyMmDdHhMm.charAt(10) != ' ') {
    return false;
  }
  return parseTimeDate(yyyyMmDdHhMm.substring(11, 16), yyyyMmDdHhMm.substring(0, 10), outClock);
}

bool tmToEpoch(const tm& value, time_t* outEpoch) {
  tm copy = value;
  const time_t epoch = mktime(&copy);
  if (epoch == static_cast<time_t>(-1)) {
    return false;
  }
  *outEpoch = epoch;
  return true;
}

String formatHm(const tm& value) {
  char buffer[6];
  strftime(buffer, sizeof(buffer), "%H:%M", &value);
  return String(buffer);
}

void refreshSunDerivedState() {
  bool nextNight = outsideIsNight;
  String nextLabel = nextSunLabelText;
  String nextTime = nextSunTimeText;

  bool resolved = false;
  if (localClockValid) {
    time_t nowEpoch = 0;
    time_t risingEpoch = 0;
    time_t settingEpoch = 0;
    const bool nowOk = tmToEpoch(localClock, &nowEpoch);
    const bool risingOk = nextRisingValid && tmToEpoch(nextRisingClock, &risingEpoch);
    const bool settingOk = nextSettingValid && tmToEpoch(nextSettingClock, &settingEpoch);
    if (nowOk) {
      const bool risingFuture = risingOk && risingEpoch > nowEpoch;
      const bool settingFuture = settingOk && settingEpoch > nowEpoch;
      if (risingFuture && settingFuture) {
        if (risingEpoch < settingEpoch) {
          nextNight = true;
          nextLabel = "SUNRISE";
          nextTime = formatHm(nextRisingClock);
        } else {
          nextNight = false;
          nextLabel = "SUNSET";
          nextTime = formatHm(nextSettingClock);
        }
        resolved = true;
      } else if (risingFuture) {
        nextNight = true;
        nextLabel = "SUNRISE";
        nextTime = formatHm(nextRisingClock);
        resolved = true;
      } else if (settingFuture) {
        nextNight = false;
        nextLabel = "SUNSET";
        nextTime = formatHm(nextSettingClock);
        resolved = true;
      }
    }
  }

  if (!resolved) {
    String sunState = sunStateText;
    sunState.trim();
    sunState.toLowerCase();
    if (sunState == "below_horizon") {
      nextNight = true;
      nextLabel = "SUNRISE";
      nextTime = nextRisingValid ? formatHm(nextRisingClock) : "--:--";
    } else if (sunState == "above_horizon") {
      nextNight = false;
      nextLabel = "SUNSET";
      nextTime = nextSettingValid ? formatHm(nextSettingClock) : "--:--";
    }
  }

  if (outsideIsNight != nextNight) {
    outsideIsNight = nextNight;
    requestRender(RenderRequest::FULL);
  }
  if (nextSunLabelText != nextLabel) {
    nextSunLabelText = nextLabel;
    requestRender(RenderRequest::CLOCK_ONLY);
  }
  if (nextSunTimeText != nextTime) {
    nextSunTimeText = nextTime;
    requestRender(RenderRequest::CLOCK_ONLY);
  }
}

bool advanceLocalClockOneMinute() {
  if (!localClockValid) return false;
  time_t epoch = mktime(&localClock);
  if (epoch == static_cast<time_t>(-1)) {
    localClockValid = false;
    return false;
  }
  epoch += 60;
  localtime_r(&epoch, &localClock);
  updateDisplayedTimeFromClock();
  refreshSunDerivedState();
  return true;
}

void drawFrameAndTitles() {
  const int16_t midX = static_cast<int16_t>(display.width() / 2) + kDividerXShift;
  const int16_t midY = static_cast<int16_t>(display.height() / 2) + kDividerYShift;
  display.drawLine(midX, 0, midX, display.height() - 1, fgColor);
  display.drawLine(0, midY, display.width() - 1, midY, fgColor);

  display.setFont(&FreeMonoBold9pt7b);
  drawTextAt(midX + 6, 19 + kContentYOffset, "OUTSIDE");
  drawTextAt(6, midY + 14 + kContentYOffset, "INSIDE");
}

void getTimePanelBounds(int16_t* outX, int16_t* outY, int16_t* outWidth, int16_t* outHeight) {
  const int16_t midX = static_cast<int16_t>(display.width() / 2) + kDividerXShift;
  const int16_t midY = static_cast<int16_t>(display.height() / 2) + kDividerYShift;
  const int16_t rawWidth = midX - kClockPartialDividerMargin;
  const int16_t rawHeight = midY - kClockPartialDividerMargin;

  *outX = 0;
  *outY = 0;
  *outWidth = rawWidth > 0 ? static_cast<int16_t>(rawWidth & ~0x7) : 0;
  *outHeight = rawHeight > 0 ? rawHeight : 0;
}

void drawTimeSection() {
  const int16_t midX = static_cast<int16_t>(display.width() / 2) + kDividerXShift;
  const int16_t midY = static_cast<int16_t>(display.height() / 2) + kDividerYShift;
  const int16_t centerX = midX / 2;

  int16_t timeTbx;
  int16_t timeTby;
  uint16_t timeTbw;
  uint16_t timeTbh;
  display.setFont(&FreeMonoBold24pt7b);
  display.getTextBounds(timeText, 0, 0, &timeTbx, &timeTby, &timeTbw, &timeTbh);

  int16_t dateTbx;
  int16_t dateTby;
  uint16_t dateTbw;
  uint16_t dateTbh;
  display.setFont(&FreeMonoBold12pt7b);
  display.getTextBounds(dateText, 0, 0, &dateTbx, &dateTby, &dateTbw, &dateTbh);

  const String sunLine = nextSunLabelText + " " + nextSunTimeText;
  int16_t sunTbx;
  int16_t sunTby;
  uint16_t sunTbw;
  uint16_t sunTbh;
  display.setFont(&FreeMonoBold9pt7b);
  display.getTextBounds(sunLine, 0, 0, &sunTbx, &sunTby, &sunTbw, &sunTbh);

  const int16_t panelTop = kContentYOffset;
  const int16_t panelBottom = midY;
  const int16_t timeDateGap = 6;
  const int16_t sunGap = 29;
  const int16_t totalHeight =
      static_cast<int16_t>(timeTbh) + timeDateGap + static_cast<int16_t>(dateTbh) + sunGap + static_cast<int16_t>(sunTbh);
  const int16_t blockTop = panelTop + (panelBottom - panelTop - totalHeight) / 2;
  const int16_t timeBaseline = blockTop - timeTby;
  const int16_t dateTop = blockTop + static_cast<int16_t>(timeTbh) + timeDateGap;
  const int16_t dateBaseline = dateTop - dateTby;
  const int16_t sunTop = dateTop + static_cast<int16_t>(dateTbh) + sunGap;
  const int16_t sunBaseline = sunTop - sunTby;

  display.setFont(&FreeMonoBold24pt7b);
  drawCentered(centerX, timeBaseline, timeText);
  display.setFont(&FreeMonoBold12pt7b);
  drawCentered(centerX, dateBaseline, dateText);
  display.setFont(&FreeMonoBold9pt7b);
  drawCentered(centerX, sunBaseline, sunLine);
}

void drawOutSection() {
  const int16_t midX = static_cast<int16_t>(display.width() / 2) + kDividerXShift;
  const int16_t labelX = midX + 6;
  const int16_t valueX = labelX + 46;
  const int16_t tempBaseline = 56 + kContentYOffset + kOutSectionTopPadding;
  const int16_t humBaseline = tempBaseline + kSectionValueSpacing;
  const int16_t aqiBaseline = humBaseline + kSectionValueSpacing;

  display.setFont(&FreeMonoBold9pt7b);
  drawTextAt(labelX, humBaseline, "HUM");
  drawTextAt(labelX, aqiBaseline, "AQI");

  display.setFont(&FreeMonoBold18pt7b);
  drawTemperatureValue(labelX, tempBaseline, outTempText);

  display.setFont(&FreeMonoBold12pt7b);
  drawTextAt(valueX, humBaseline, outHumText);
  drawTextAt(valueX, aqiBaseline, outAqiText);
  const String iconWeatherKey = outsideIsNight
      ? WeatherIconMapper::nightIconKey(outWeatherText)
      : WeatherIconMapper::dayIconKey(outWeatherText);
  drawWeatherIconByKey(display.width() - kOutIconRightMargin - kOutIconSize, kOutSectionTopPadding, iconWeatherKey, kOutIconSize);
}

void drawInSection() {
  const int16_t midY = static_cast<int16_t>(display.height() / 2) + kDividerYShift;
  const int16_t labelX = 6;
  const int16_t valueX = labelX + 46;
  const int16_t tempBaseline = midY + 52 + kContentYOffset;
  const int16_t humBaseline = tempBaseline + kSectionValueSpacing;
  const int16_t pressureBaseline = humBaseline + kSectionValueSpacing;
  const int16_t pm25Baseline = pressureBaseline + kSectionValueSpacing;

  display.setFont(&FreeMonoBold9pt7b);
  drawTextAt(labelX, humBaseline, "HUM");
  drawTextAt(labelX, pressureBaseline, "PRES");
  drawTextAt(labelX, pm25Baseline, "PM25");

  display.setFont(&FreeMonoBold18pt7b);
  drawTemperatureValue(labelX, tempBaseline, inTempText);

  display.setFont(&FreeMonoBold12pt7b);
  drawTextAt(valueX, humBaseline, inHumText);
  drawTextAt(valueX, pressureBaseline, inPressureText);
  drawTextAt(valueX, pm25Baseline, inPm25Text);
}

struct ForecastRowData {
  const char* title;
  const String& maxTemp;
  const String& minTemp;
  const String& weather;
  int16_t top;
  int16_t bottom;
};

void drawForecastRow(const ForecastRowData& row, int16_t labelX, int16_t tempX) {
  const int16_t iconY = row.top + ((row.bottom - row.top) - kForecastIconSize) / 2;
  const int16_t titleBaseline = row.top + 14 + kContentYOffset;
  const int16_t maxBaseline = row.top + 42 + kContentYOffset;
  const int16_t minBaseline = row.top + 68 + kContentYOffset;

  display.setFont(&FreeMonoBold9pt7b);
  drawTextAt(labelX, titleBaseline, row.title);
  drawTextAt(labelX, maxBaseline, "MAX");
  drawTextAt(labelX, minBaseline, "MIN");

  display.setFont(&FreeMonoBold12pt7b);
  drawTemperatureValue(tempX, maxBaseline, row.maxTemp, true);
  drawTemperatureValue(tempX, minBaseline, row.minTemp, true);
  drawWeatherIcon(display.width() - kForecastIconRightMargin - kForecastIconSize, iconY, row.weather, kForecastIconSize);
}

void drawForecastSection() {
  const int16_t midX = static_cast<int16_t>(display.width() / 2) + kDividerXShift;
  const int16_t midY = static_cast<int16_t>(display.height() / 2) + kDividerYShift;
  const int16_t sectionTop = midY;
  const int16_t sectionBottom = display.height();
  const int16_t splitY = sectionTop + (sectionBottom - sectionTop) / 2;
  display.drawLine(midX, splitY, display.width() - 1, splitY, fgColor);

  const int16_t labelX = midX + 6;
  const int16_t tempX = labelX + 46;

  const ForecastRowData todayRow = {"TODAY", todayMaxText, todayMinText, todayWeatherText, sectionTop, splitY};
  const ForecastRowData tomorrowRow = {"TOMORROW", tomorrowMaxText, tomorrowMinText, tomorrowWeatherText, splitY, sectionBottom};

  drawForecastRow(todayRow, labelX, tempX);
  drawForecastRow(tomorrowRow, labelX, tempX);
}

void setValue(String* target, const String& value) {
  String next = value;
  next.trim();
  if (next.isEmpty()) {
    next = "--";
  }
  if (*target == next) {
    return;
  }
  *target = next;
  requestRender(RenderRequest::FULL);
}

}  // namespace

void begin() {
  syncThemeFromSettings();
  const uint32_t now = millis();
  if (!localClockValid) {
    lastClockTickMs = now;
  }
  pendingRender = RenderRequest::FULL;
}

void loop() {
  syncThemeFromSettings();

  if (localClockValid) {
    while (millis() - lastClockTickMs >= kMinuteMs) {
      lastClockTickMs += kMinuteMs;
      if (advanceLocalClockOneMinute()) {
        requestRender(RenderRequest::CLOCK_ONLY);
      }
    }
  }

  if (pendingRender != RenderRequest::NONE) {
    render();
  }
}

bool renderTimeSectionOnly() {
  int16_t x;
  int16_t y;
  int16_t width;
  int16_t height;
  getTimePanelBounds(&x, &y, &width, &height);
  if (width <= 0 || height <= 0) {
    return false;
  }

  display.setPartialWindow(x, y, width, height);
  display.firstPage();
  do {
    display.fillRect(x, y, width, height, bgColor);
    display.setTextColor(fgColor);
    display.setTextSize(1);
    drawTimeSection();
  } while (display.nextPage());

  return true;
}

void render() {
  ActivityLed::ScopedNetworkActivity ledDuringRefresh;
  syncThemeFromSettings();

  RenderRequest request = pendingRender;
  if (request == RenderRequest::NONE) {
    request = RenderRequest::FULL;
  }

  if (request == RenderRequest::CLOCK_ONLY) {
    if (renderTimeSectionOnly()) {
      pendingRender = RenderRequest::NONE;
      return;
    }
  }

  const uint8_t fullRefreshEvery = AppConfig::kMainFullRefreshEveryN;
  const bool useFullRefresh = fullRefreshEvery > 0 && (renderCount % fullRefreshEvery == 0);
  if (useFullRefresh) {
    display.setFullWindow();
  } else {
    display.setPartialWindow(0, 0, display.width(), display.height());
  }
  display.firstPage();
  do {
    display.fillScreen(bgColor);
    display.setTextColor(fgColor);
    display.setTextSize(1);

    drawFrameAndTitles();
    drawTimeSection();
    drawOutSection();
    drawInSection();
    drawForecastSection();
  } while (display.nextPage());
  renderCount++;
  pendingRender = RenderRequest::NONE;
}

void setTimeAndDate(const String& hhmm, const String& yyyyMmDd) {
  tm parsedClock;
  if (parseTimeDate(hhmm, yyyyMmDd, &parsedClock)) {
    const String previousTime = timeText;
    const String previousDate = dateText;
    localClock = parsedClock;
    localClockValid = true;
    lastClockTickMs = millis();
    updateDisplayedTimeFromClock();
    if (timeText != previousTime || dateText != previousDate) {
      requestRender(RenderRequest::CLOCK_ONLY);
    }
    refreshSunDerivedState();
    return;
  }

  bool changed = false;
  if (!hhmm.isEmpty() && timeText != hhmm) {
    timeText = hhmm;
    changed = true;
  }
  if (!yyyyMmDd.isEmpty() && dateText != yyyyMmDd) {
    dateText = yyyyMmDd;
    changed = true;
  }
  if (changed) {
    requestRender(RenderRequest::CLOCK_ONLY);
  }
}

void setSunSchedule(const String& nextRisingYmdHm, const String& nextSettingYmdHm, const String& sunState) {
  tm parsed;
  nextRisingValid = parseDateTimeYmdHm(nextRisingYmdHm, &parsed);
  if (nextRisingValid) {
    nextRisingClock = parsed;
  }

  nextSettingValid = parseDateTimeYmdHm(nextSettingYmdHm, &parsed);
  if (nextSettingValid) {
    nextSettingClock = parsed;
  }

  String nextSunState = sunState;
  nextSunState.trim();
  nextSunState.toLowerCase();
  sunStateText = nextSunState;
  refreshSunDerivedState();
}

void setOutsideTemperature(const String& outsideTemperature) { setValue(&outTempText, outsideTemperature); }
void setOutsideHumidity(const String& outsideHumidity) { setValue(&outHumText, outsideHumidity); }
void setOutsideAqi(const String& outsideAqi) { setValue(&outAqiText, outsideAqi); }
void setOutsideWeather(const String& outsideWeather) { setValue(&outWeatherText, outsideWeather); }

void setInsideTemperature(const String& insideTemperature) { setValue(&inTempText, insideTemperature); }
void setInsideHumidity(const String& insideHumidity) { setValue(&inHumText, insideHumidity); }
void setInsidePressure(const String& insidePressure) { setValue(&inPressureText, insidePressure); }
void setInsidePm25(const String& insidePm25) { setValue(&inPm25Text, insidePm25); }

void setTodayMaxTemperature(const String& todayMaxTemperature) { setValue(&todayMaxText, todayMaxTemperature); }
void setTodayMinTemperature(const String& todayMinTemperature) { setValue(&todayMinText, todayMinTemperature); }
void setTodayWeather(const String& todayWeather) { setValue(&todayWeatherText, todayWeather); }
void setTomorrowMaxTemperature(const String& tomorrowMaxTemperature) { setValue(&tomorrowMaxText, tomorrowMaxTemperature); }
void setTomorrowMinTemperature(const String& tomorrowMinTemperature) { setValue(&tomorrowMinText, tomorrowMinTemperature); }
void setTomorrowWeather(const String& tomorrowWeather) { setValue(&tomorrowWeatherText, tomorrowWeather); }

}  // namespace MainApp
