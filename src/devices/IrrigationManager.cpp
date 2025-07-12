#include "devices/IrrigationManager.h"
#include <Arduino.h>
#include <time.h>

IrrigationManager::IrrigationManager() {}

void IrrigationManager::begin(BME280Device* bme, SoilMoistureSensor* soil, MQ135Sensor* mq135, TimeManager* timeMgr) {
    bme280 = bme;
    soilSensor = soil;
    mq135Sensor = mq135;
    timeManager = timeMgr;
    state = IDLE;
}

void IrrigationManager::trigger() {
    if (state == IDLE || state == COMPLETE) {
        Serial.println("[IrrigationManager] Triggered: Starting irrigation reading sequence.");
        state = START;
        stateStart = millis();
        completePrinted = false;
    }
}

void IrrigationManager::waterNow() {
    if (state == IDLE || state == COMPLETE) {
        Serial.println("[IrrigationManager] Water Now: Activating relay 1 for watering duration.");
        // Get watering duration from config or use default
        if (configManager) {
            wateringDuration = configManager->getInt("watering_duration_sec", 60);
        } else {
            wateringDuration = 60;
        }
        if (relayController) relayController->setRelayMode(1, Relay::ON); // Relay 1 ON
        wateringActive = true;
        wateringStart = millis();
        state = WATER_NOW;
        stateStart = millis();
        completePrinted = false;
    }
}

