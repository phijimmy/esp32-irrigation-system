#include <Arduino.h>

#include "devices/BME280Device.h"

void BME280Device::forceIdle() {
    state = READY;
}

#include "system/TimeManager.h"

BME280Device::BME280Device(uint8_t addr, I2CManager* i2c, DiagnosticManager* diag)
    : address(addr), i2cManager(i2c), diagnosticManager(diag) {}

void BME280Device::setTimeManager(TimeManager* timeMgr) {
    timeManager = timeMgr;
}

bool BME280Device::begin() {
    state = READING;
    bool ok = false;
    if (i2cManager && i2cManager->getI2CMutex()) xSemaphoreTake(i2cManager->getI2CMutex(), portMAX_DELAY);
    ok = bme.begin(address);
    if (i2cManager && i2cManager->getI2CMutex()) xSemaphoreGive(i2cManager->getI2CMutex());
    if (!ok) {
        if (diagnosticManager) diagnosticManager->log(DiagnosticManager::LOG_WARN, "BME280", "Device not found at 0x%02X", address);
        initialized = false;
        state = ERROR;
        lastError = "Device not found";
        return false;
    }
    if (i2cManager && i2cManager->getI2CMutex()) xSemaphoreTake(i2cManager->getI2CMutex(), portMAX_DELAY);
    bme.setSampling(Adafruit_BME280::MODE_SLEEP, Adafruit_BME280::SAMPLING_X16, Adafruit_BME280::SAMPLING_X16, Adafruit_BME280::SAMPLING_X16, Adafruit_BME280::FILTER_X16, Adafruit_BME280::STANDBY_MS_0_5);
    if (i2cManager && i2cManager->getI2CMutex()) xSemaphoreGive(i2cManager->getI2CMutex());
    initialized = true;
    state = READY;
    if (diagnosticManager) diagnosticManager->log(DiagnosticManager::LOG_INFO, "BME280", "Initialized at 0x%02X", address);
    // Take a full set of readings after initialization
    readData();
    if (diagnosticManager) {
        diagnosticManager->log(DiagnosticManager::LOG_INFO, "BME280", "Initial reading: T=%.2fC, H=%.2f%%, P=%.2fhPa, HI=%.2fC, DP=%.2fC, ts=%04d-%02d-%02d %02d:%02d:%02d", 
            lastReading.temperature, lastReading.humidity, lastReading.pressure, lastReading.heatIndex, lastReading.dewPoint,
            lastReading.timestamp.year(), lastReading.timestamp.month(), lastReading.timestamp.day(),
            lastReading.timestamp.hour(), lastReading.timestamp.minute(), lastReading.timestamp.second());
        diagnosticManager->log(DiagnosticManager::LOG_INFO, "BME280", "Averaged: T=%.2fC, H=%.2f%%, P=%.2fhPa, HI=%.2fC, DP=%.2fC", 
            lastReading.avgTemperature, lastReading.avgHumidity, lastReading.avgPressure, lastReading.avgHeatIndex, lastReading.avgDewPoint);
    }
    return true;
}

