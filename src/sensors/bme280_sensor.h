#ifndef BME280_SENSOR_H
#define BME280_SENSOR_H

namespace Bme280Sensor {

void begin();
void loop();
bool getTemperatureC(float* outValue);
bool getHumidity(float* outValue);
bool getPressureHpa(float* outValue);
bool isMqttConnected();

}  // namespace Bme280Sensor

#endif