void IrrigationManager::update() {
    static unsigned long lastProgressPrint = 0;
    static BME280Reading bmeAvg{};
    static SoilMoistureSensor::Reading soilAvg{};
    static float airAvg = 0;
    static float wateringThreshold = 0;
    switch (state) {
        case IDLE:
            break;
        case START:
            Serial.println("[IrrigationManager] Starting BME280 reading...");
            if (bme280) {
                BME280Reading r = bme280->readData();
                if (r.valid) {
                    char timeStr[32] = "";
                    if (r.timestamp.isValid()) {
                        snprintf(timeStr, sizeof(timeStr), "%04d-%02d-%02d %02d:%02d:%02d", r.timestamp.year(), r.timestamp.month(), r.timestamp.day(), r.timestamp.hour(), r.timestamp.minute(), r.timestamp.second());
                    } else {
                        strcpy(timeStr, "N/A");
                    }
                    Serial.printf("[IrrigationManager][BME280] Reading: T=%.2fC, H=%.2f%%, P=%.2fhPa, HI=%.2fC, DP=%.2fC | avgT=%.2fC, avgH=%.2f%%, avgP=%.2fhPa, avgHI=%.2fC, avgDP=%.2fC, time=%s\n",
                        r.temperature, r.humidity, r.pressure, r.heatIndex, r.dewPoint,
                        r.avgTemperature, r.avgHumidity, r.avgPressure, r.avgHeatIndex, r.avgDewPoint, timeStr);
                    bmeAvg = r;
                } else {
                    Serial.println("[IrrigationManager][BME280] Reading: not valid");
                    memset(&bmeAvg, 0, sizeof(bmeAvg));
                }
            }
            if (soilSensor) {
                Serial.println("[IrrigationManager] Starting soil moisture sensor stabilisation...");
                soilSensor->beginStabilisation();
                lastProgressPrint = millis();
            }
            startNextState(BME_READING);
            break;
        case BME_READING:
            if (soilSensor) {
                unsigned long now = millis();
                int elapsed = (now - soilSensor->getStabilisationStart()) / 1000;
                int total = soilSensor->getStabilisationTimeSec();
                if (now - lastProgressPrint >= 1000) {
                    Serial.printf("[IrrigationManager][SoilMoistureSensor] Stabilising... %d/%d seconds\n", elapsed, total);
                    lastProgressPrint = now;
                }
                if (soilSensor->readyForReading()) {
                    Serial.println("[IrrigationManager] Soil sensor stabilisation complete, taking reading...");
                    soilSensor->takeReading();
                    soilSensor->printReading();
                    soilAvg = soilSensor->getLastReading();
                    // Clamp soil moisture percent to [0, 100]
                    soilAvg.avgPercent = std::max(0.0f, std::min(100.0f, soilAvg.avgPercent));
                    soilAvg.percent = std::max(0.0f, std::min(100.0f, soilAvg.percent));
                    startNextState(SOIL_READING);
                }
            }
            break;
        case SOIL_READING:
            if (mq135Sensor) {
                Serial.println("[IrrigationManager] Starting MQ135 air quality sensor warmup...");
                mq135Sensor->startReading();
                lastProgressPrint = millis();
            }
            startNextState(MQ135_READING);
            break;
        case MQ135_READING:
            if (mq135Sensor) {
                unsigned long now = millis();
                int elapsed = (now - mq135Sensor->getWarmupStart()) / 1000;
                int total = mq135Sensor->isWarmingUp() ? mq135Sensor->getWarmupTimeSec() : 0;
                if (mq135Sensor->isWarmingUp() && now - lastProgressPrint >= 1000) {
                    Serial.printf("[IrrigationManager][MQ135Sensor] Warming up... %d/%d seconds\n", elapsed, total);
                    lastProgressPrint = now;
                }
                if (mq135Sensor->readyForReading()) {
                    Serial.println("[IrrigationManager] MQ135 warmup complete, taking air quality reading...");
                    mq135Sensor->takeReading();
                    const MQ135Sensor::Reading& r = mq135Sensor->getLastReading();
                    char timeStr[32];
                    struct tm* tm_info = localtime(&r.timestamp);
                    strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", tm_info);
                    Serial.printf("[IrrigationManager][MQ135Sensor] Reading: raw=%d, voltage=%.4f V | avgRaw=%.1f, avgVoltage=%.4f V, AQI=%s, timestamp=%s\n",
                        r.raw, r.voltage, r.avgRaw, r.avgVoltage, MQ135Sensor::getAirQualityLabel(r.avgVoltage), timeStr);
                    airAvg = r.avgVoltage;
                    // Store timestamp of completion
                    lastReadingTimestamp = r.timestamp;
                    // Apply linear correction: corrected = soilAvgPercent - k * (bmeAvgTemp - t0)
                    float k = 0.5f;
                    float t0 = 25.0f;
                    lastAvgTemp = bmeAvg.avgTemperature;
                    lastAvgHumidity = bmeAvg.avgHumidity;
                    lastAvgPressure = bmeAvg.avgPressure;
                    lastAvgHeatIndex = bmeAvg.avgHeatIndex;
                    lastAvgDewPoint = bmeAvg.avgDewPoint;
                    lastAvgSoilRaw = soilAvg.avgRaw;
                    lastAvgSoilVoltage = soilAvg.avgVoltage;
                    lastAvgSoilPercent = soilAvg.avgPercent;
                    // Clamp avg soil percent to [0, 100]
                    if (lastAvgSoilPercent < 0) lastAvgSoilPercent = 0;
                    if (lastAvgSoilPercent > 100) lastAvgSoilPercent = 100;
                    lastAvgAir = airAvg;
                    lastAvgSoilCorrected = soilAvg.avgPercent - k * (bmeAvg.avgTemperature - t0);
                    // Clamp corrected soil percent to [0, 100]
                    if (lastAvgSoilCorrected < 0) lastAvgSoilCorrected = 0;
                    if (lastAvgSoilCorrected > 100) lastAvgSoilCorrected = 100;
                    // Print all results
                    Serial.println("[IrrigationManager] === FINAL IRRIGATION RESULTS ===");
                    Serial.printf("[IrrigationManager] Avg Temp: %.2f C\n", lastAvgTemp);
                    Serial.printf("[IrrigationManager] Avg Humidity: %.2f %%\n", lastAvgHumidity);
                    Serial.printf("[IrrigationManager] Avg Pressure: %.2f hPa\n", lastAvgPressure);
                    Serial.printf("[IrrigationManager] Avg Heat Index: %.2f C\n", lastAvgHeatIndex);
                    Serial.printf("[IrrigationManager] Avg Dew Point: %.2f C\n", lastAvgDewPoint);
                    Serial.printf("[IrrigationManager] Avg Soil Moisture Raw: %.1f\n", lastAvgSoilRaw);
                    Serial.printf("[IrrigationManager] Avg Soil Moisture Voltage: %.4f V\n", lastAvgSoilVoltage);
                    Serial.printf("[IrrigationManager] Avg Soil Moisture: %.2f %%\n", lastAvgSoilPercent);
                    Serial.printf("[IrrigationManager] Corrected Soil Moisture: %.2f %%\n", lastAvgSoilCorrected);
                    Serial.printf("[IrrigationManager] Avg Air Quality (V): %.4f V\n", lastAvgAir);
                    Serial.printf("[IrrigationManager] Results timestamp: %s\n", timeStr);
                    // Watering logic
                    wateringActive = false;
                    if (configManager) {
                        wateringThreshold = static_cast<float>(configManager->getInt("watering_threshold", 50));
                        wateringDuration = configManager->getInt("watering_duration_sec", 60); // Set default to 60 seconds (1 minute)
                    } else {
                        wateringThreshold = 50.0f;
                        wateringDuration = 60;
                    }
                    if (lastAvgSoilCorrected <= wateringThreshold) {
                        Serial.printf("[IrrigationManager] Soil moisture (%.2f%%) is below threshold (%.2f%%). Starting watering for %d seconds.\n", lastAvgSoilCorrected, wateringThreshold, wateringDuration);
                        if (relayController) relayController->setRelayMode(1, Relay::ON); // Relay 1 ON
                        wateringActive = true;
                        wateringStart = millis();
                        startNextState(COMPLETE);
                        break;
                    }
                    startNextState(COMPLETE);
                }
            }
            break;
        case COMPLETE:
            if (wateringActive) {
                unsigned long now = millis();
                int elapsed = (now - wateringStart) / 1000;
                if (now - lastProgressPrint >= 1000) {
                    Serial.printf("[IrrigationManager] Watering... %d/%d seconds\n", elapsed, wateringDuration);
                    lastProgressPrint = now;
                }
                if (elapsed >= wateringDuration) {
                    if (relayController) relayController->setRelayMode(1, Relay::OFF); // Relay 1 OFF
                    Serial.println("[IrrigationManager] Watering complete. Relay 1 OFF.");
                    wateringActive = false;
                }
            }
            if (!completePrinted && !wateringActive) {
                Serial.println("[IrrigationManager] Irrigation reading sequence complete.");
                updateDashboardSensorValues(); // Update dashboard with latest sensor readings
                if (timeManager) {
                    lastRunTimestamp = timeManager->getLocalTime().unixtime();
                } else {
                    lastRunTimestamp = time(nullptr);
                }
                completePrinted = true;
            }
            break;
        case WATER_NOW:
            if (wateringActive) {
                unsigned long now = millis();
                int elapsed = (now - wateringStart) / 1000;
                if (now - lastProgressPrint >= 1000) {
                    Serial.printf("[IrrigationManager] Water Now... %d/%d seconds\n", elapsed, wateringDuration);
                    lastProgressPrint = now;
                }
                if (elapsed >= wateringDuration) {
                    if (relayController) relayController->setRelayMode(1, Relay::OFF); // Relay 1 OFF
                    Serial.println("[IrrigationManager] Water Now complete. Relay 1 OFF.");
                    wateringActive = false;
                    state = COMPLETE;
                }
            }
            if (!completePrinted && !wateringActive) {
                Serial.println("[IrrigationManager] Water Now sequence complete.");
                updateDashboardSensorValues(); // Update dashboard with latest sensor readings
                if (timeManager) {
                    lastRunTimestamp = timeManager->getLocalTime().unixtime();
                } else {
                    lastRunTimestamp = time(nullptr);
                }
                completePrinted = true;
            }
            break;
    }
}