BME280Reading BME280Device::readData() {
    state = UPDATING;
    const int N = 10;
    BME280Reading readings[N];
    for (int i = 0; i < N; ++i) {
        if (!initialized) {
            if (!begin()) {
                BME280Reading result;
                result.valid = false;
                state = ERROR;
                lastError = "Not initialized";
                return result;
            }
        }
        if (i2cManager && i2cManager->getI2CMutex()) xSemaphoreTake(i2cManager->getI2CMutex(), portMAX_DELAY);
        bme.setSampling(Adafruit_BME280::MODE_FORCED, Adafruit_BME280::SAMPLING_X16, Adafruit_BME280::SAMPLING_X16, Adafruit_BME280::SAMPLING_X16, Adafruit_BME280::FILTER_X16, Adafruit_BME280::STANDBY_MS_0_5);
        delay(100);
        readings[i].temperature = bme.readTemperature();
        readings[i].humidity = bme.readHumidity();
        readings[i].pressure = bme.readPressure() / 100.0F;
        readings[i].heatIndex = computeHeatIndex(readings[i].temperature, readings[i].humidity);
        readings[i].dewPoint = computeDewPoint(readings[i].temperature, readings[i].humidity);
        if (i2cManager && i2cManager->getI2CMutex()) xSemaphoreGive(i2cManager->getI2CMutex());
        readings[i].timestamp = timeManager ? timeManager->getTime() : DateTime();
        // Defensive check for invalid timestamp
        if (!readings[i].timestamp.isValid() || readings[i].timestamp.year() < 2000 || readings[i].timestamp.month() < 1 || readings[i].timestamp.month() > 12 || readings[i].timestamp.day() < 1 || readings[i].timestamp.day() > 31) {
            if (diagnosticManager) {
                diagnosticManager->log(DiagnosticManager::LOG_WARN, "BME280", "Invalid timestamp detected after reading (year=%d, month=%d, day=%d) - setting to 0", readings[i].timestamp.year(), readings[i].timestamp.month(), readings[i].timestamp.day());
            }
            readings[i].timestamp = DateTime(1970, 1, 1, 0, 0, 0); // Set to known invalid value
        }
        readings[i].valid = true;
    }
    // Store the first reading as the 'single' value
    BME280Reading result = readings[0];
    // Filter outliers and average
    filterAndAverage(readings, N, result);
    lastReading = result;
    if (i2cManager && i2cManager->getI2CMutex()) xSemaphoreTake(i2cManager->getI2CMutex(), portMAX_DELAY);
    bme.setSampling(Adafruit_BME280::MODE_SLEEP, Adafruit_BME280::SAMPLING_X16, Adafruit_BME280::SAMPLING_X16, Adafruit_BME280::SAMPLING_X16, Adafruit_BME280::FILTER_X16, Adafruit_BME280::STANDBY_MS_0_5);
    if (i2cManager && i2cManager->getI2CMutex()) xSemaphoreGive(i2cManager->getI2CMutex());
    state = READY;
    return result;
}

void BME280Device::filterAndAverage(const BME280Reading* readings, int count, BME280Reading& avgResult) {
    // Simple outlier rejection: discard min/max temperature, average the rest
    if (count <= 2) {
        avgResult.avgTemperature = readings[0].temperature;
        avgResult.avgHumidity = readings[0].humidity;
        avgResult.avgPressure = readings[0].pressure;
        avgResult.avgHeatIndex = readings[0].heatIndex;
        avgResult.avgDewPoint = readings[0].dewPoint;
        return;
    }
    int minIdx = 0, maxIdx = 0;
    for (int i = 1; i < count; ++i) {
        if (readings[i].temperature < readings[minIdx].temperature) minIdx = i;
        if (readings[i].temperature > readings[maxIdx].temperature) maxIdx = i;
    }
    float sumT = 0, sumH = 0, sumP = 0, sumHI = 0, sumDP = 0;
    int n = 0;
    for (int i = 0; i < count; ++i) {
        if (i == minIdx || i == maxIdx) continue;
        sumT += readings[i].temperature;
        sumH += readings[i].humidity;
        sumP += readings[i].pressure;
        sumHI += readings[i].heatIndex;
        sumDP += readings[i].dewPoint;
        ++n;
    }
    if (n > 0) {
        avgResult.avgTemperature = sumT / n;
        avgResult.avgHumidity = sumH / n;
        avgResult.avgPressure = sumP / n;
        avgResult.avgHeatIndex = sumHI / n;
        avgResult.avgDewPoint = sumDP / n;
    } else {
        avgResult.avgTemperature = readings[0].temperature;
        avgResult.avgHumidity = readings[0].humidity;
        avgResult.avgPressure = readings[0].pressure;
        avgResult.avgHeatIndex = readings[0].heatIndex;
        avgResult.avgDewPoint = readings[0].dewPoint;
    }
}

float BME280Device::computeHeatIndex(float t, float h) {
    // Only valid for t >= 26C
    if (t < 26.0f) return t;
    // Rothfusz regression (Celsius version)
    float hi = -8.784695 + 1.61139411 * t + 2.338549 * h - 0.14611605 * t * h
        - 0.012308094 * t * t - 0.016424828 * h * h + 0.002211732 * t * t * h
        + 0.00072546 * t * h * h - 0.000003582 * t * t * h * h;
    return hi;
}

float BME280Device::computeDewPoint(float t, float h) {
    // Magnus formula
    const float a = 17.62, b = 243.12;
    float gamma = (a * t) / (b + t) + log(h / 100.0f);
    return (b * gamma) / (a - gamma);
}
