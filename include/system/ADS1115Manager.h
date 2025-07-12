#ifndef ADS1115_MANAGER_H
#define ADS1115_MANAGER_H

#include <Arduino.h>

class ADS1115Manager {
public:
    enum Gain {
        GAIN_TWOTHIRDS = 0, // +/-6.144V
        GAIN_ONE = 1,       // +/-4.096V
        GAIN_TWO = 2,       // +/-2.048V
        GAIN_FOUR = 3,      // +/-1.024V
        GAIN_EIGHT = 4,     // +/-0.512V
        GAIN_SIXTEEN = 5    // +/-0.256V
    };

    ADS1115Manager();
    void begin(SemaphoreHandle_t i2cMutex = nullptr, uint8_t i2cAddress = 0x48);
    float readVoltage(uint8_t channel, Gain gain = GAIN_TWOTHIRDS, uint16_t timeoutMs = 100);
    bool isConnected() const;
    uint8_t getAddress() const;
    uint16_t readRaw(uint8_t channel, Gain gain, uint16_t timeoutMs);

private:
    uint8_t address = 0x48;
    bool connected = false;
    SemaphoreHandle_t i2cMutex = nullptr;
    bool checkConnection();
};

#endif // ADS1115_MANAGER_H
