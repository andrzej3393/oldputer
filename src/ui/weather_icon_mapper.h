#ifndef WEATHER_ICON_MAPPER_H
#define WEATHER_ICON_MAPPER_H

#include <Arduino.h>

namespace WeatherIconMapper {

String dayIconKey(const String& weather);
String nightIconKey(const String& weather);

}  // namespace WeatherIconMapper

#endif
