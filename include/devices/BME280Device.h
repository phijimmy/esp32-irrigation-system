    // ...existing code...
#ifndef BME280_DEVICE_H
#define BME280_DEVICE_H

#include <Arduino.h>
#include <Adafruit_BME280.h>
#include <RTClib.h> // Include RTClib for DateTime
// Forward declaration to avoid circular include
class I2CManager;
#include "diagnostics/DiagnosticManager.h"

struct BME280Reading {
    float temperature;
    float humidity;
    float pressure;
    float heatIndex;
    float dewPoint;
    DateTime timestamp; // Add timestamp
    bool valid;
    // Averaged (filtered) values
    float avgTemperature = 0;
    float avgHumidity = 0;
    float avgPressure = 0;
    float avgHeatIndex = 0;
    float avgDewPoint = 0;
};

class TimeManager; // Forward declaration
class BME280Device {
public:
    void forceIdle();
    enum State { UNINITIALIZED, READY, READING, UPDATING, ERROR };
    static const char* stateToString(State s) {
        switch (s) {
            case UNINITIALIZED: return "uninitialized";
            case READY: return "ready";
            case READING: return "reading";
            case UPDATING: return "updating";
            case ERROR: return "error";
            default: return "unknown";
        }
    }
    BME280Device(uint8_t address, I2CManager* i2c, DiagnosticManager* diag);
    void setTimeManager(TimeManager* timeMgr);
    bool begin();
    BME280Reading readData();
    const BME280Reading& getLastReading() const { return lastReading; }
    uint8_t getAddress() const { return address; }
    State getState() const { return state; }
    const String& getLastError() const { return lastError; }
private:
    uint8_t address;
    I2CManager* i2cManager;
    DiagnosticManager* diagnosticManager;
    TimeManager* timeManager = nullptr;
    Adafruit_BME280 bme;
    bool initialized = false;
    BME280Reading lastReading; // Store the last reading
    State state = UNINITIALIZED;
    String lastError;
    float computeHeatIndex(float t, float h);
    float computeDewPoint(float t, float h);
    void filterAndAverage(const BME280Reading* readings, int count, BME280Reading& avgResult);
};

#endif // BME280_DEVICE_H
