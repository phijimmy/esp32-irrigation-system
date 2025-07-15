
#include <RTClib.h>

class BME280Device;

class SoilMoistureSensor;

class ReadingManager {
public:
    ReadingManager(BME280Device* bme, SoilMoistureSensor* soil);
    void begin();
    void loop(const DateTime& now);
private:
    BME280Device* bme280;
    SoilMoistureSensor* soilSensor;
    int lastHour = -1;
};
