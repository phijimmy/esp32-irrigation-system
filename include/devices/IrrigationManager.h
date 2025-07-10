#ifndef IRRIGATION_MANAGER_H
#define IRRIGATION_MANAGER_H

#include "devices/BME280Device.h"
#include "devices/SoilMoistureSensor.h"
#include "devices/MQ135Sensor.h"
#include "system/TimeManager.h"
#include "config/ConfigManager.h" // Corrected include path

class IrrigationManager {
public:
    IrrigationManager();
    void begin(BME280Device* bme, SoilMoistureSensor* soil, MQ135Sensor* mq135, TimeManager* timeMgr);
    void trigger(); // Start the irrigation reading sequence
    void update();  // Call this in loop to process state
    void checkAndRunScheduled(); // Check if it's time to run scheduled irrigation
    bool isRunning() const;
    bool isComplete() const;
    void reset();
    void waterNow();
private:
    enum State { IDLE, START, BME_READING, SOIL_READING, MQ135_READING, WATER_NOW, COMPLETE };
    State state = IDLE;
    BME280Device* bme280 = nullptr;
    SoilMoistureSensor* soilSensor = nullptr;
    MQ135Sensor* mq135Sensor = nullptr;
    TimeManager* timeManager = nullptr;
    ConfigManager* configManager = nullptr; // Add ConfigManager pointer
    RelayController* relayController = nullptr;
    unsigned long stateStart = 0;
    bool completePrinted = false;
    bool wateringActive = false;
    unsigned long wateringStart = 0;
    int wateringDuration = 0;
    // Storage for latest readings
    float lastAvgTemp = 0;
    float lastAvgHumidity = 0;
    float lastAvgPressure = 0;
    float lastAvgHeatIndex = 0;
    float lastAvgDewPoint = 0;
    float lastAvgSoilRaw = 0;
    float lastAvgSoilVoltage = 0;
    float lastAvgSoilPercent = 0;
    float lastAvgSoilCorrected = 0;
    float lastAvgAir = 0;
    time_t lastReadingTimestamp = 0;
    void startNextState(State next);
public:
    float getLastAvgTemp() const { return lastAvgTemp; }
    float getLastAvgHumidity() const { return lastAvgHumidity; }
    float getLastAvgPressure() const { return lastAvgPressure; }
    float getLastAvgHeatIndex() const { return lastAvgHeatIndex; }
    float getLastAvgDewPoint() const { return lastAvgDewPoint; }
    float getLastAvgSoilRaw() const { return lastAvgSoilRaw; }
    float getLastAvgSoilVoltage() const { return lastAvgSoilVoltage; }
    float getLastAvgSoilPercent() const { return lastAvgSoilPercent; }
    float getLastAvgSoilCorrected() const { return lastAvgSoilCorrected; }
    float getLastAvgAir() const { return lastAvgAir; }
    time_t getLastReadingTimestamp() const { return lastReadingTimestamp; }
    void setConfigManager(ConfigManager* cfg) { configManager = cfg; } // Setter for ConfigManager
    void setRelayController(RelayController* rc) { relayController = rc; }
};

#endif // IRRIGATION_MANAGER_H
