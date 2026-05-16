# Home Assistant Template Sensors (Today + Tomorrow)

Home Assistant usually does not expose forecast days as standalone entities.
Create template sensors from `weather.get_forecasts` response.

## Add to `configuration.yaml`

```yaml
template:
  - trigger:
      - trigger: time_pattern
        minutes: "/10"
    action:
      - action: weather.get_forecasts
        target:
          entity_id: weather.forecast_home
        data:
          type: daily
        response_variable: wx
    sensor:
      - name: forecast_today_condition
        state: >
          {{ wx['weather.forecast_home'].forecast[0].condition
             if wx and wx['weather.forecast_home'].forecast | count > 0 else 'unknown' }}

      - name: forecast_today_temp_max
        unit_of_measurement: "°C"
        state: >
          {{ wx['weather.forecast_home'].forecast[0].temperature
             if wx and wx['weather.forecast_home'].forecast | count > 0 else 'unknown' }}

      - name: forecast_today_temp_min
        unit_of_measurement: "°C"
        state: >
          {{ wx['weather.forecast_home'].forecast[0].templow
             if wx and wx['weather.forecast_home'].forecast | count > 0 else 'unknown' }}

      - name: forecast_tomorrow_condition
        state: >
          {{ wx['weather.forecast_home'].forecast[1].condition
             if wx and wx['weather.forecast_home'].forecast | count > 1 else 'unknown' }}

      - name: forecast_tomorrow_temp_max
        unit_of_measurement: "°C"
        state: >
          {{ wx['weather.forecast_home'].forecast[1].temperature
             if wx and wx['weather.forecast_home'].forecast | count > 1 else 'unknown' }}

      - name: forecast_tomorrow_temp_min
        unit_of_measurement: "°C"
        state: >
          {{ wx['weather.forecast_home'].forecast[1].templow
             if wx and wx['weather.forecast_home'].forecast | count > 1 else 'unknown' }}
```

## Reload

1. Save config.
2. In Home Assistant, go to `Developer Tools -> YAML`.
3. Click `Reload Template Entities`.

## Resulting entities

- `sensor.forecast_today_condition`
- `sensor.forecast_today_temp_max`
- `sensor.forecast_today_temp_min`
- `sensor.forecast_tomorrow_condition`
- `sensor.forecast_tomorrow_temp_max`
- `sensor.forecast_tomorrow_temp_min`

Use these entity IDs in the ESP config page in sections `Today` and `Tomorrow`.
