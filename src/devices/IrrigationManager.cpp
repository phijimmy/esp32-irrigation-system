/*
 * IrrigationManager.cpp - Recent changes summary (2025-07-16)
 *
 * - Added forceIdle() calls for all sensors in stopNow() to ensure backend and frontend state are always synchronized when irrigation is stopped.
 * - Manual watering (WATER_NOW) now completes without triggering an MQ135 air quality reading, matching user requirements.
 * - Cleaned up state transitions and ensured backend state/JSON status is robust and accurate.
 * - All JSON operations use cJSON as required by workspace policy.
 *
 * If you change state machine logic or backend/frontend sync, update this comment for maintainers.
 */

#include "devices/IrrigationManager.h"
#include "system/MqttManager.h"
#include "devices/RelayController.h"
#include "devices/Relay.h"
#include "devices/MQ135Sensor.h"
#include <Arduino.h>
#include <time.h>


IrrigationManager::IrrigationManager()
    : mq135Sensor(nullptr), lastAirQualityVoltage(-1.0f)
{}
// Setter for MQ135Sensor
void IrrigationManager::setMQ135Sensor(MQ135Sensor* sensor) {
    mq135Sensor = sensor;
}
// Immediately stop all irrigation activity, deactivate relay 1, and set state to IDLE
void IrrigationManager::stopNow() {
    Serial.println("[IrrigationManager] STOP: Immediately stopping all irrigation activity and deactivating relay 1.");
    if (relayController) relayController->setRelayMode(1, Relay::OFF); // Relay 1 OFF
    // Relay state will be published by RelayController
    wateringActive = false;
    // Reset/cancel any sensor activity here
    if (soilSensor) soilSensor->forceIdle();
    if (mq135Sensor) mq135Sensor->forceIdle();
    if (bme280) bme280->forceIdle();
    state = IDLE;
    completePrinted = false;
}
// ...existing code...

