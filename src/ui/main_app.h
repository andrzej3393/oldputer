#ifndef MAIN_APP_H
#define MAIN_APP_H

#include <Arduino.h>

namespace MainApp {

void begin();
void loop();
void render();
void setTimeAndDate(const String& hhmm, const String& yyyyMmDd);
void setSunSchedule(const String& nextRisingYmdHm, const String& nextSettingYmdHm, const String& sunState);
void setOutsideTemperature(const String& outsideTemperature);
void setOutsideHumidity(const String& outsideHumidity);
void setOutsideAqi(const String& outsideAqi);
void setOutsideWeather(const String& outsideWeather);

void setInsideTemperature(const String& insideTemperature);
void setInsideHumidity(const String& insideHumidity);
void setInsidePressure(const String& insidePressure);
void setInsidePm25(const String& insidePm25);

void setTodayMaxTemperature(const String& todayMaxTemperature);
void setTodayMinTemperature(const String& todayMinTemperature);
void setTodayWeather(const String& todayWeather);
void setTomorrowMaxTemperature(const String& tomorrowMaxTemperature);
void setTomorrowMinTemperature(const String& tomorrowMinTemperature);
void setTomorrowWeather(const String& tomorrowWeather);

}  // namespace MainApp

#endif
