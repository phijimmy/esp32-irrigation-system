#include <Arduino.h>
#include "system/SystemManager.h"
#include "devices/LedDevice.h"
#include "devices/TouchSensorDevice.h"
#include "devices/RelayController.h"
#include "devices/SoilMoistureSensor.h"
#include "devices/MQ135Sensor.h"
#include "devices/IrrigationManager.h"
#include "system/DashboardManager.h"

#include "system/WebServerManager.h"

SystemManager systemManager;
LedDevice led;
TouchSensorDevice touch;
RelayController relayController;
SoilMoistureSensor soilMoistureSensor;
MQ135Sensor mq135Sensor;
bool soilReadingTaken = false;
bool soilReadingRequested = false;
bool mq135ReadingTaken = false;
bool mq135ReadingRequested = false;
unsigned long lastStabilisationPrint = 0;
unsigned long mq135LastProgressPrint = 0;

// Sensor state machine
enum SensorState { IDLE, SOIL_STABILISING, SOIL_DONE, MQ135_WARMUP, MQ135_DONE };
SensorState sensorState = IDLE;
// Sequential sensor initialization state 
enum InitState { INIT_IDLE, INIT_SOIL_STABILISING, INIT_SOIL_DONE, INIT_MQ135_WARMUP, INIT_MQ135_DONE, INIT_COMPLETE };
InitState initState = INIT_IDLE;
bool sensorsInitialized = false;
// Hourly reading state
bool hourlyReadingRequested = false;
bool deferredHourlyReading = false;
IrrigationManager irrigationManager;
WebServerManager* webServerManager = nullptr;
bool webServerStarted = false;
unsigned long irrigationTriggerTime = 0;
bool irrigationTriggerScheduled = false;
bool irrigationTriggered = false;

DashboardManager* dashboard = nullptr;