void IrrigationManager::begin(BME280Device* bme, SoilMoistureSensor* soil, TimeManager* timeMgr) {
    bme280 = bme;
    soilSensor = soil;
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
    static float wateringThreshold = 0;
    float k = 0.5f;
    float t0 = 25.0f;
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
        case SOIL_READING: {
            // Run watering logic here using available data
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
            lastAvgSoilCorrected = soilAvg.avgPercent - k * (bmeAvg.avgTemperature - t0);
            if (lastAvgSoilCorrected < 0) lastAvgSoilCorrected = 0;
            if (lastAvgSoilCorrected > 100) lastAvgSoilCorrected = 100;
            // Set lastReadingTimestamp for dashboard/status JSON
            if (timeManager) {
                lastReadingTimestamp = timeManager->getLocalTime().unixtime();
            } else {
                lastReadingTimestamp = time(nullptr);
            }
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
            // Watering logic with Sunday check
            bool skipWatering = false;
            if (configManager && timeManager) {
                time_t now = timeManager->getLocalTime().unixtime();
                struct tm* tm_info = localtime(&now);
                if (tm_info->tm_wday == 0) { // Sunday is 0
                    cJSON* root = configManager->getRoot();
                    bool sundayWatering = false;
                    if (root) {
                        cJSON* swItem = cJSON_GetObjectItemCaseSensitive(root, "sunday_watering");
                        if (cJSON_IsBool(swItem)) {
                            sundayWatering = cJSON_IsTrue(swItem);
                        } else if (cJSON_IsString(swItem) && swItem->valuestring) {
                            sundayWatering = (strcmp(swItem->valuestring, "true") == 0 || strcmp(swItem->valuestring, "1") == 0);
                        }
                    }
                    if (!sundayWatering) skipWatering = true;
                }
            }
            if (configManager) {
                wateringThreshold = static_cast<float>(configManager->getInt("watering_threshold", 50));
                wateringDuration = configManager->getInt("watering_duration_sec", 60);
            } else {
                wateringThreshold = 50.0f;
                wateringDuration = 60;
            }
            if (!skipWatering && lastAvgSoilCorrected <= wateringThreshold) {
                Serial.printf("[IrrigationManager] Soil moisture (%.2f%%) is below threshold (%.2f%%). Starting watering for %d seconds.\n", lastAvgSoilCorrected, wateringThreshold, wateringDuration);
                if (relayController) relayController->setRelayMode(1, Relay::ON);
                // Relay state will be published by RelayController
                wateringActive = true;
                wateringStart = millis();
                // Go to watering state, then after watering is done, go to air quality
                startNextState(COMPLETE);
            } else {
                if (skipWatering) {
                    Serial.println("[IrrigationManager] Sunday watering disabled, skipping watering.");
                }
                // No watering needed, go directly to air quality reading
                startNextState(MQ135_READING);
            }
        }
            break;
        case MQ135_READING: {
            // Show progress for MQ135 warmup and reading
            static bool started = false;
            static unsigned long lastMQ135ProgressPrint = 0;
            // Relay state will be published by RelayController
            if (mq135Sensor) {
                if (!started) {
                    Serial.println("[IrrigationManager] Starting MQ135 air quality sensor warmup...");
                    mq135Sensor->startReading();
                    started = true;
                    lastMQ135ProgressPrint = millis();
                }
                unsigned long now = millis();
                int elapsed = (now - mq135Sensor->getWarmupStart()) / 1000;
                int total = mq135Sensor->getWarmupTimeSec();
                if (!mq135Sensor->readyForReading()) {
                    if (now - lastMQ135ProgressPrint >= 1000) {
                        Serial.printf("[IrrigationManager][MQ135Sensor] Warming up... %d/%d seconds\n", elapsed, total);
                        lastMQ135ProgressPrint = now;
                    }
                } else {
                    Serial.println("[IrrigationManager] MQ135 warmup complete, taking air quality reading...");
                    mq135Sensor->takeReading();
                    lastAirQualityVoltage = mq135Sensor->getLastReading().avgVoltage;
                    Serial.printf("[IrrigationManager][MQ135] Air quality voltage: %.4f V\n", lastAirQualityVoltage);
                    extern MqttManager mqttManager;
                    if (mqttManager.isInitialized()) {
                        mqttManager.publishMQ135AirQuality();
                    }
                    started = false;
                    startNextState(COMPLETE);
                }
            } else {
                Serial.println("[IrrigationManager] MQ135 sensor not available, skipping air quality reading.");
                lastAirQualityVoltage = -1.0f;
                started = false;
                startNextState(COMPLETE);
            }
            break;
        }
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
                    // Relay state will be published by RelayController
                    Serial.println("[IrrigationManager] Watering complete. Relay 1 OFF.");
                    wateringActive = false;
                    // After watering is done, go to air quality reading
                    startNextState(MQ135_READING);
                }
            } else if (!completePrinted) {
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
                    // Relay state will be published by RelayController
                    Serial.println("[IrrigationManager] Water Now complete. Relay 1 OFF.");
                    wateringActive = false;
                    // After manual watering, finish without air quality reading
                    state = COMPLETE;
                    stateStart = millis();
                    break;
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
        bool doWatering = true;
        // Check for Sunday and sunday_watering config
        if (tm_info->tm_wday == 0) { // Sunday is 0
            // Get sunday_watering as bool, fallback to string if needed
            bool sundayWatering = false;
            cJSON* root = configManager->getRoot();
            if (root) {
                cJSON* swItem = cJSON_GetObjectItemCaseSensitive(root, "sunday_watering");
                if (cJSON_IsBool(swItem)) {
                    sundayWatering = cJSON_IsTrue(swItem);
                } else if (cJSON_IsString(swItem) && swItem->valuestring) {
                    sundayWatering = (strcmp(swItem->valuestring, "true") == 0 || strcmp(swItem->valuestring, "1") == 0);
                }
            }
            doWatering = sundayWatering;
        }
        Serial.println("[IrrigationManager] Scheduled time reached, triggering irrigation.");
        if (doWatering) {
            trigger(); // Normal: take readings and do watering
        } else {
            // Only take readings and air quality, skip watering
            Serial.println("[IrrigationManager] Sunday watering disabled, skipping watering but taking readings.");
            state = START; // Start state machine, but skip watering in SOIL_READING
            stateStart = millis();
            completePrinted = false;
        }
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
        case WATER_NOW: return "watering_manual";
        case COMPLETE: return wateringActive ? "watering_scheduled" : "complete";
        default: return "idle";
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
        // Removed air_quality_voltage from JSON
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
