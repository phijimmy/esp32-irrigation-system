
#ifndef SOIL_MOISTURE_SENSOR_H
#define SOIL_MOISTURE_SENSOR_H

#include <Arduino.h>
#include <ctime>
#include "system/ADS1115Manager.h"
#include "config/ConfigManager.h"
#include "system/TimeManager.h"
#include "diagnostics/DiagnosticManager.h"

class SoilMoistureSensor {
public:
    float getLastPercent();
    float getLastAvgPercent();
    void onNewReading(float percent);
private:
    float lastPercent = -1.0f;
public:
    void forceIdle();
    enum State {
        IDLE,
        STABILISING,
        READING,
        ERROR
    };
    struct Reading {
        int16_t raw;
        float voltage;
        float percent;
        time_t timestamp;
        // Averaged (filtered) values
        float avgRaw = 0;
        float avgVoltage = 0;
        float avgPercent = 0;
    };
    SoilMoistureSensor();
    void begin(ADS1115Manager* adsMgr, ConfigManager* configMgr = nullptr, TimeManager* timeMgr = nullptr, DiagnosticManager* diagMgr = nullptr);
    int16_t readRaw();
    float readVoltage();
    void readBoth(int16_t& raw, float& voltage);
    float readPercent();
    void takeReading();
    void beginStabilisation(); // Start stabilisation timer
    bool readyForReading() const; // True if stabilisation time has elapsed
    const Reading& getLastReading() const;
    void printReading() const; // Print the last reading to Serial
    unsigned long getStabilisationStart() const { return stabilisationStart; }
    int getStabilisationTimeSec() const { return stabilisationTimeSec; }
    void setStabilisationTimeSec(int sec) { stabilisationTimeSec = sec; }
    void setPowerGpio(int gpio) { soilPowerGpio = gpio; }
    int getPowerGpio() const { return soilPowerGpio; }
    static const char* stateToString(State s) {
        switch (s) {
            case IDLE: return "idle";
            case STABILISING: return "stabilising";
            case READING: return "reading";
            case ERROR: return "error";
            default: return "unknown";
        }
    }
    State getState() const { return state; }
private:
    ADS1115Manager* ads = nullptr;
    ConfigManager* config = nullptr;
    TimeManager* timeManager = nullptr;
    DiagnosticManager* diagnosticManager = nullptr;
    static constexpr uint8_t channel = 0; // A0
    static constexpr ADS1115Manager::Gain gain = ADS1115Manager::GAIN_TWOTHIRDS; // For 3.3V
    Reading lastReading{};
    unsigned long stabilisationStart = 0;
    int stabilisationTimeSec = 10;
    int soilPowerGpio = -1;
    State state = IDLE;
    void filterAndAverage(float* rawVals, float* voltVals, float* percentVals, int count, float& avgRaw, float& avgVolt, float& avgPercent);
};

#endif // SOIL_MOISTURE_SENSOR_H
