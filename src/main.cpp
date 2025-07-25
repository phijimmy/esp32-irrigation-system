#include <Arduino.h>
#include "system/SystemManager.h"
#include "devices/LedDevice.h"
#include "devices/TouchSensorDevice.h"
#include "devices/RelayController.h"
#include "devices/SoilMoistureSensor.h"
#include "devices/MQ135Sensor.h"
#include "devices/IrrigationManager.h"
#include "system/DashboardManager.h"

#include "system/ReadingManager.h"

#include "system/WebServerManager.h"
// Add MQTT Manager include
#include "system/MqttManager.h"

SystemManager systemManager;
LedDevice led;
TouchSensorDevice touch;
RelayController relayController;
SoilMoistureSensor soilMoistureSensor;
MQ135Sensor mq135Sensor;
// Add global MqttManager instance
MqttManager mqttManager;
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
int originalMQ135Warmup = 0;
int originalSoilStab = 0;
// Hourly reading state
IrrigationManager irrigationManager;
WebServerManager* webServerManager = nullptr;
bool webServerStarted = false;
unsigned long irrigationTriggerTime = 0;
bool irrigationTriggerScheduled = false;
bool irrigationTriggered = false;

DashboardManager* dashboard = nullptr;



void setup() {

    // ...existing initialization code...
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
    irrigationManager.begin(systemManager.getDeviceManager().getBME280Device(), &soilMoistureSensor, &systemManager.getTimeManager());
    irrigationManager.setConfigManager(&systemManager.getConfigManager());
    irrigationManager.setRelayController(&relayController);
    irrigationManager.setMQ135Sensor(&mq135Sensor);
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
    relayController.setMqttManager(&mqttManager);
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

    // --- ReadingManager: instantiation moved to loop() after sensorsInitialized ---
}

