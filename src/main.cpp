#include <Arduino.h>
#include "system/SystemManager.h"
#include "devices/LedDevice.h"
#include "devices/TouchSensorDevice.h"
#include "devices/RelayController.h"
#include "devices/SoilMoistureSensor.h"
#include "devices/MQ135Sensor.h"
#include "devices/IrrigationManager.h"
#include "system/DashboardManager.h"

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
IrrigationManager irrigationManager;
unsigned long irrigationTriggerTime = 0;
bool irrigationTriggerScheduled = false;
bool irrigationTriggered = false;

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
    led.setDiagnosticManager(&systemManager.getDiagnosticManager());
    systemManager.getDeviceManager().addDevice(&led);
    led.begin(); // Explicitly call begin() since DeviceManager.begin() was called before adding this device
    led.setMode(LedDevice::BLINK);

    // Get touch config from proper config sections
    int touchGpio = cJSON_GetObjectItem(root, "touch_gpio") ? cJSON_GetObjectItem(root, "touch_gpio")->valueint : 4;
    int touchLongPress = cJSON_GetObjectItem(root, "touch_long_press") ? cJSON_GetObjectItem(root, "touch_long_press")->valueint : 5000;
    
    touch.setConfigManager(&systemManager.getConfigManager());
    touch.setDiagnosticManager(&systemManager.getDiagnosticManager());
    touch.setGpio(touchGpio);
    touch.setLongPressDuration(touchLongPress);
    systemManager.getDeviceManager().addDevice(&touch);
    touch.begin(); // Explicitly call begin() since DeviceManager.begin() was called before adding this device

    relayController.setConfigManager(&systemManager.getConfigManager());
    relayController.setDiagnosticManager(&systemManager.getDiagnosticManager());
    systemManager.getDeviceManager().addDevice(&relayController);
    relayController.begin(); // Explicitly call begin() since DeviceManager.begin() was called before adding this device
    
    // Connect TimeManager to RelayController for INT/SQW relay control
    systemManager.getTimeManager().setRelayController(&relayController);

    // Now that relays are initialized, initialize MQ135
    mq135Sensor.begin(&systemManager.getADS1115Manager(), &systemManager.getConfigManager(), &relayController);
    mq135Sensor.setTimeManager(&systemManager.getTimeManager());
    // Take initial MQ135 reading after warmup
    mq135Sensor.startReading();
    Serial.println("[MQ135Sensor] Powering on sensor for initial reading (relay 3)...");
    Serial.printf("[MQ135Sensor] Waiting for warmup: %d seconds...\n", mq135Sensor.getWarmupTimeSec());
    unsigned long mq135Start = millis();
    while ((millis() - mq135Start) < (unsigned long)(mq135Sensor.getWarmupTimeSec() * 1000)) {
        delay(50);
    }
    mq135Sensor.takeReading();
    Serial.println("[MQ135Sensor] Initial reading after warmup:");
    const MQ135Sensor::Reading& mq135r = mq135Sensor.getLastReading();
    char timeStr[32];
    struct tm* tm_info = localtime(&mq135r.timestamp);
    strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", tm_info);
    Serial.printf("[MQ135Sensor] Reading: raw=%d, voltage=%.4f V | avgRaw=%.1f, avgVoltage=%.4f V, AQI=%s, timestamp=%s\n",
        mq135r.raw, mq135r.voltage, mq135r.avgRaw, mq135r.avgVoltage, MQ135Sensor::getAirQualityLabel(mq135r.avgVoltage), timeStr);
    
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
    static DashboardManager dashboard(
        &systemManager.getTimeManager(),
        &systemManager.getConfigManager(),
        &systemManager, // Pass SystemManager pointer
        &systemManager.getDiagnosticManager(),
        &led, // Pass LedDevice pointer
        &relayController, // Pass RelayController pointer
        &touch, // Pass TouchSensorDevice pointer
        systemManager.getDeviceManager().getBME280Device() // Pass BME280Device pointer
    );
    dashboard.setSoilMoistureSensor(&soilMoistureSensor);
    dashboard.begin();
    Serial.println("[DashboardManager] JSON status:");
    Serial.println(dashboard.getStatusString());
}

void loop() {
    systemManager.update();
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
        // After BME280, take soil moisture reading
        if (now.hour() != lastSoilHour) {
            soilMoistureSensor.beginStabilisation();
            Serial.printf("[SoilMoistureSensor] Hourly: Powering on sensor (GPIO %d) for reading...\n", soilMoistureSensor.getPowerGpio());
            Serial.printf("[SoilMoistureSensor] Hourly: Waiting for stabilisation: %d seconds...\n", soilMoistureSensor.getStabilisationTimeSec());
            unsigned long start = millis();
            while ((millis() - start) < (unsigned long)(soilMoistureSensor.getStabilisationTimeSec() * 1000)) {
                delay(50);
            }
            soilMoistureSensor.takeReading();
            // Ensure power GPIO is set LOW after reading
            digitalWrite(soilMoistureSensor.getPowerGpio(), LOW);
            Serial.println("[SoilMoistureSensor] Hourly reading after stabilisation:");
            soilMoistureSensor.printReading();
            lastSoilHour = now.hour();
        }
    }
    switch (sensorState) {
        case IDLE:
            // Serial.println("[SoilMoistureSensor] Reading requested, starting stabilisation...");
            // soilReadingRequested = true;
            // soilReadingTaken = false;
            // soilMoistureSensor.beginStabilisation();
            // lastStabilisationPrint = millis();
            // sensorState = SOIL_STABILISING;
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
                sensorState = SOIL_DONE;
            }
            break;
        }
        case SOIL_DONE:
            Serial.println("[MQ135Sensor] Starting air quality sensor warmup...");
            mq135Sensor.startReading();
            mq135ReadingRequested = true;
            mq135ReadingTaken = false;
            mq135LastProgressPrint = millis();
            sensorState = MQ135_WARMUP;
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