void setup() {
    systemManager.begin();
    // Set soil power GPIO LOW at boot (redundant safety)
    int soilPowerGpio = systemManager.getConfigManager().getInt("soil_power_gpio", 16);
    if (soilPowerGpio >= 0) {
        pinMode(soilPowerGpio, OUTPUT);
        digitalWrite(soilPowerGpio, LOW);
    }
    soilMoistureSensor.begin(&systemManager.getADS1115Manager(), &systemManager.getConfigManager(), &systemManager.getTimeManager());
    mq135Sensor.begin(&systemManager.getADS1115Manager(), &systemManager.getConfigManager(), &relayController);
    mq135Sensor.setTimeManager(&systemManager.getTimeManager());
    soilReadingTaken = false;
    soilReadingRequested = false;
    mq135ReadingTaken = false;
    mq135ReadingRequested = false;
    lastStabilisationPrint = 0;
    mq135LastProgressPrint = 0;
    sensorState = IDLE;
    irrigationManager.begin(systemManager.getDeviceManager().getBME280Device(), &soilMoistureSensor, &mq135Sensor, &systemManager.getTimeManager());
    irrigationManager.setConfigManager(&systemManager.getConfigManager());
    irrigationManager.setRelayController(&relayController);
    irrigationTriggered = false;
    irrigationTriggerTime = 0;
    // Weekly schedule alarms are already set during TimeManager initialization
    
    // Get LED config from proper config sections
    cJSON* root = systemManager.getConfigManager().getRoot();
    int ledGpio = cJSON_GetObjectItem(root, "led_gpio") ? cJSON_GetObjectItem(root, "led_gpio")->valueint : 23;
    int ledBlinkRate = cJSON_GetObjectItem(root, "led_blink_rate") ? cJSON_GetObjectItem(root, "led_blink_rate")->valueint : 500;
    
    led.setGpio(ledGpio);
    led.setBlinkRate(ledBlinkRate);
    led.setConfigManager(&systemManager.getConfigManager());
    led.setDiagnosticManager(&systemManager.getDiagnosticManager());
    systemManager.getDeviceManager().addDevice(&led);
    led.begin(); // Explicitly call begin() since DeviceManager.begin() was called before adding this device
    // led.setMode(LedDevice::BLINK); // Set LED ON at boot

    // Get touch config from proper config sections
    // Patch: ensure touch_threshold and touch_long_press are numbers in config
    cJSON* thresholdItem = cJSON_GetObjectItem(root, "touch_threshold");
    if (thresholdItem && cJSON_IsString(thresholdItem)) {
        int thresholdVal = atoi(thresholdItem->valuestring);
        cJSON_ReplaceItemInObject(root, "touch_threshold", cJSON_CreateNumber(thresholdVal));
    }
    cJSON* longPressItem = cJSON_GetObjectItem(root, "touch_long_press");
    if (longPressItem && cJSON_IsString(longPressItem)) {
        int longPressVal = atoi(longPressItem->valuestring);
        cJSON_ReplaceItemInObject(root, "touch_long_press", cJSON_CreateNumber(longPressVal));
    }
    touch.setConfigManager(&systemManager.getConfigManager());
    touch.setDiagnosticManager(&systemManager.getDiagnosticManager());
    systemManager.getDeviceManager().addDevice(&touch);
    touch.begin(); // Explicitly call begin() since DeviceManager.begin() was called before adding this device

    relayController.setConfigManager(&systemManager.getConfigManager());
    relayController.setDiagnosticManager(&systemManager.getDiagnosticManager());
    systemManager.getDeviceManager().addDevice(&relayController);
    relayController.begin(); // Explicitly call begin() since DeviceManager.begin() was called before adding this device
    
    // Connect TimeManager to RelayController for INT/SQW relay control
    systemManager.getTimeManager().setRelayController(&relayController);

    // Now that relays are initialized, initialize MQ135 (but don't start it yet)
    mq135Sensor.begin(&systemManager.getADS1115Manager(), &systemManager.getConfigManager(), &relayController);
    mq135Sensor.setTimeManager(&systemManager.getTimeManager());
    // Note: MQ135 reading will start after soil sensor initialization is complete (sequential)
    
    // Print initial BME280 reading if available
    // BME280Device* bme = systemManager.getDeviceManager().getBME280Device();
    // if (bme) {
    //     BME280Reading r = bme->readData();
    //     if (r.valid) {
    //         char timeStr[32] = "";
    //         if (r.timestamp.isValid()) {
    //             snprintf(timeStr, sizeof(timeStr), "%04d-%02d-%02d %02d:%02d:%02d", r.timestamp.year(), r.timestamp.month(), r.timestamp.day(), r.timestamp.hour(), r.timestamp.minute(), r.timestamp.second());
    //         } else {
    //             strcpy(timeStr, "N/A");
    //         }
    //         Serial.printf("[BME280] Initial reading: T=%.2fC, H=%.2%%, P=%.2fhPa, HI=%.2fC, DP=%.2fC | avgT=%.2fC, avgH=%.2%%, avgP=%.2fhPa, avgHI=%.2fC, avgDP=%.2fC, time=%s\n",
    //             r.temperature, r.humidity, r.pressure, r.heatIndex, r.dewPoint,
    //             r.avgTemperature, r.avgHumidity, r.avgPressure, r.avgHeatIndex, r.avgDewPoint, timeStr);
    //     } else {
    //         Serial.println("[BME280] Initial reading: not valid");
    //     }
    // }
    
    // Now create and initialize DashboardManager LAST
    // dashboard.setSoilMoistureSensor(&soilMoistureSensor);
    // dashboard.setMQ135Sensor(&mq135Sensor);
    // dashboard.setIrrigationManager(&irrigationManager);
    // irrigationManager.setDashboardManager(&dashboard); // Connect irrigation manager to dashboard
    // dashboard.begin();
    // Serial.println("[DashboardManager] JSON status:");
    // Serial.println(dashboard.getStatusString());
    // REMOVE webServerManager = new WebServerManager(&dashboard, &systemManager.getDiagnosticManager());
    // REMOVE webServerManager->begin();
}

