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
private:
    std::string deviceName;
    std::vector<std::string> relayNames;
    bool initialized;
    void publishDiscoveryForRelay(int relayIndex);
    void publish(const std::string& topic, cJSON* payload);
    void publish(const std::string& topic, const char* payload);
};