void IrrigationManager::checkAndRunScheduled() {
    if (!configManager || !timeManager) return;
    int schedHour = configManager->getInt("irrigation_scheduled_hour", 13);
    int schedMin = configManager->getInt("irrigation_scheduled_minute", 15);
    time_t now = timeManager->getLocalTime().unixtime();
    struct tm* tm_info = localtime(&now);
    static int lastRunDay = -1;
    if (tm_info->tm_hour == schedHour && tm_info->tm_min == schedMin && lastRunDay != tm_info->tm_yday) {
        Serial.println("[IrrigationManager] Scheduled time reached, triggering irrigation.");
        trigger();
        lastRunDay = tm_info->tm_yday;
    }
}

void IrrigationManager::startNextState(State next) {
    state = next;
    stateStart = millis();
    if (next != COMPLETE) completePrinted = false;
}

bool IrrigationManager::isRunning() const {
    return state != IDLE && state != COMPLETE;
}

bool IrrigationManager::isComplete() const {
    return state == COMPLETE;
}

void IrrigationManager::reset() {
    state = IDLE;
    completePrinted = false;
}

const char* IrrigationManager::getStateString() const {
    switch (state) {
        case IDLE: return "idle";
        case START: return "starting";
        case BME_READING: return "reading_environment";
        case SOIL_READING: return "reading_soil";
        case MQ135_READING: return "reading_air_quality";
        case WATER_NOW: return "watering_manual";
        case COMPLETE: return wateringActive ? "watering_scheduled" : "complete";
        default: return "unknown";
    }
}