void loop() {
    systemManager.update();
    // --- WebServer non-blocking initialization ---
    if (!webServerStarted && dashboard && dashboard->hasValidSensorData()) {
        webServerManager = new WebServerManager(dashboard, &systemManager.getDiagnosticManager());
        webServerManager->begin();
        webServerStarted = true;
        Serial.println("[WebServerManager] Started after valid sensor data detected.");
        // config.json is no longer deleted automatically here; deletion is now on-demand only
    }
    // --- Sequential sensor initialization (non-blocking) ---
    if (!sensorsInitialized) {
        switch (initState) {
            case INIT_IDLE:
                // Start soil sensor stabilization first
                soilMoistureSensor.beginStabilisation();
                Serial.println("[INIT] Starting soil sensor stabilisation...");
                initState = INIT_SOIL_STABILISING;
                lastStabilisationPrint = millis();
                break;
                
            case INIT_SOIL_STABILISING: {
                unsigned long now = millis();
                int elapsed = (now - soilMoistureSensor.getStabilisationStart()) / 1000;
                int total = soilMoistureSensor.getStabilisationTimeSec();
                if (now - lastStabilisationPrint >= 1000) {
                    Serial.printf("[INIT][SoilMoistureSensor] Stabilising... %d/%d seconds\n", elapsed, total);
                    lastStabilisationPrint = now;
                }
                if (soilMoistureSensor.readyForReading()) {
                    Serial.println("[INIT] Soil sensor stabilisation complete, taking reading...");
                    soilMoistureSensor.takeReading();
                    soilMoistureSensor.printReading();
                    initState = INIT_SOIL_DONE;
                }
                break;
            }
            
            case INIT_SOIL_DONE:
                // Soil is done, now start MQ135 warmup (sequential to avoid ADS1115 conflicts)
                Serial.println("[INIT] Soil sensor done, starting MQ135 warmup...");
                mq135Sensor.startReading(); // Start MQ135 warmup
                initState = INIT_MQ135_WARMUP;
                mq135LastProgressPrint = millis();
                break;
                
            case INIT_MQ135_WARMUP: {
                unsigned long now = millis();
                int elapsed = (now - mq135Sensor.getWarmupStart()) / 1000;
                int total = mq135Sensor.isWarmingUp() ? mq135Sensor.getWarmupTimeSec() : 0;
                if (mq135Sensor.isWarmingUp() && now - mq135LastProgressPrint >= 1000) {
                    Serial.printf("[INIT][MQ135Sensor] Warming up... %d/%d seconds\n", elapsed, total);
                    mq135LastProgressPrint = now;
                }
                if (mq135Sensor.readyForReading()) {
                    Serial.println("[INIT] MQ135 warmup complete, taking reading...");
                    mq135Sensor.takeReading();
                    const MQ135Sensor::Reading& r = mq135Sensor.getLastReading();
                    char timeStr[32];
                    struct tm* tm_info = localtime(&r.timestamp);
                    strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", tm_info);
                    Serial.printf("[INIT][MQ135Sensor] Reading: raw=%d, voltage=%.4f V | avgRaw=%.1f, avgVoltage=%.4f V, AQI=%s, timestamp=%s\n",
                        r.raw, r.voltage, r.avgRaw, r.avgVoltage, MQ135Sensor::getAirQualityLabel(r.avgVoltage), timeStr);
                    initState = INIT_MQ135_DONE;
                }
                break;
            }
            
            case INIT_MQ135_DONE:
                Serial.println("[INIT] All sensors initialized!");
                sensorsInitialized = true;
                initState = INIT_COMPLETE;
                // Now create and initialize DashboardManager
                dashboard = new DashboardManager(
                    &systemManager.getTimeManager(),
                    &systemManager.getConfigManager(),
                    &systemManager,
                    &systemManager.getDiagnosticManager(),
                    &led,
                    &relayController,
                    &touch,
                    systemManager.getDeviceManager().getBME280Device()
                );
                dashboard->setSoilMoistureSensor(&soilMoistureSensor);
                dashboard->setMQ135Sensor(&mq135Sensor);
                dashboard->setIrrigationManager(&irrigationManager);
                irrigationManager.setDashboardManager(dashboard);
                dashboard->begin();
                Serial.println("[DashboardManager] JSON status:");
                Serial.println(dashboard->getStatusString());
                break;
                
            case INIT_COMPLETE:
                // Sensors are ready
                break;
        }
        // Don't continue with normal operations until initialization is complete
        return;
    }
    
    irrigationManager.update();
    irrigationManager.checkAndRunScheduled();
    // --- BME280 and SoilMoistureSensor hourly reading logic ---
    static int lastBmeHour = -1;
    static int lastSoilHour = -1;
    BME280Device* bme = systemManager.getDeviceManager().getBME280Device();
    TimeManager& timeMgr = systemManager.getTimeManager();
    DateTime now = timeMgr.getTime();
    if (bme && now.isValid() && now.minute() == 0 && now.second() == 0 && now.hour() != lastBmeHour) {
        BME280Reading r = bme->readData();
        lastBmeHour = now.hour();
        if (r.valid) {
            char timeStr[32] = "";
            if (r.timestamp.isValid()) {
                snprintf(timeStr, sizeof(timeStr), "%04d-%02d-%02d %02d:%02d:%02d", r.timestamp.year(), r.timestamp.month(), r.timestamp.day(), r.timestamp.hour(), r.timestamp.minute(), r.timestamp.second());
            } else {
                strcpy(timeStr, "N/A");
            }
            Serial.printf("[BME280] Hourly reading: T=%.2fC, H=%.2f%%, P=%.2fhPa, HI=%.2fC, DP=%.2fC | avgT=%.2fC, avgH=%.2f%%, avgP=%.2fhPa, avgHI=%.2fC, avgDP=%.2fC, time=%s\n",
                r.temperature, r.humidity, r.pressure, r.heatIndex, r.dewPoint,
                r.avgTemperature, r.avgHumidity, r.avgPressure, r.avgHeatIndex, r.avgDewPoint, timeStr);
        } else {
            Serial.println("[BME280] Hourly reading: not valid");
        }
        // After BME280, start soil moisture reading (non-blocking)
        if (now.hour() != lastSoilHour) {
            // If a reading is already in progress, defer the hourly reading
            if (sensorState != IDLE || soilReadingRequested || mq135ReadingRequested) {
                deferredHourlyReading = true;
                Serial.printf("[SoilMoistureSensor] Hourly: Reading in progress, deferring hourly reading. State: %d, soilReadingRequested: %d, mq135ReadingRequested: %d\n", sensorState, soilReadingRequested, mq135ReadingRequested);
            } else {
                soilMoistureSensor.beginStabilisation();
                Serial.printf("[SoilMoistureSensor] Hourly: Starting non-blocking stabilisation (GPIO %d)...\n", soilMoistureSensor.getPowerGpio());
                Serial.printf("[SoilMoistureSensor] Hourly: Stabilisation time: %d seconds...\n", soilMoistureSensor.getStabilisationTimeSec());
                // Set flags to handle reading via state machine
                hourlyReadingRequested = true;
                soilReadingRequested = true;
                sensorState = SOIL_STABILISING;
                lastSoilHour = now.hour();
            }
        }
    }
    
    // --- Handle hourly soil reading completion (non-blocking) ---
    if (hourlyReadingRequested && soilMoistureSensor.readyForReading()) {
        Serial.println("[SoilMoistureSensor] Hourly stabilisation complete, taking reading...");
        soilMoistureSensor.takeReading();
        // Ensure power GPIO is set LOW after reading
        digitalWrite(soilMoistureSensor.getPowerGpio(), LOW);
        Serial.println("[SoilMoistureSensor] Hourly reading after stabilisation:");
        soilMoistureSensor.printReading();
        hourlyReadingRequested = false;
        // After soil reading, trigger MQ135 hourly reading if needed
        if (deferredHourlyReading) {
            deferredHourlyReading = false;
            Serial.println("[SoilMoistureSensor] Triggering deferred hourly reading now.");
            soilMoistureSensor.beginStabilisation();
            hourlyReadingRequested = true;
            lastSoilHour = now.hour();
        }
    }
    
    // If a deferred hourly reading is pending and the system is now idle, trigger it
    if (deferredHourlyReading && sensorState == IDLE && !soilReadingRequested && !mq135ReadingRequested) {
        deferredHourlyReading = false;
        Serial.println("[SoilMoistureSensor] System idle, running deferred hourly reading now.");
        soilMoistureSensor.beginStabilisation();
        hourlyReadingRequested = true;
        lastSoilHour = now.hour();
    }

    switch (sensorState) {
        case IDLE:
            // If a manual MQ135 reading was requested, start warmup
            if (mq135ReadingRequested) {
                Serial.println("[MQ135Sensor] Manual air quality reading requested, starting warmup...");
                mq135Sensor.startReading();
                mq135ReadingRequested = false;
                mq135ReadingTaken = false;
                mq135LastProgressPrint = millis();
                sensorState = MQ135_WARMUP;
            }
            // ...existing code...
            break;
        case SOIL_STABILISING: {
            unsigned long now = millis();
            int elapsed = (now - soilMoistureSensor.getStabilisationStart()) / 1000;
            int total = soilMoistureSensor.getStabilisationTimeSec();
            if (now - lastStabilisationPrint >= 1000) {
                Serial.printf("[SoilMoistureSensor] Stabilising... %d/%d seconds\n", elapsed, total);
                lastStabilisationPrint = now;
            }
            if (soilMoistureSensor.readyForReading()) {
                Serial.println("[SoilMoistureSensor] Stabilisation complete, taking reading...");
                soilMoistureSensor.takeReading();
                soilMoistureSensor.printReading();
                soilReadingTaken = true;
                // Reset flags to allow next reading
                soilReadingRequested = false;
                sensorState = SOIL_DONE;
            }
            break;
        }
        case SOIL_DONE:
            // Only trigger MQ135 if this was part of the initialisation or hourly reading, not from manual soil reading
            if (soilReadingRequested) {
                Serial.println("[MQ135Sensor] Starting air quality sensor warmup...");
                mq135Sensor.startReading();
                mq135ReadingRequested = true;
                mq135ReadingTaken = false;
                mq135LastProgressPrint = millis();
                // Reset soil reading flags before starting MQ135
                soilReadingRequested = false;
                sensorState = MQ135_WARMUP;
            } else {
                // Manual soil reading: return to idle
                // Ensure all flags are reset
                soilReadingRequested = false;
                hourlyReadingRequested = false;
                sensorState = IDLE;
            }
            break;
        case MQ135_WARMUP: {
            unsigned long now = millis();
            int elapsed = (now - mq135Sensor.getWarmupStart()) / 1000;
            int total = mq135Sensor.isWarmingUp() ? mq135Sensor.getWarmupTimeSec() : 0;
            if (mq135Sensor.isWarmingUp() && now - mq135LastProgressPrint >= 1000) {
                Serial.printf("[MQ135Sensor] Warming up... %d/%d seconds\n", elapsed, total);
                mq135LastProgressPrint = now;
            }
            if (mq135Sensor.readyForReading()) {
                Serial.println("[MQ135Sensor] Warmup complete, taking air quality reading...");
                mq135Sensor.takeReading();
                const MQ135Sensor::Reading& r = mq135Sensor.getLastReading();
                char timeStr[32];
                struct tm* tm_info = localtime(&r.timestamp);
                strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", tm_info);
                Serial.printf("[MQ135Sensor] DEBUG: raw ADC=%d, expected for 0.4V=~2133 (GAIN_TWOTHIRDS)\n", r.raw);
                Serial.printf("[MQ135Sensor] Reading: raw=%d, voltage=%.4f V | avgRaw=%.1f, avgVoltage=%.4f V, AQI=%s, timestamp=%s\n",
                    r.raw, r.voltage, r.avgRaw, r.avgVoltage, MQ135Sensor::getAirQualityLabel(r.avgVoltage), timeStr);
                mq135ReadingTaken = true;
                sensorState = MQ135_DONE;
            }
            break;
        }
        case MQ135_DONE:
            // Only schedule irrigation trigger once after initial MQ135_DONE
            // if (!irrigationTriggerScheduled && !irrigationTriggered) {
            //     irrigationTriggerTime = millis();
            //     irrigationTriggerScheduled = true;
            // }
            // After 30s, trigger irrigationManager ONCE
            // if (irrigationTriggerScheduled && !irrigationTriggered && millis() - irrigationTriggerTime >= 30000) {
            //     Serial.println("[IrrigationManager] Triggering irrigation readings after 30s delay.");
            //     irrigationManager.trigger();
            //     irrigationTriggered = true;
            // }
            break;
    }
    // Poll INT/SQW GPIO for hardware interrupt detection
    if (systemManager.getTimeManager().pollInterruptPin()) {
        systemManager.getTimeManager().handleAlarmInterrupt();
    } else {
        systemManager.getTimeManager().handleAlarmInterrupt();
    }
    systemManager.getTimeManager().updateRelayFromIntSqw();
    // --- Touch sensor actions ---
    // Only bring up AP if in AP mode, AP is not active, and touch is pressed (short press)
    NetworkManager& netMgr = systemManager.getNetworkManager();
    ConfigManager& cfgMgr = systemManager.getConfigManager();
    const char* wifiMode = cfgMgr.get("wifi_mode");
    if (touch.isTouched() && !netMgr.isAPActive() && wifiMode && strcmp(wifiMode, "ap") == 0) {
        netMgr.startAP();
    }
    // Long press: reboot system
    if (touch.isLongPressed()) {
        Serial.println("[TouchSensor] Long press detected, rebooting system...");
        delay(1000); // Give user feedback
        ESP.restart();
    }
    delay(10);
}