void loop() {
    systemManager.update();
    static ReadingManager* readingManager = nullptr;
    static bool readingManagerInitialized = false;
    static bool mqttManagerInitialized = false;
    DateTime now = systemManager.getTimeManager().getTime();
    int year = now.year();

    // --- Sequential sensor initialization (non-blocking) ---
    if (!sensorsInitialized) {
        switch (initState) {
            case INIT_IDLE: {
                originalSoilStab = soilMoistureSensor.getStabilisationTimeSec();
                soilMoistureSensor.setStabilisationTimeSec(3);
                soilMoistureSensor.beginStabilisation();
                Serial.println("[INIT] Starting soil sensor stabilisation (override 3s)...");
                initState = INIT_SOIL_STABILISING;
                lastStabilisationPrint = millis();
                break;
            }
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
                    soilMoistureSensor.setStabilisationTimeSec(originalSoilStab);
                    initState = INIT_SOIL_DONE;
                }
                break;
            }
            case INIT_SOIL_DONE: {
                originalMQ135Warmup = mq135Sensor.getWarmupTimeSec();
                mq135Sensor.setWarmupTimeSec(5);
                Serial.println("[INIT] Soil sensor done, starting MQ135 warmup (override 5s)...");
                mq135Sensor.startReading();
                initState = INIT_MQ135_WARMUP;
                mq135LastProgressPrint = millis();
                break;
            }
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
            case INIT_MQ135_DONE: {
                mq135Sensor.setWarmupTimeSec(originalMQ135Warmup);
                Serial.println("[INIT] All sensors initialized!");
                sensorsInitialized = true;
                initState = INIT_COMPLETE;
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

                // --- MqttManager initialization: moved here to guarantee execution ---
                const char* mqttEnabledStr = systemManager.getConfigManager().get("mqtt_enabled");
                bool mqttEnabled = mqttEnabledStr && (strcmp(mqttEnabledStr, "true") == 0 || strcmp(mqttEnabledStr, "1") == 0);
                if (mqttEnabled) {
                    const char* deviceNameCStr = systemManager.getConfigManager().get("device_name");
                    std::string deviceName = deviceNameCStr ? deviceNameCStr : "esp32_device";
                    std::vector<std::string> relayNames;
                    cJSON* relayNamesArr = cJSON_GetObjectItemCaseSensitive(systemManager.getConfigManager().getRoot(), "relay_names");
                    if (relayNamesArr && cJSON_IsArray(relayNamesArr)) {
                        int arrSize = cJSON_GetArraySize(relayNamesArr);
                        for (int i = 0; i < arrSize; ++i) {
                            cJSON* item = cJSON_GetArrayItem(relayNamesArr, i);
                            if (cJSON_IsString(item)) relayNames.push_back(item->valuestring);
                        }
                    } else {
                        relayNames = {"Zone 1", "Zone 2", "Zone 3", "Zone 4"};
                    }
                    mqttManager.begin(deviceName, relayNames, &systemManager.getConfigManager());
                    mqttManager.setInitialized(true);
                    // Publish initial BME280 temperature to Home Assistant
                    BME280Device* bme = systemManager.getDeviceManager().getBME280Device();
                    if (bme) {
                        BME280Reading r = bme->getLastReading();
                        if (r.valid) {
                            mqttManager.publishBME280Temperature(r.avgTemperature);
                            Serial.printf("[MqttManager] Initial BME280 temperature published to Home Assistant: %.2fC\n", r.avgTemperature);
                        }
                    }
                    // Set flag so loop MQTT logic runs
                    mqttManagerInitialized = true;
                    Serial.println("[MqttManager] Home Assistant discovery published for relays.");
                }
                break;
            }
            case INIT_COMPLETE:
                break;
        }
        return;
    }

    // --- WebServerManager initialization ---
    if (!webServerStarted && dashboard && dashboard->hasValidSensorData()) {
        Serial.println("[DEBUG] Initializing WebServerManager...");
        webServerManager = new WebServerManager(dashboard, &systemManager.getDiagnosticManager());
        webServerManager->begin();
        webServerStarted = true;
        Serial.println("[WebServerManager] Started after valid sensor data detected.");
    }
    // --- ReadingManager initialization ---
    if (!readingManagerInitialized) {
        Serial.printf("[DEBUG] RM? sensorsInitialized=%d, webServerStarted=%d, dashboard=%p, dashboard->hasValidSensorData()=%d\n",
            sensorsInitialized, webServerStarted, dashboard, (dashboard ? dashboard->hasValidSensorData() : -1));
    }
    if (!readingManagerInitialized && sensorsInitialized && webServerStarted) {
        Serial.println("[DEBUG] Initializing ReadingManager...");
        readingManager = new ReadingManager(systemManager.getDeviceManager().getBME280Device(), &soilMoistureSensor);
        readingManager->begin();
        readingManagerInitialized = true;
        char timeStr[32] = "";
        snprintf(timeStr, sizeof(timeStr), "%04d-%02d-%02d %02d:%02d:%02d", now.year(), now.month(), now.day(), now.hour(), now.minute(), now.second());

        // --- MqttManager initialization: moved here to guarantee execution after ReadingManager ---
        Serial.printf("[DEBUG] MQTT init check: mqttManagerInitialized=%d, webServerStarted=%d, dashboard=%p, dashboard->hasValidSensorData()=%d\n",
            mqttManagerInitialized, webServerStarted, dashboard, (dashboard ? dashboard->hasValidSensorData() : -1));
        // Robust MQTT enabled detection: handle boolean and string
        cJSON* mqttEnabledItem = cJSON_GetObjectItemCaseSensitive(systemManager.getConfigManager().getRoot(), "mqtt_enabled");
        bool mqttEnabled = false;
        if (mqttEnabledItem) {
            if (cJSON_IsBool(mqttEnabledItem)) {
                mqttEnabled = cJSON_IsTrue(mqttEnabledItem);
            } else if (cJSON_IsString(mqttEnabledItem)) {
                const char* val = mqttEnabledItem->valuestring;
                mqttEnabled = val && (strcmp(val, "true") == 0 || strcmp(val, "1") == 0);
            }
        }
        Serial.printf("[DEBUG] MQTT config: mqtt_enabledItem type=%d, value='%s'\n", mqttEnabledItem ? mqttEnabledItem->type : -1, mqttEnabledItem && cJSON_IsString(mqttEnabledItem) ? mqttEnabledItem->valuestring : "(not string)");
        if (!mqttManagerInitialized && webServerStarted && dashboard && dashboard->hasValidSensorData()) {
            Serial.printf("[DEBUG] MQTT enabled: %d\n", mqttEnabled);
            if (mqttEnabled) {
                const char* deviceNameCStr = systemManager.getConfigManager().get("device_name");
                std::string deviceName = deviceNameCStr ? deviceNameCStr : "esp32_device";
                std::vector<std::string> relayNames;
                cJSON* relayNamesArr = cJSON_GetObjectItemCaseSensitive(systemManager.getConfigManager().getRoot(), "relay_names");
                if (relayNamesArr && cJSON_IsArray(relayNamesArr)) {
                    int arrSize = cJSON_GetArraySize(relayNamesArr);
                    for (int i = 0; i < arrSize; ++i) {
                        cJSON* item = cJSON_GetArrayItem(relayNamesArr, i);
                        if (cJSON_IsString(item)) relayNames.push_back(item->valuestring);
                    }
                } else {
                    relayNames = {"Zone 1", "Zone 2", "Zone 3", "Zone 4"};
                }
                Serial.printf("[DEBUG] MQTT begin: deviceName='%s', relayNames.size=%d\n", deviceName.c_str(), (int)relayNames.size());
                mqttManager.begin(deviceName, relayNames, &systemManager.getConfigManager());
                mqttManager.setInitialized(true);
                mqttManagerInitialized = true;
                Serial.println("[MqttManager] Home Assistant discovery published for relays.");
            }
        }
    }

    // --- ReadingManager initialization ---
    if (!readingManagerInitialized) {
        Serial.printf("[DEBUG] RM? sensorsInitialized=%d, webServerStarted=%d, dashboard=%p, dashboard->hasValidSensorData()=%d\n",
            sensorsInitialized, webServerStarted, dashboard, (dashboard ? dashboard->hasValidSensorData() : -1));
    }
    if (!readingManagerInitialized && sensorsInitialized && webServerStarted) {
        Serial.println("[DEBUG] Initializing ReadingManager...");
        readingManager = new ReadingManager(systemManager.getDeviceManager().getBME280Device(), &soilMoistureSensor);
        readingManager->begin();
        readingManagerInitialized = true;
        char timeStr[32] = "";
        snprintf(timeStr, sizeof(timeStr), "%04d-%02d-%02d %02d:%02d:%02d", now.year(), now.month(), now.day(), now.hour(), now.minute(), now.second());
        Serial.printf("[ReadingManager] Initialized at %s after sensors and web server ready.\n", timeStr);

        // --- MqttManager initialization: moved here to guarantee execution after everything else ---
        if (!mqttManagerInitialized) {
            const char* mqttEnabledStr = systemManager.getConfigManager().get("mqtt_enabled");
            bool mqttEnabled = mqttEnabledStr && (strcmp(mqttEnabledStr, "true") == 0 || strcmp(mqttEnabledStr, "1") == 0);
            if (mqttEnabled) {
                const char* deviceNameCStr = systemManager.getConfigManager().get("device_name");
                std::string deviceName = deviceNameCStr ? deviceNameCStr : "esp32_device";
                std::vector<std::string> relayNames;
                cJSON* relayNamesArr = cJSON_GetObjectItemCaseSensitive(systemManager.getConfigManager().getRoot(), "relay_names");
                if (relayNamesArr && cJSON_IsArray(relayNamesArr)) {
                    int arrSize = cJSON_GetArraySize(relayNamesArr);
                    for (int i = 0; i < arrSize; ++i) {
                        cJSON* item = cJSON_GetArrayItem(relayNamesArr, i);
                        if (cJSON_IsString(item)) relayNames.push_back(item->valuestring);
                    }
                } else {
                    relayNames = {"Zone 1", "Zone 2", "Zone 3", "Zone 4"};
                }
                mqttManager.begin(deviceName, relayNames, &systemManager.getConfigManager());
                mqttManager.setInitialized(true);
                mqttManagerInitialized = true;
                Serial.println("[MqttManager] Home Assistant discovery published for relays.");
            }
        }
    }
    if (readingManagerInitialized && readingManager) readingManager->loop(now);
    // MQTT loop
    if (mqttManagerInitialized) mqttManager.loop();
    
    if (sensorsInitialized && initState == INIT_COMPLETE) {
        irrigationManager.update();
        irrigationManager.checkAndRunScheduled();
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
            // Only trigger MQ135 if this was part of the initialisation, not from manual soil reading
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
                extern MqttManager mqttManager;
                if (mqttManager.isInitialized()) {
                    mqttManager.publishMQ135AirQuality();
                }
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
            sensorState = IDLE; // Always return to IDLE so deferred readings can trigger
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
