#pragma once
#include <cJSON.h>
#include <string>
#include <vector>
#include "config/ConfigManager.h"
class MqttManager {
public:
    MqttManager();
    void begin(const std::string& deviceName, const std::vector<std::string>& relayNames, ConfigManager* configManager);
    void publishDiscovery();
    void publishRelayState(int relayIndex, bool state);
    void handleRelayCommand(int relayIndex, const std::string& payload);
    void loop();
    void setInitialized(bool initialized);
    bool isInitialized() const;
    void publishDiscoveryForBME280Temperature();
    void publishBME280Temperature(float temperature);
    // Add declarations for new BME280 sensor functions
    void publishDiscoveryForBME280Humidity();
    void publishDiscoveryForBME280Pressure();
    void publishDiscoveryForBME280HeatIndex();
    void publishDiscoveryForBME280DewPoint();
    void publishBME280Humidity(float humidity);
    void publishBME280Pressure(float pressure);
    void publishBME280HeatIndex(float heatIndex);
    void publishBME280DewPoint(float dewPoint);
private:
    std::string deviceName;
    std::vector<std::string> relayNames;
    bool initialized;
    void publishDiscoveryForRelay(int relayIndex);
    void publish(const std::string& topic, cJSON* payload);
    void publish(const std::string& topic, const char* payload);
};
