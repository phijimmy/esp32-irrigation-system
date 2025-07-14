#include "devices/SoilMoistureSensor.h"
#include "config/ConfigManager.h"
#include <time.h>
#include "system/TimeManager.h"

SoilMoistureSensor::SoilMoistureSensor() {}

void SoilMoistureSensor::begin(ADS1115Manager* adsMgr, ConfigManager* configMgr, TimeManager* timeMgr, DiagnosticManager* diagMgr) {
    ads = adsMgr;
    config = configMgr;
    timeManager = timeMgr;
    diagnosticManager = diagMgr;
    state = IDLE;
    // Get stabilisation time from config->soil_moisture if available
    if (config) {
        cJSON* soilSection = config->getSection("soil_moisture");
        if (soilSection) {
            cJSON* stabItem = cJSON_GetObjectItem(soilSection, "stabilisation_time");
            int t = 10;
            if (cJSON_IsNumber(stabItem)) t = stabItem->valueint;
            if (t > 0) stabilisationTimeSec = t;
        }
        // Get soil power gpio from config
        soilPowerGpio = config->getInt("soil_power_gpio", 16);
        if (soilPowerGpio >= 0) {
            pinMode(soilPowerGpio, OUTPUT);
            digitalWrite(soilPowerGpio, LOW); // Ensure off at boot
        }
    }
    // Start initial stabilisation (non-blocking)
    beginStabilisation();
    if (soilPowerGpio >= 0) {
        Serial.printf("[SoilMoistureSensor] Powering on sensor (GPIO %d) for initial reading...\n", soilPowerGpio);
    }
    Serial.printf("[SoilMoistureSensor] Starting non-blocking stabilisation: %d seconds...\n", stabilisationTimeSec);
    // Note: Initial reading will be taken when readyForReading() returns true
}

void SoilMoistureSensor::printReading() const {
    char timeStr[32];
    struct tm* tm_info = localtime(&lastReading.timestamp);
    strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", tm_info);
    Serial.print("[SoilMoistureSensor] Reading: raw=");
    Serial.print(lastReading.raw);
    Serial.print(", voltage=");
    Serial.print(lastReading.voltage, 4);
    Serial.print(", percent=");
    Serial.print(lastReading.percent, 1);
    Serial.print("% | avgRaw=");
    Serial.print(lastReading.avgRaw, 1);
    Serial.print(", avgVoltage=");
    Serial.print(lastReading.avgVoltage, 4);
    Serial.print(", avgPercent=");
    Serial.print(lastReading.avgPercent, 1);
    Serial.print("%, timestamp=");
    Serial.println(timeStr);
}

void SoilMoistureSensor::beginStabilisation() {
    stabilisationStart = millis();
    state = STABILISING;
    if (soilPowerGpio >= 0) {
        digitalWrite(soilPowerGpio, HIGH); // Power on sensor
    }
}

bool SoilMoistureSensor::readyForReading() const {
    return (millis() - stabilisationStart) >= (unsigned long)(stabilisationTimeSec * 1000);
}

void SoilMoistureSensor::takeReading() {
    state = READING;
    // Take 10 readings in quick succession
    const int N = 10;
    float rawVals[N], voltVals[N], percentVals[N];
    for (int i = 0; i < N; ++i) {
        rawVals[i] = (float)readRaw();
        voltVals[i] = readVoltage();
        percentVals[i] = readPercent();
        delay(10); // Small delay between readings
    }
    // Store the first reading as the 'single' value
    lastReading.raw = (int16_t)rawVals[0];
    lastReading.voltage = voltVals[0];
    lastReading.percent = percentVals[0];
    // Filter outliers and average
    filterAndAverage(rawVals, voltVals, percentVals, N, lastReading.avgRaw, lastReading.avgVoltage, lastReading.avgPercent);
    if (timeManager) {
        DateTime dt = timeManager->getLocalTime();
        if (!dt.isValid() || dt.year() < 2000 || dt.month() < 1 || dt.month() > 12 || dt.day() < 1 || dt.day() > 31) {
            if (diagnosticManager) {
                diagnosticManager->log(DiagnosticManager::LOG_WARN, "SoilMoistureSensor", "Invalid timestamp detected after reading (year=%d, month=%d, day=%d) - setting to 0", dt.year(), dt.month(), dt.day());
            }
            lastReading.timestamp = 0; // Set to known invalid value
        } else {
            lastReading.timestamp = dt.unixtime();
        }
    } else {
        lastReading.timestamp = time(nullptr);
    }
    if (soilPowerGpio >= 0) {
        digitalWrite(soilPowerGpio, LOW); // Power off sensor after reading
    }
    state = IDLE;
}

void SoilMoistureSensor::filterAndAverage(float* rawVals, float* voltVals, float* percentVals, int count, float& avgRaw, float& avgVolt, float& avgPercent) {
    // Simple outlier rejection: discard min and max, average the rest
    if (count <= 2) {
        avgRaw = rawVals[0];
        avgVolt = voltVals[0];
        avgPercent = percentVals[0];
        return;
    }
    // Find min/max indices
    int minIdx = 0, maxIdx = 0;
    for (int i = 1; i < count; ++i) {
        if (rawVals[i] < rawVals[minIdx]) minIdx = i;
        if (rawVals[i] > rawVals[maxIdx]) maxIdx = i;
    }
    float sumRaw = 0, sumVolt = 0, sumPercent = 0;
    int n = 0;
    for (int i = 0; i < count; ++i) {
        if (i == minIdx || i == maxIdx) continue;
        sumRaw += rawVals[i];
        sumVolt += voltVals[i];
        sumPercent += percentVals[i];
        ++n;
    }
    if (n > 0) {
        avgRaw = sumRaw / n;
        avgVolt = sumVolt / n;
        avgPercent = sumPercent / n;
    } else {
        avgRaw = rawVals[0];
        avgVolt = voltVals[0];
        avgPercent = percentVals[0];
    }
}

const SoilMoistureSensor::Reading& SoilMoistureSensor::getLastReading() const {
    return lastReading;
}

int16_t SoilMoistureSensor::readRaw() {
    if (!ads) return 0;
    // Use single-ended mode, channel 0 (A0)
    uint16_t raw = ads->readRaw(channel, gain, 100);
    return (int16_t)raw;
}

float SoilMoistureSensor::readVoltage() {
    if (!ads) return 0.0f;
    return ads->readVoltage(channel, gain, 100);
}

void SoilMoistureSensor::readBoth(int16_t& raw, float& voltage) {
    raw = readRaw();
    voltage = readVoltage();
}

float SoilMoistureSensor::readPercent() {
    if (!config) return 0.0f;
    cJSON* soilSection = config->getSection("soil_moisture");
    if (!soilSection) return 0.0f;
    int wet = 0;
    int dry = 11300;
    cJSON* wetItem = cJSON_GetObjectItem(soilSection, "wet");
    cJSON* dryItem = cJSON_GetObjectItem(soilSection, "dry");
    if (cJSON_IsNumber(wetItem)) wet = wetItem->valueint;
    if (cJSON_IsNumber(dryItem)) dry = dryItem->valueint;
    int16_t raw = readRaw();
    float percent = 0.0f;
    if (dry != wet) {
        percent = 100.0f * (float)(dry - raw) / (float)(dry - wet);
        if (percent < 0.0f) percent = 0.0f;
        if (percent > 100.0f) percent = 100.0f;
    }
    return percent;
}