void IrrigationManager::addStatusToJson(cJSON* parent) const {
    if (!parent) return;
    
    cJSON* irrigationJson = cJSON_CreateObject();
    
    // Current state
    cJSON_AddStringToObject(irrigationJson, "state", getStateString());
    cJSON_AddBoolToObject(irrigationJson, "running", isRunning());
    cJSON_AddBoolToObject(irrigationJson, "watering_active", wateringActive);
    
    // Scheduled time info
    if (configManager) {
        int schedHour = configManager->getInt("irrigation_scheduled_hour", 13);
        int schedMin = configManager->getInt("irrigation_scheduled_minute", 15);
        cJSON_AddNumberToObject(irrigationJson, "scheduled_hour", schedHour);
        cJSON_AddNumberToObject(irrigationJson, "scheduled_minute", schedMin);
    }
    
    // Watering info
    if (wateringActive) {
        unsigned long elapsed = (millis() - wateringStart) / 1000;
        cJSON_AddNumberToObject(irrigationJson, "watering_elapsed_sec", elapsed);
        cJSON_AddNumberToObject(irrigationJson, "watering_duration_sec", wateringDuration);
    }
    
    // Last readings (if available)
    if (lastReadingTimestamp > 0) {
        cJSON* lastReadings = cJSON_CreateObject();
        cJSON_AddNumberToObject(lastReadings, "temperature", lastAvgTemp);
        cJSON_AddNumberToObject(lastReadings, "humidity", lastAvgHumidity);
        cJSON_AddNumberToObject(lastReadings, "pressure", lastAvgPressure);
        cJSON_AddNumberToObject(lastReadings, "heat_index", lastAvgHeatIndex);
        cJSON_AddNumberToObject(lastReadings, "dew_point", lastAvgDewPoint);
        cJSON_AddNumberToObject(lastReadings, "soil_raw", lastAvgSoilRaw);
        cJSON_AddNumberToObject(lastReadings, "soil_voltage", lastAvgSoilVoltage);
        cJSON_AddNumberToObject(lastReadings, "soil_percent", lastAvgSoilPercent);
        cJSON_AddNumberToObject(lastReadings, "soil_corrected", lastAvgSoilCorrected);
        cJSON_AddNumberToObject(lastReadings, "air_quality_voltage", lastAvgAir);
        // Also expose corrected soil moisture at the top level for dashboard convenience
        cJSON_AddNumberToObject(irrigationJson, "soil_corrected", lastAvgSoilCorrected);
        // Format timestamp
        char timeStr[32];
        struct tm* tm_info = localtime(&lastReadingTimestamp);
        strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", tm_info);
        cJSON_AddStringToObject(lastReadings, "timestamp", timeStr);
        cJSON_AddItemToObject(irrigationJson, "last_readings", lastReadings);
    } else {
        cJSON_AddStringToObject(irrigationJson, "last_readings", "none");
    }
    
    // Last run timestamp (when irrigation sequence was last completed)
    if (lastRunTimestamp > 0) {
        char runTimeStr[32];
        struct tm* run_tm_info = localtime(&lastRunTimestamp);
        strftime(runTimeStr, sizeof(runTimeStr), "%Y-%m-%d %H:%M:%S", run_tm_info);
        cJSON_AddStringToObject(irrigationJson, "last_run_time", runTimeStr);
    } else {
        cJSON_AddStringToObject(irrigationJson, "last_run_time", "never");
    }
    
    // Watering threshold info
    if (configManager) {
        float threshold = static_cast<float>(configManager->getInt("watering_threshold", 50));
        cJSON_AddNumberToObject(irrigationJson, "watering_threshold", threshold);
    }
    
    cJSON_AddItemToObject(parent, "irrigation", irrigationJson);
}

void IrrigationManager::updateDashboardSensorValues() {
    if (!dashboardManager) return;
    
    // Update the dashboard's sensor devices with our latest readings
    // This ensures the dashboard shows the most recent data from our irrigation cycle
    
    // The dashboard already has references to the actual sensor objects,
    // so we don't need to push specific values - the sensors themselves
    // contain the latest readings that the dashboard will use.
    
    // However, we can trigger the sensors to update their internal state
    // if they have methods to do so, or we could add a method to force
    // the dashboard to refresh its cached values.
    
    // For now, the dashboard will automatically pick up the latest readings
    // from the sensor objects when getStatusJson() is called, since we're
    // using the same sensor instances.
    
    Serial.println("[IrrigationManager] Updated dashboard with latest sensor values");
}
