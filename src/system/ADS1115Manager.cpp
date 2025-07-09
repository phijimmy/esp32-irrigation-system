#include <Wire.h>
#include "system/ADS1115Manager.h"

ADS1115Manager::ADS1115Manager() {}

void ADS1115Manager::begin(void* unused, uint8_t i2cAddress) {
    address = i2cAddress;
    Wire.begin();
    connected = checkConnection();
}

bool ADS1115Manager::checkConnection() {
    Wire.beginTransmission(address);
    uint8_t error = Wire.endTransmission();
    return (error == 0);
}

bool ADS1115Manager::isConnected() const {
    return connected;
}

uint8_t ADS1115Manager::getAddress() const {
    return address;
}

uint16_t ADS1115Manager::readRaw(uint8_t channel, Gain gain, uint16_t timeoutMs) {
    // Config register bits
    uint16_t config = 0x8000; // Start single conversion
    // Set MUX for single-ended mode: 0x04,0x05,0x06,0x07 for A0-A3
    config |= (0x04 + (channel & 0x03)) << 12;
    config |= (gain & 0x07) << 9;     // PGA bits
    config |= 0x0100; // Single-shot mode
    config |= 0x0083; // 128SPS, disable comparator

    // Write config
    Wire.beginTransmission(address);
    Wire.write(0x01); // Config register
    Wire.write((config >> 8) & 0xFF);
    Wire.write(config & 0xFF);
    Wire.endTransmission();

    // Wait for conversion
    uint32_t start = millis();
    while (millis() - start < timeoutMs) {
        Wire.beginTransmission(address);
        Wire.write(0x01);
        Wire.endTransmission();
        Wire.requestFrom(address, (uint8_t)2);
        uint8_t hi = Wire.read();
        uint8_t lo = Wire.read();
        if (hi & 0x80) break; // Conversion ready
    }
    // Read conversion result
    Wire.beginTransmission(address);
    Wire.write(0x00); // Conversion register
    Wire.endTransmission();
    Wire.requestFrom(address, (uint8_t)2);
    uint16_t raw = ((uint16_t)Wire.read() << 8) | Wire.read();
    return raw;
}

float ADS1115Manager::readVoltage(uint8_t channel, Gain gain, uint16_t timeoutMs) {
    uint16_t raw = readRaw(channel, gain, timeoutMs);
    float multiplier = 0.1875f / 1000.0f; // Default for GAIN_TWOTHIRDS
    switch (gain) {
        case GAIN_TWOTHIRDS: multiplier = 0.1875f / 1000.0f; break;
        case GAIN_ONE:       multiplier = 0.125f / 1000.0f; break;
        case GAIN_TWO:       multiplier = 0.0625f / 1000.0f; break;
        case GAIN_FOUR:      multiplier = 0.03125f / 1000.0f; break;
        case GAIN_EIGHT:     multiplier = 0.015625f / 1000.0f; break;
        case GAIN_SIXTEEN:   multiplier = 0.0078125f / 1000.0f; break;
    }
    int16_t value = (int16_t)raw;
    return value * multiplier;
}
