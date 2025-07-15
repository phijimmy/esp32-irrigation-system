#include "system/ReadingManager.h"
#include "devices/BME280Device.h"
#include "devices/SoilMoistureSensor.h"
#include <time.h>
#include <Arduino.h>

ReadingManager::ReadingManager(BME280Device* bme, SoilMoistureSensor* soil)
    : bme280(bme), soilSensor(soil) {}

void ReadingManager::begin() {
    lastHour = -1;
}

enum SoilState { SOIL_IDLE, SOIL_STABILISING, SOIL_READY };
static SoilState soilState = SOIL_IDLE;
static unsigned long soilStabStart = 0;
static bool timeValid = false;

void ReadingManager::loop(const DateTime& now) {
    static int lastLoggedHour = -2; // -2 so first valid time always logs

    int year = now.year();
    if (!timeValid) {
        if (year >= 2020) {
            timeValid = true;
            lastHour = now.hour(); // Prevent false trigger on first valid time
            Serial.printf("[ReadingManager] Time became valid: %04d-%02d-%02d %02d:%02d:%02d\n", year, now.month(), now.day(), now.hour(), now.minute(), now.second());
            lastLoggedHour = now.hour();
        } else {
            return;
        }
    }

    // Only log once per hour (on hour change or first valid time)
    if (now.hour() != lastLoggedHour) {
        Serial.printf("[ReadingManager] loop: time=%04d-%02d-%02d %02d:%02d:%02d, lastHour=%d, soilState=%d\n",
            now.year(), now.month(), now.day(), now.hour(), now.minute(), now.second(), lastHour, soilState);
        lastLoggedHour = now.hour();
    }

    // Hourly trigger: allow the entire first minute (xx:00:00–xx:00:59), but only once per hour
    if (timeValid && now.minute() == 0 && soilState == SOIL_IDLE) {
        if (now.hour() != lastHour) {
            Serial.printf("[ReadingManager] Hourly trigger at %02d:00.\n", now.hour());
            if (bme280) {
                auto reading = bme280->readData();
                Serial.printf("[ReadingManager] BME280: T=%.2fC, H=%.2f%%, P=%.2fhPa, time=%04d-%02d-%02d %02d:%02d:%02d\n",
                    reading.temperature, reading.humidity, reading.pressure,
                    now.year(), now.month(), now.day(), now.hour(), now.minute(), now.second());
            }
            if (soilSensor) {
                int stab = soilSensor->getStabilisationTimeSec();
                soilSensor->setStabilisationTimeSec(stab); // ensure config is used
                soilSensor->beginStabilisation();
                soilStabStart = millis();
                soilState = SOIL_STABILISING;
            }
            lastHour = now.hour();
        }
    }

    // Non-blocking soil stabilisation with progress output
    static unsigned long lastStabProgressPrint = 0;
    if (soilState == SOIL_STABILISING && soilSensor) {
        unsigned long nowMs = millis();
        int elapsed = (nowMs - soilStabStart) / 1000;
        int total = soilSensor->getStabilisationTimeSec();
        if (nowMs - lastStabProgressPrint >= 1000) {
            Serial.printf("[ReadingManager][SoilMoistureSensor] Stabilising... %d/%d seconds\n", elapsed, total);
            lastStabProgressPrint = nowMs;
        }
        if (soilSensor->readyForReading()) {
            soilSensor->takeReading();
            auto s = soilSensor->getLastReading();
            Serial.printf("[ReadingManager] Soil: raw=%d, V=%.3f, %%=%.1f, time=%ld\n", s.raw, s.voltage, s.percent, s.timestamp);
            soilState = SOIL_IDLE;
            lastStabProgressPrint = 0;
        } else if (nowMs - soilStabStart > 60000) {
            Serial.println("[ReadingManager] Soil: Stabilisation timeout!");
            soilState = SOIL_IDLE;
            lastStabProgressPrint = 0;
        }
    }
}
