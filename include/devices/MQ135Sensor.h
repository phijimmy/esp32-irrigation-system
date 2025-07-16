    // ...existing code...
#ifndef MQ135_SENSOR_H
#define MQ135_SENSOR_H

#include <Arduino.h>
#include <ctime>
#include "system/ADS1115Manager.h"
#include "config/ConfigManager.h"
#include "devices/RelayController.h"
#include "system/TimeManager.h"
#include "diagnostics/DiagnosticManager.h"

class TimeManager;

class MQ135Sensor {
public:
    void forceIdle();
    enum State { IDLE, WARMING_UP, READING, ERROR };
    MQ135Sensor();
    void begin(ADS1115Manager* adsMgr, ConfigManager* configMgr, RelayController* relayCtrl, DiagnosticManager* diagMgr = nullptr);
    void startReading(); // Activates relay, starts warmup
    bool readyForReading() const; // True if warmup time has elapsed
    void takeReading(); // Takes the reading, deactivates relay
    struct Reading {
        int16_t raw = 0;
        float voltage = 0;
        time_t timestamp = 0;
        bool valid = false;
        // Averaged (filtered) values
        float avgRaw = 0;
        float avgVoltage = 0;
    };
    const Reading& getLastReading() const;
    bool isWarmingUp() const { return warmingUp; }
    unsigned long getWarmupStart() const { return warmupStart; }
    int getWarmupTimeSec() const { return warmupTimeSec; }
    void setWarmupTimeSec(int sec) { warmupTimeSec = sec; }
    void setTimeManager(TimeManager* tm) { timeManager = tm; }
    State getState() const { return state; }
    const char* stateToString() const {
        switch (state) {
            case IDLE: return "idle";
            case WARMING_UP: return "warming_up";
            case READING: return "reading";
            case ERROR: return "error";
            default: return "unknown";
        }
    }
    // Returns a qualitative air quality label based on avgVoltage
    static const char* getAirQualityLabel(float voltage) {
        if (voltage < 0.25f) return "Excellent";
        if (voltage < 0.35f) return "Good";
        if (voltage < 0.45f) return "Moderate";
        if (voltage < 0.60f) return "Poor";
        return "Very Poor";
    }
private:
    ADS1115Manager* ads = nullptr;
    ConfigManager* config = nullptr;
    RelayController* relay = nullptr;
    TimeManager* timeManager = nullptr;
    DiagnosticManager* diagnosticManager = nullptr;
    static constexpr uint8_t channel = 1; // A1
    static constexpr ADS1115Manager::Gain gain = ADS1115Manager::GAIN_TWOTHIRDS; // For 6V (±6.144V)
    Reading lastReading{};
    unsigned long warmupStart = 0;
    int warmupTimeSec = 60;
    bool warmingUp = false;
    State state = IDLE;
    void filterAndAverage(float* rawVals, float* voltVals, int count, float& avgRaw, float& avgVolt);
};

#endif // MQ135_SENSOR_H
