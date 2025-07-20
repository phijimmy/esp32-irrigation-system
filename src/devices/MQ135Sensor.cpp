#include <Arduino.h>

#include "devices/MQ135Sensor.h"

#include "system/MqttManager.h"

void MQ135Sensor::forceIdle() {
    state = IDLE;
    warmingUp = false;
}

#include "devices/MQ135Sensor.h"
#include <time.h>

MQ135Sensor::MQ135Sensor() { state = IDLE; }

void MQ135Sensor::begin(ADS1115Manager* adsMgr, ConfigManager* configMgr, RelayController* relayCtrl, DiagnosticManager* diagMgr) {
    ads = adsMgr;
    config = configMgr;
    relay = relayCtrl;
    diagnosticManager = diagMgr;
    if (config) {
        cJSON* mq135Section = config->getSection("mq135");
        if (mq135Section) {
            cJSON* warmup = cJSON_GetObjectItem(mq135Section, "warmup_time");
            if (cJSON_IsNumber(warmup)) {
                warmupTimeSec = warmup->valueint;
            }
        }
    }
    warmingUp = false;
    state = IDLE;
    // Do NOT power on or take a reading here! Only set up pointers/config.
}

void MQ135Sensor::startReading() {
    if (!relay) { state = ERROR; return; }
    relay->activateRelay(3); // GPIO 26
    warmupStart = millis();
    warmingUp = true;
    state = WARMING_UP;
}

bool MQ135Sensor::readyForReading() const {
    return warmingUp && (millis() - warmupStart) >= (unsigned long)(warmupTimeSec * 1000);
}

void MQ135Sensor::takeReading() {
    if (!ads || !relay) { state = ERROR; return; }
    state = READING;
    // Take 10 readings in quick succession
    const int N = 10;
    float rawVals[N], voltVals[N];
    for (int i = 0; i < N; ++i) {
        rawVals[i] = (float)ads->readRaw(channel, gain, 100);
        voltVals[i] = ads->readVoltage(channel, gain, 100);
        delay(10); // Small delay between readings
    }
    // Store the first reading as the 'single' value
    lastReading.raw = (int16_t)rawVals[0];
    lastReading.voltage = voltVals[0];
    // Filter outliers and average
    filterAndAverage(rawVals, voltVals, N, lastReading.avgRaw, lastReading.avgVoltage);
    if (timeManager) {
        DateTime dt = timeManager->getLocalTime();
        if (!dt.isValid() || dt.year() < 2000 || dt.month() < 1 || dt.month() > 12 || dt.day() < 1 || dt.day() > 31) {
            if (diagnosticManager) {
                diagnosticManager->log(DiagnosticManager::LOG_WARN, "MQ135Sensor", "Invalid timestamp detected after reading (year=%d, month=%d, day=%d) - setting to 0", dt.year(), dt.month(), dt.day());
            }
            lastReading.timestamp = 0;
        } else {
            lastReading.timestamp = dt.unixtime();
        }
    } else {
        lastReading.timestamp = time(nullptr);
    }
    lastReading.valid = true;
    relay->deactivateRelay(3); // Power off sensor
    warmingUp = false;
    state = IDLE;
    // Publish updated air quality to Home Assistant after every reading
    extern MqttManager mqttManager;
    mqttManager.publishMQ135AirQuality();
}

void MQ135Sensor::filterAndAverage(float* rawVals, float* voltVals, int count, float& avgRaw, float& avgVolt) {
    // Simple outlier rejection: discard min and max, average the rest
    if (count <= 2) {
        avgRaw = rawVals[0];
        avgVolt = voltVals[0];
        return;
    }
    int minIdx = 0, maxIdx = 0;
    for (int i = 1; i < count; ++i) {
        if (rawVals[i] < rawVals[minIdx]) minIdx = i;
        if (rawVals[i] > rawVals[maxIdx]) maxIdx = i;
    }
    float sumRaw = 0, sumVolt = 0;
    int n = 0;
    for (int i = 0; i < count; ++i) {
        if (i == minIdx || i == maxIdx) continue;
        sumRaw += rawVals[i];
        sumVolt += voltVals[i];
        ++n;
    }
    if (n > 0) {
        avgRaw = sumRaw / n;
        avgVolt = sumVolt / n;
    } else {
        avgRaw = rawVals[0];
        avgVolt = voltVals[0];
    }
}

const MQ135Sensor::Reading& MQ135Sensor::getLastReading() const {
    return lastReading;
}
