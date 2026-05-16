#include "weather_icon_mapper.h"

namespace WeatherIconMapper {

namespace {

enum class WeatherKind {
  SUNNY,
  CLEAR,
  PARTLY_CLOUDY,
  CLOUDY,
  FOG,
  RAIN,
  SNOW,
  SLEET,
  STORMS,
  HAZY,
  UNKNOWN,
};

WeatherKind normalizeWeather(const String& value) {
  String key = value;
  key.trim();
  key.toLowerCase();

  if (key == "sunny") return WeatherKind::SUNNY;
  if (key == "clear" || key == "clear-night") return WeatherKind::CLEAR;
  if (key == "partlycloudy") return WeatherKind::PARTLY_CLOUDY;
  if (key == "cloudy" || key == "overcast") return WeatherKind::CLOUDY;
  if (key == "fog" || key == "haze") return WeatherKind::FOG;
  if (key == "rainy" || key == "pouring") return WeatherKind::RAIN;
  if (key == "snowy") return WeatherKind::SNOW;
  if (key == "snowy-rainy" || key == "hail") return WeatherKind::SLEET;
  if (key == "lightning" || key == "lightning-rainy") return WeatherKind::STORMS;
  if (key == "windy" || key == "windy-variant") return WeatherKind::HAZY;
  return WeatherKind::UNKNOWN;
}

}  // namespace

String dayIconKey(const String& weather) {
  const WeatherKind kind = normalizeWeather(weather);
  if (kind == WeatherKind::SUNNY) return "sunny";
  if (kind == WeatherKind::CLEAR) return "clear";
  if (kind == WeatherKind::PARTLY_CLOUDY) return "partlycloudy";
  if (kind == WeatherKind::CLOUDY) return "cloudy";
  if (kind == WeatherKind::FOG) return "fog";
  if (kind == WeatherKind::RAIN) return "rain";
  if (kind == WeatherKind::SNOW) return "snow";
  if (kind == WeatherKind::SLEET) return "sleet";
  if (kind == WeatherKind::STORMS) return "tstorms";
  if (kind == WeatherKind::HAZY) return "hazy";
  return "unknown";
}

String nightIconKey(const String& weather) {
  const WeatherKind kind = normalizeWeather(weather);
  if (kind == WeatherKind::SUNNY || kind == WeatherKind::CLEAR) return "nt_clear";
  if (kind == WeatherKind::PARTLY_CLOUDY) return "nt_partlycloudy";
  if (kind == WeatherKind::CLOUDY) return "nt_cloudy";
  if (kind == WeatherKind::FOG) return "nt_fog";
  if (kind == WeatherKind::RAIN) return "nt_rain";
  if (kind == WeatherKind::SNOW) return "nt_snow";
  if (kind == WeatherKind::SLEET) return "nt_sleet";
  if (kind == WeatherKind::STORMS) return "nt_tstorms";
  if (kind == WeatherKind::HAZY) return "nt_hazy";
  return "nt_unknown";
}

}  // namespace WeatherIconMapper
