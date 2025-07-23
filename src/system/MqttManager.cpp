// Implementation moved after constructor and includes

// Includes
#include <Arduino.h>
#include "system/MqttManager.h"
#include "devices/RelayController.h"
#include "devices/MQ135Sensor.h"
#include "devices/BME280Device.h"
#include "devices/SoilMoistureSensor.h"
#include "system/SystemManager.h"
#include <cJSON.h>
#include <string>
#include <vector>
#include <stdio.h>
#include <WiFi.h>
#include <PubSubClient.h>

WiFiClient espClient;
PubSubClient mqttClient(espClient);

// MQTT config values (should be set from ConfigManager)
std::string mqttServer;
int mqttPort = 1883;
std::string mqttUser;
std::string mqttPass;
std::string deviceName;
std::vector<std::string> relayNames;

extern SystemManager systemManager;

MqttManager::MqttManager() : initialized(false) {}

// Publish Home Assistant MQTT Discovery for BME280 temperature sensor
void MqttManager::publishDiscoveryForBME280Temperature() {
    char state_topic[128], availability_topic[128], topic[128], unique_id[128];
    snprintf(state_topic, sizeof(state_topic), "homeassistant/%s/bme280_temperature/state", deviceName.c_str());
    snprintf(availability_topic, sizeof(availability_topic), "homeassistant/esp32/%s/availability", deviceName.c_str());
    snprintf(topic, sizeof(topic), "homeassistant/sensor/%s_bme280_temperature/config", deviceName.c_str());
    snprintf(unique_id, sizeof(unique_id), "%s_bme280_temperature", deviceName.c_str());
    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "name", "BME280 Temperature");
    cJSON_AddStringToObject(root, "state_topic", state_topic);
    cJSON_AddStringToObject(root, "unit_of_measurement", "°C");
    cJSON_AddStringToObject(root, "device_class", "temperature");
    cJSON_AddStringToObject(root, "availability_topic", availability_topic);
    cJSON_AddStringToObject(root, "payload_available", "online");
    cJSON_AddStringToObject(root, "payload_not_available", "offline");
    cJSON_AddStringToObject(root, "unique_id", unique_id);
    cJSON* device = cJSON_CreateObject();
    cJSON_AddItemToObject(root, "device", device);
    cJSON_AddItemToArray(cJSON_AddArrayToObject(device, "identifiers"), cJSON_CreateString(deviceName.c_str()));
    cJSON_AddStringToObject(device, "manufacturer", "Binghe");
    cJSON_AddStringToObject(device, "model", "LC-Relay-ESP32-4R-A2");
    cJSON_AddStringToObject(device, "name", deviceName.c_str());
    publish(topic, root);
}

// Publish BME280 temperature value to MQTT
void MqttManager::publishBME280Temperature(float temperature) {
    char topic[128];
    snprintf(topic, sizeof(topic), "homeassistant/%s/bme280_temperature/state", deviceName.c_str());
    // Add random jitter between 0.01 and 0.02
    float jitter = ((float)random(10, 21)) / 100.0f; // 0.10 to 0.20, but we want 0.01 to 0.02
    jitter = jitter / 10.0f; // 0.01 to 0.02
    float tempJittered = temperature + jitter;
    char payload[16];
    snprintf(payload, sizeof(payload), "%.2f", tempJittered);
    publish(topic, payload);
}

// Publish BME280 humidity value to MQTT (with config checks)
void MqttManager::publishBME280Humidity(float humidity) {
    cJSON* config = systemManager.getConfigManager().getRoot();
    cJSON* wifiModeItem = cJSON_GetObjectItemCaseSensitive(config, "wifi_mode");
    cJSON* mqttEnabledItem = cJSON_GetObjectItemCaseSensitive(config, "mqtt_enabled");
    bool mqttEnabled = false;
    if (mqttEnabledItem) {
        if (cJSON_IsBool(mqttEnabledItem)) {
            mqttEnabled = cJSON_IsTrue(mqttEnabledItem);
        } else if (cJSON_IsNumber(mqttEnabledItem)) {
            mqttEnabled = mqttEnabledItem->valueint != 0;
        } else if (cJSON_IsString(mqttEnabledItem)) {
            std::string val = mqttEnabledItem->valuestring;
            mqttEnabled = (val == "true" || val == "1" || val == "yes" || val == "on");
        }
    }
    std::string wifiMode = wifiModeItem && cJSON_IsString(wifiModeItem) ? wifiModeItem->valuestring : "client";
    if (mqttEnabled && (wifiMode == "client" || wifiMode == "wifi")) {
        char topic[128];
        snprintf(topic, sizeof(topic), "homeassistant/%s/bme280_humidity/state", deviceName.c_str());
        // Add random jitter between 0.01 and 0.02
        float jitter = ((float)random(10, 21)) / 100.0f;
        jitter = jitter / 10.0f;
        float humJittered = humidity + jitter;
        char payload[16];
        snprintf(payload, sizeof(payload), "%.2f", humJittered);
        publish(topic, payload);
    }
}

// Publish BME280 pressure value to MQTT (with config checks)
void MqttManager::publishBME280Pressure(float pressure) {
    cJSON* config = systemManager.getConfigManager().getRoot();
    cJSON* wifiModeItem = cJSON_GetObjectItemCaseSensitive(config, "wifi_mode");
    cJSON* mqttEnabledItem = cJSON_GetObjectItemCaseSensitive(config, "mqtt_enabled");
    bool mqttEnabled = false;
    if (mqttEnabledItem) {
        if (cJSON_IsBool(mqttEnabledItem)) {
            mqttEnabled = cJSON_IsTrue(mqttEnabledItem);
        } else if (cJSON_IsNumber(mqttEnabledItem)) {
            mqttEnabled = mqttEnabledItem->valueint != 0;
        } else if (cJSON_IsString(mqttEnabledItem)) {
            std::string val = mqttEnabledItem->valuestring;
            mqttEnabled = (val == "true" || val == "1" || val == "yes" || val == "on");
        }
    }
    std::string wifiMode = wifiModeItem && cJSON_IsString(wifiModeItem) ? wifiModeItem->valuestring : "client";
    if (mqttEnabled && (wifiMode == "client" || wifiMode == "wifi")) {
        char topic[128];
        snprintf(topic, sizeof(topic), "homeassistant/%s/bme280_pressure/state", deviceName.c_str());
        // Add random jitter between 0.01 and 0.02
        float jitter = ((float)random(10, 21)) / 100.0f;
        jitter = jitter / 10.0f;
        float pressureJittered = pressure + jitter;
        char payload[16];
        snprintf(payload, sizeof(payload), "%.2f", pressureJittered);
        publish(topic, payload);
    }
}

// Publish BME280 heat index value to MQTT (with config checks)
void MqttManager::publishBME280HeatIndex(float heatIndex) {
    cJSON* config = systemManager.getConfigManager().getRoot();
    cJSON* wifiModeItem = cJSON_GetObjectItemCaseSensitive(config, "wifi_mode");
    cJSON* mqttEnabledItem = cJSON_GetObjectItemCaseSensitive(config, "mqtt_enabled");
    bool mqttEnabled = false;
    if (mqttEnabledItem) {
        if (cJSON_IsBool(mqttEnabledItem)) {
            mqttEnabled = cJSON_IsTrue(mqttEnabledItem);
        } else if (cJSON_IsNumber(mqttEnabledItem)) {
            mqttEnabled = mqttEnabledItem->valueint != 0;
        } else if (cJSON_IsString(mqttEnabledItem)) {
            std::string val = mqttEnabledItem->valuestring;
            mqttEnabled = (val == "true" || val == "1" || val == "yes" || val == "on");
        }
    }
    std::string wifiMode = wifiModeItem && cJSON_IsString(wifiModeItem) ? wifiModeItem->valuestring : "client";
    if (mqttEnabled && (wifiMode == "client" || wifiMode == "wifi")) {
        char topic[128];
        snprintf(topic, sizeof(topic), "homeassistant/%s/bme280_heat_index/state", deviceName.c_str());
        // Add random jitter between 0.01 and 0.02
        float jitter = ((float)random(10, 21)) / 100.0f;
        jitter = jitter / 10.0f;
        float heatIndexJittered = heatIndex + jitter;
        char payload[16];
        snprintf(payload, sizeof(payload), "%.2f", heatIndexJittered);
        publish(topic, payload);
    }
}

// Publish BME280 dew point value to MQTT (with config checks)
void MqttManager::publishBME280DewPoint(float dewPoint) {
    cJSON* config = systemManager.getConfigManager().getRoot();
    cJSON* wifiModeItem = cJSON_GetObjectItemCaseSensitive(config, "wifi_mode");
    cJSON* mqttEnabledItem = cJSON_GetObjectItemCaseSensitive(config, "mqtt_enabled");
    bool mqttEnabled = false;
    if (mqttEnabledItem) {
        if (cJSON_IsBool(mqttEnabledItem)) {
            mqttEnabled = cJSON_IsTrue(mqttEnabledItem);
        } else if (cJSON_IsNumber(mqttEnabledItem)) {
            mqttEnabled = mqttEnabledItem->valueint != 0;
        } else if (cJSON_IsString(mqttEnabledItem)) {
            std::string val = mqttEnabledItem->valuestring;
            mqttEnabled = (val == "true" || val == "1" || val == "yes" || val == "on");
        }
    }
    std::string wifiMode = wifiModeItem && cJSON_IsString(wifiModeItem) ? wifiModeItem->valuestring : "client";
    if (mqttEnabled && (wifiMode == "client" || wifiMode == "wifi")) {
        char topic[128];
        snprintf(topic, sizeof(topic), "homeassistant/%s/bme280_dew_point/state", deviceName.c_str());
        // Add random jitter between 0.01 and 0.02
        float jitter = ((float)random(10, 21)) / 100.0f;
        jitter = jitter / 10.0f;
        float dewPointJittered = dewPoint + jitter;
        char payload[16];
        snprintf(payload, sizeof(payload), "%.2f", dewPointJittered);
        publish(topic, payload);
    }
}

// Publish Home Assistant MQTT Discovery for BME280 humidity sensor
void MqttManager::publishDiscoveryForBME280Humidity() {
    char state_topic[128], availability_topic[128], topic[128], unique_id[128];
    snprintf(state_topic, sizeof(state_topic), "homeassistant/%s/bme280_humidity/state", deviceName.c_str());
    snprintf(availability_topic, sizeof(availability_topic), "homeassistant/esp32/%s/availability", deviceName.c_str());
    snprintf(topic, sizeof(topic), "homeassistant/sensor/%s_bme280_humidity/config", deviceName.c_str());
    snprintf(unique_id, sizeof(unique_id), "%s_bme280_humidity", deviceName.c_str());
    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "name", "BME280 Humidity");
    cJSON_AddStringToObject(root, "state_topic", state_topic);
    cJSON_AddStringToObject(root, "unit_of_measurement", "%");
    cJSON_AddStringToObject(root, "device_class", "humidity");
    cJSON_AddStringToObject(root, "availability_topic", availability_topic);
    cJSON_AddStringToObject(root, "payload_available", "online");
    cJSON_AddStringToObject(root, "payload_not_available", "offline");
    cJSON_AddStringToObject(root, "unique_id", unique_id);
    cJSON* device = cJSON_CreateObject();
    cJSON_AddItemToObject(root, "device", device);
    cJSON_AddItemToArray(cJSON_AddArrayToObject(device, "identifiers"), cJSON_CreateString(deviceName.c_str()));
    cJSON_AddStringToObject(device, "manufacturer", "Binghe");
    cJSON_AddStringToObject(device, "model", "LC-Relay-ESP32-4R-A2");
    cJSON_AddStringToObject(device, "name", deviceName.c_str());
    publish(topic, root);
}

// Publish Home Assistant MQTT Discovery for BME280 pressure sensor
void MqttManager::publishDiscoveryForBME280Pressure() {
    char state_topic[128], availability_topic[128], topic[128], unique_id[128];
    snprintf(state_topic, sizeof(state_topic), "homeassistant/%s/bme280_pressure/state", deviceName.c_str());
    snprintf(availability_topic, sizeof(availability_topic), "homeassistant/esp32/%s/availability", deviceName.c_str());
    snprintf(topic, sizeof(topic), "homeassistant/sensor/%s_bme280_pressure/config", deviceName.c_str());
    snprintf(unique_id, sizeof(unique_id), "%s_bme280_pressure", deviceName.c_str());
    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "name", "BME280 Pressure");
    cJSON_AddStringToObject(root, "state_topic", state_topic);
    cJSON_AddStringToObject(root, "unit_of_measurement", "hPa");
    cJSON_AddStringToObject(root, "device_class", "pressure");
    cJSON_AddStringToObject(root, "availability_topic", availability_topic);
    cJSON_AddStringToObject(root, "payload_available", "online");
    cJSON_AddStringToObject(root, "payload_not_available", "offline");
    cJSON_AddStringToObject(root, "unique_id", unique_id);
    cJSON* device = cJSON_CreateObject();
    cJSON_AddItemToObject(root, "device", device);
    cJSON_AddItemToArray(cJSON_AddArrayToObject(device, "identifiers"), cJSON_CreateString(deviceName.c_str()));
    cJSON_AddStringToObject(device, "manufacturer", "Binghe");
    cJSON_AddStringToObject(device, "model", "LC-Relay-ESP32-4R-A2");
    cJSON_AddStringToObject(device, "name", deviceName.c_str());
    publish(topic, root);
}

// Publish Home Assistant MQTT Discovery for BME280 heat index sensor
void MqttManager::publishDiscoveryForBME280HeatIndex() {
    char state_topic[128], availability_topic[128], topic[128], unique_id[128];
    snprintf(state_topic, sizeof(state_topic), "homeassistant/%s/bme280_heat_index/state", deviceName.c_str());
    snprintf(availability_topic, sizeof(availability_topic), "homeassistant/esp32/%s/availability", deviceName.c_str());
    snprintf(topic, sizeof(topic), "homeassistant/sensor/%s_bme280_heat_index/config", deviceName.c_str());
    snprintf(unique_id, sizeof(unique_id), "%s_bme280_heat_index", deviceName.c_str());
    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "name", "BME280 Heat Index");
    cJSON_AddStringToObject(root, "state_topic", state_topic);
    cJSON_AddStringToObject(root, "unit_of_measurement", "°C");
    cJSON_AddStringToObject(root, "device_class", "temperature");
    cJSON_AddStringToObject(root, "availability_topic", availability_topic);
    cJSON_AddStringToObject(root, "payload_available", "online");
    cJSON_AddStringToObject(root, "payload_not_available", "offline");
    cJSON_AddStringToObject(root, "unique_id", unique_id);
    cJSON* device = cJSON_CreateObject();
    cJSON_AddItemToObject(root, "device", device);
    cJSON_AddItemToArray(cJSON_AddArrayToObject(device, "identifiers"), cJSON_CreateString(deviceName.c_str()));
    cJSON_AddStringToObject(device, "manufacturer", "Binghe");
    cJSON_AddStringToObject(device, "model", "LC-Relay-ESP32-4R-A2");
    cJSON_AddStringToObject(device, "name", deviceName.c_str());
    publish(topic, root);
}

// Publish Home Assistant MQTT Discovery for BME280 dew point sensor
void MqttManager::publishDiscoveryForBME280DewPoint() {
    char state_topic[128], availability_topic[128], topic[128], unique_id[128];
    snprintf(state_topic, sizeof(state_topic), "homeassistant/%s/bme280_dew_point/state", deviceName.c_str());
    snprintf(availability_topic, sizeof(availability_topic), "homeassistant/esp32/%s/availability", deviceName.c_str());
    snprintf(topic, sizeof(topic), "homeassistant/sensor/%s_bme280_dew_point/config", deviceName.c_str());
    snprintf(unique_id, sizeof(unique_id), "%s_bme280_dew_point", deviceName.c_str());
    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "name", "BME280 Dew Point");
    cJSON_AddStringToObject(root, "state_topic", state_topic);
    cJSON_AddStringToObject(root, "unit_of_measurement", "°C");
    cJSON_AddStringToObject(root, "device_class", "temperature");
    cJSON_AddStringToObject(root, "availability_topic", availability_topic);
    cJSON_AddStringToObject(root, "payload_available", "online");
    cJSON_AddStringToObject(root, "payload_not_available", "offline");
    cJSON_AddStringToObject(root, "unique_id", unique_id);
    cJSON* device = cJSON_CreateObject();
    cJSON_AddItemToObject(root, "device", device);
    cJSON_AddItemToArray(cJSON_AddArrayToObject(device, "identifiers"), cJSON_CreateString(deviceName.c_str()));
    cJSON_AddStringToObject(device, "manufacturer", "Binghe");
    cJSON_AddStringToObject(device, "model", "LC-Relay-ESP32-4R-A2");
    cJSON_AddStringToObject(device, "name", deviceName.c_str());
    publish(topic, root);
}

// Publish Home Assistant MQTT Discovery for Soil Moisture sensor
void MqttManager::publishDiscoveryForSoilMoisture() {
    char state_topic[128], availability_topic[128], topic[128], unique_id[128];
    snprintf(state_topic, sizeof(state_topic), "homeassistant/%s/soil_moisture/state", deviceName.c_str());
    snprintf(availability_topic, sizeof(availability_topic), "homeassistant/esp32/%s/availability", deviceName.c_str());
    snprintf(topic, sizeof(topic), "homeassistant/sensor/%s_soil_moisture/config", deviceName.c_str());
    snprintf(unique_id, sizeof(unique_id), "%s_soil_moisture", deviceName.c_str());
    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "name", "Soil Moisture");
    cJSON_AddStringToObject(root, "state_topic", state_topic);
    cJSON_AddStringToObject(root, "unit_of_measurement", "%");
    cJSON_AddStringToObject(root, "device_class", "moisture");
    cJSON_AddStringToObject(root, "availability_topic", availability_topic);
    cJSON_AddStringToObject(root, "payload_available", "online");
    cJSON_AddStringToObject(root, "payload_not_available", "offline");
    cJSON_AddStringToObject(root, "unique_id", unique_id);
    cJSON* device = cJSON_CreateObject();
    cJSON_AddItemToObject(root, "device", device);
    cJSON_AddItemToArray(cJSON_AddArrayToObject(device, "identifiers"), cJSON_CreateString(deviceName.c_str()));
    cJSON_AddStringToObject(device, "manufacturer", "Binghe");
    cJSON_AddStringToObject(device, "model", "LC-Relay-ESP32-4R-A2");
    cJSON_AddStringToObject(device, "name", deviceName.c_str());
    publish(topic, root);
}

// Publish Home Assistant MQTT Discovery for MQ135 Air Quality Rating
void MqttManager::publishDiscoveryForMQ135AirQuality() {
    char state_topic[128], availability_topic[128], topic[128], unique_id[128];
    snprintf(state_topic, sizeof(state_topic), "homeassistant/%s/mq135_air_quality/state", deviceName.c_str());
    snprintf(availability_topic, sizeof(availability_topic), "homeassistant/esp32/%s/availability", deviceName.c_str());
    snprintf(topic, sizeof(topic), "homeassistant/sensor/%s_mq135_air_quality/config", deviceName.c_str());
    snprintf(unique_id, sizeof(unique_id), "%s_mq135_air_quality", deviceName.c_str());
    char attributes_topic[128];
    snprintf(attributes_topic, sizeof(attributes_topic), "homeassistant/%s/mq135_air_quality/attributes", deviceName.c_str());
    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "name", "MQ135 Air Quality Rating");
    cJSON_AddStringToObject(root, "state_topic", state_topic);
    cJSON_AddStringToObject(root, "json_attributes_topic", attributes_topic);
    cJSON_AddStringToObject(root, "availability_topic", availability_topic);
    cJSON_AddStringToObject(root, "payload_available", "online");
    cJSON_AddStringToObject(root, "payload_not_available", "offline");
    cJSON_AddStringToObject(root, "unique_id", unique_id);
    cJSON_AddStringToObject(root, "device_class", "aqi");
    cJSON* device = cJSON_CreateObject();
    cJSON_AddItemToObject(root, "device", device);
    cJSON_AddItemToArray(cJSON_AddArrayToObject(device, "identifiers"), cJSON_CreateString(deviceName.c_str()));
    cJSON_AddStringToObject(device, "manufacturer", "Binghe");
    cJSON_AddStringToObject(device, "model", "LC-Relay-ESP32-4R-A2");
    cJSON_AddStringToObject(device, "name", deviceName.c_str());
    publish(topic, root);
}

// Publish Soil Moisture value to MQTT (with config checks)
void MqttManager::publishSoilMoisture(float percent) {
    cJSON* config = systemManager.getConfigManager().getRoot();
    cJSON* wifiModeItem = cJSON_GetObjectItemCaseSensitive(config, "wifi_mode");
    cJSON* mqttEnabledItem = cJSON_GetObjectItemCaseSensitive(config, "mqtt_enabled");
    bool mqttEnabled = false;
    if (mqttEnabledItem) {
        if (cJSON_IsBool(mqttEnabledItem)) {
            mqttEnabled = cJSON_IsTrue(mqttEnabledItem);
        } else if (cJSON_IsNumber(mqttEnabledItem)) {
            mqttEnabled = mqttEnabledItem->valueint != 0;
        } else if (cJSON_IsString(mqttEnabledItem)) {
            std::string val = mqttEnabledItem->valuestring;
            mqttEnabled = (val == "true" || val == "1" || val == "yes" || val == "on");
        }
    }
    std::string wifiMode = wifiModeItem && cJSON_IsString(wifiModeItem) ? wifiModeItem->valuestring : "client";
    if (mqttEnabled && (wifiMode == "client" || wifiMode == "wifi")) {
        // Always fetch latest single value from sensor object and publish
        SoilMoistureSensor* soil = systemManager.getDeviceManager().getSoilMoistureSensor();
        if (soil) {
            float singlePercent = soil->getLastPercent();
            // Add random jitter between 0.01 and 0.02
            float jitter = ((float)random(10, 21)) / 100.0f;
            jitter = jitter / 10.0f;
            float percentJittered = singlePercent + jitter;
            char topic[128];
            snprintf(topic, sizeof(topic), "homeassistant/%s/soil_moisture/state", deviceName.c_str());
            char payload[16];
            snprintf(payload, sizeof(payload), "%.2f", percentJittered);
            publish(topic, payload);
        }
    }
}

// Publish MQ135 Air Quality Rating to MQTT (with config checks)
void MqttManager::publishMQ135AirQuality() {
    extern MQ135Sensor mq135Sensor;
    cJSON* config = systemManager.getConfigManager().getRoot();
    cJSON* wifiModeItem = cJSON_GetObjectItemCaseSensitive(config, "wifi_mode");
    cJSON* mqttEnabledItem = cJSON_GetObjectItemCaseSensitive(config, "mqtt_enabled");
    bool mqttEnabled = false;
    if (mqttEnabledItem) {
        if (cJSON_IsBool(mqttEnabledItem)) {
            mqttEnabled = cJSON_IsTrue(mqttEnabledItem);
        } else if (cJSON_IsNumber(mqttEnabledItem)) {
            mqttEnabled = mqttEnabledItem->valueint != 0;
        } else if (cJSON_IsString(mqttEnabledItem)) {
            std::string val = mqttEnabledItem->valuestring;
            mqttEnabled = (val == "true" || val == "1" || val == "yes" || val == "on");
        }
    }
    std::string wifiMode = wifiModeItem && cJSON_IsString(wifiModeItem) ? wifiModeItem->valuestring : "client";
    if (mqttEnabled && (wifiMode == "client" || wifiMode == "wifi")) {
        float voltage = mq135Sensor.getLastReading().avgVoltage;
        const char* rating = MQ135Sensor::getAirQualityLabel(voltage);
        int aqi = MQ135Sensor::getAirQualityIndexFromLabel(rating);
        char state_topic[128];
        snprintf(state_topic, sizeof(state_topic), "homeassistant/%s/mq135_air_quality/state", deviceName.c_str());
        char attributes_topic[128];
        snprintf(attributes_topic, sizeof(attributes_topic), "homeassistant/%s/mq135_air_quality/attributes", deviceName.c_str());
        // Publish numeric AQI value to state topic
        char aqi_str[8];
        snprintf(aqi_str, sizeof(aqi_str), "%d", aqi);
        publish(state_topic, aqi_str);
        // Publish label as attribute to attributes topic
        cJSON* attr_payload = cJSON_CreateObject();
        cJSON_AddStringToObject(attr_payload, "label", rating);
        publish(attributes_topic, attr_payload);
    }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
    std::string payloadStr((char*)payload, length);
    // Parse relay index from topic
    int relayIndex = -1;
    std::string topicStr(topic);
    size_t relayPos = topicStr.find("/relay");
    size_t setPos = topicStr.find("/set");
    if (relayPos != std::string::npos && setPos != std::string::npos) {
        size_t idxStart = relayPos + 6;
        size_t idxEnd = topicStr.find("/", idxStart);
        if (idxEnd == std::string::npos) idxEnd = setPos;
        std::string idxStr = topicStr.substr(idxStart, idxEnd - idxStart);
        relayIndex = atoi(idxStr.c_str()) - 1;
    }
    if (relayIndex >= 0) {
        Serial.printf("[MqttManager] Received command for relay %d: %s\n", relayIndex, payloadStr.c_str());
        extern MqttManager mqttManager;
        mqttManager.handleRelayCommand(relayIndex, payloadStr);
    }
}

void MqttManager::begin(const std::string& deviceName_, const std::vector<std::string>& relayNames_, ConfigManager* configManager) {
    Serial.println("[DEBUG] MqttManager::begin called");
    deviceName = deviceName_;
    relayNames = relayNames_;
    initialized = false;
    // Robustly pull MQTT config from ConfigManager using cJSON
    cJSON* root = configManager->getRoot();
    cJSON* serverItem = cJSON_GetObjectItemCaseSensitive(root, "mqtt_server");
    cJSON* portItem = cJSON_GetObjectItemCaseSensitive(root, "mqtt_port");
    cJSON* userItem = cJSON_GetObjectItemCaseSensitive(root, "mqtt_username");
    cJSON* passItem = cJSON_GetObjectItemCaseSensitive(root, "mqtt_password");

    mqttServer = (serverItem && cJSON_IsString(serverItem)) ? serverItem->valuestring : "";
    if (portItem) {
        if (cJSON_IsNumber(portItem)) {
            mqttPort = portItem->valueint;
        } else if (cJSON_IsString(portItem)) {
            mqttPort = atoi(portItem->valuestring);
        } else {
            mqttPort = 1883;
        }
    } else {
        mqttPort = 1883;
    }
    mqttUser = (userItem && cJSON_IsString(userItem)) ? userItem->valuestring : "";
    mqttPass = (passItem && cJSON_IsString(passItem)) ? passItem->valuestring : "";

    Serial.printf("[MqttManager] Config: server=%s, port=%d, user=%s, pass=%s\n", mqttServer.c_str(), mqttPort, mqttUser.c_str(), mqttPass.c_str());
    mqttClient.setServer(mqttServer.c_str(), mqttPort);
    mqttClient.setCallback(mqttCallback);
}

void MqttManager::setInitialized(bool init) {
    initialized = init;
    if (initialized) {
        Serial.println("[MqttManager] MQTT Manager initialized and ready.");
        // Ensure MQTT connection before publishing availability
        if (!mqttClient.connected()) {
            Serial.printf("[MQTT] Not connected, attempting to connect to %s:%d as user '%s'\n", mqttServer.c_str(), mqttPort, mqttUser.c_str());
            if (mqttClient.connect(deviceName.c_str(), mqttUser.c_str(), mqttPass.c_str())) {
                Serial.println("[MQTT] Connected to broker.");
            } else {
                Serial.println("[MQTT] Connection to broker failed!");
                return;
            }
        }
        // Now publish 'online' to availability topic for Home Assistant with retain=true
        char availTopic[128];
        snprintf(availTopic, sizeof(availTopic), "homeassistant/esp32/%s/availability", deviceName.c_str());
        bool availResult = mqttClient.publish(availTopic, "online", true);
        Serial.printf("[MQTT] Published availability topic '%s': online (retain=true)\n", availTopic);
        Serial.printf("[MQTT] Publish result: %s\n", availResult ? "success" : "fail");

        // Publish config topics for Home Assistant with retain=true
        publishDiscovery();
        // Publish BME280 temperature sensor discovery for Home Assistant
        publishDiscoveryForBME280Temperature();
        publishDiscoveryForBME280Humidity();
        publishDiscoveryForBME280Pressure();
        publishDiscoveryForBME280HeatIndex();
        publishDiscoveryForBME280DewPoint();

        // Publish Soil Moisture sensor discovery for Home Assistant
        publishDiscoveryForSoilMoisture();

        // Publish MQ135 Air Quality Rating sensor discovery for Home Assistant
        publishDiscoveryForMQ135AirQuality();

        // Wait 1 second to ensure Home Assistant processes config before state
        {
            unsigned long start = millis();
            while (millis() - start < 1000) {
                vTaskDelay(1); // Yield to RTOS, non-blocking
            }
        }

        // Publish last BME280 temperature reading to Home Assistant
        BME280Device* bme = systemManager.getDeviceManager().getBME280Device();
        if (bme) {
            BME280Reading r = bme->getLastReading();
            if (r.valid) {
                publishBME280Temperature(r.avgTemperature);
                publishBME280Humidity(r.avgHumidity);
                publishBME280Pressure(r.avgPressure);
                publishBME280HeatIndex(r.avgHeatIndex);
                publishBME280DewPoint(r.avgDewPoint);
                Serial.printf("[MqttManager] Initial BME280 temperature published to Home Assistant: %.2fC\n", r.avgTemperature);
            }
        }

        // Publish last soil moisture reading to Home Assistant
        SoilMoistureSensor* soil = systemManager.getDeviceManager().getSoilMoistureSensor();
        if (soil) {
            float percent = soil->getLastPercent();
            // Always publish initial value after discovery, even if 0
            publishSoilMoisture(percent);
            Serial.printf("[MqttManager] Initial soil moisture published to Home Assistant: %.2f%%\n", percent);
        }

        // Publish initial MQ135 Air Quality Rating to Home Assistant
        publishMQ135AirQuality();

        // Publish initial relay states so Home Assistant sees them as available
        extern RelayController relayController;
        for (size_t i = 0; i < relayNames.size(); ++i) {
            bool state = relayController.getRelayState((int)i);
            publishRelayState((int)i, state);
        }
        // Subscribe to relay command topics
        for (size_t i = 0; i < relayNames.size(); ++i) {
            char cmdTopic[128];
            snprintf(cmdTopic, sizeof(cmdTopic), "homeassistant/%s/relay%d/set", deviceName.c_str(), (int)i + 1);
            mqttClient.subscribe(cmdTopic);
            Serial.printf("[MqttManager] Subscribed to command topic: %s\n", cmdTopic);
        }
    }
}

bool MqttManager::isInitialized() const {
    return initialized;
}

void MqttManager::publishDiscovery() {
    for (size_t i = 0; i < relayNames.size(); ++i) {
        publishDiscoveryForRelay(static_cast<int>(i));
    }
}

void MqttManager::publishDiscoveryForRelay(int relayIndex) {
    char state_topic[128], command_topic[128], unique_id[128], availability_topic[128], topic[128];
    snprintf(state_topic, sizeof(state_topic), "homeassistant/%s/relay%d/state", deviceName.c_str(), relayIndex + 1);
    snprintf(command_topic, sizeof(command_topic), "homeassistant/%s/relay%d/set", deviceName.c_str(), relayIndex + 1);
    snprintf(unique_id, sizeof(unique_id), "%s_relay_%d", deviceName.c_str(), relayIndex + 1);
    snprintf(availability_topic, sizeof(availability_topic), "homeassistant/esp32/%s/availability", deviceName.c_str());
    snprintf(topic, sizeof(topic), "homeassistant/switch/%s/config", unique_id);
    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "name", relayNames[relayIndex].c_str());
    cJSON_AddStringToObject(root, "state_topic", state_topic);
    cJSON_AddStringToObject(root, "command_topic", command_topic);
    cJSON_AddStringToObject(root, "availability_topic", availability_topic);
    cJSON_AddStringToObject(root, "payload_on", "ON");
    cJSON_AddStringToObject(root, "payload_off", "OFF");
    cJSON_AddStringToObject(root, "unique_id", unique_id);
    cJSON_AddStringToObject(root, "device_class", "outlet");
    cJSON_AddStringToObject(root, "payload_available", "online");
    cJSON_AddStringToObject(root, "payload_not_available", "offline");
    cJSON* device = cJSON_CreateObject();
    cJSON_AddItemToObject(root, "device", device);
    cJSON_AddItemToArray(cJSON_AddArrayToObject(device, "identifiers"), cJSON_CreateString(deviceName.c_str()));
    cJSON_AddStringToObject(device, "manufacturer", "Binghe");
    cJSON_AddStringToObject(device, "model", "LC-Relay-ESP32-4R-A2");
    cJSON_AddStringToObject(device, "name", deviceName.c_str());
    publish(topic, root);
}

void MqttManager::publishRelayState(int relayIndex, bool state) {
    // Check config before publishing relay state
    cJSON* config = systemManager.getConfigManager().getRoot();
    cJSON* wifiModeItem = cJSON_GetObjectItemCaseSensitive(config, "wifi_mode");
    cJSON* mqttEnabledItem = cJSON_GetObjectItemCaseSensitive(config, "mqtt_enabled");
    bool mqttEnabled = false;
    if (mqttEnabledItem) {
        if (cJSON_IsBool(mqttEnabledItem)) {
            mqttEnabled = cJSON_IsTrue(mqttEnabledItem);
        } else if (cJSON_IsNumber(mqttEnabledItem)) {
            mqttEnabled = mqttEnabledItem->valueint != 0;
        } else if (cJSON_IsString(mqttEnabledItem)) {
            std::string val = mqttEnabledItem->valuestring;
            mqttEnabled = (val == "true" || val == "1" || val == "yes" || val == "on");
        }
    }
    std::string wifiMode = wifiModeItem && cJSON_IsString(wifiModeItem) ? wifiModeItem->valuestring : "client";
    if (mqttEnabled && (wifiMode == "client" || wifiMode == "wifi")) {
        char topic[128];
        snprintf(topic, sizeof(topic), "homeassistant/%s/relay%d/state", deviceName.c_str(), relayIndex + 1);
        publish(topic, state ? "ON" : "OFF");
    }
    // Only publish to Home Assistant topic format. Remove legacy topic publishing.
}

void MqttManager::handleRelayCommand(int relayIndex, const std::string& payload) {
    // Check config before handling relay command and publishing state
    cJSON* config = systemManager.getConfigManager().getRoot();
    cJSON* wifiModeItem = cJSON_GetObjectItemCaseSensitive(config, "wifi_mode");
    cJSON* mqttEnabledItem = cJSON_GetObjectItemCaseSensitive(config, "mqtt_enabled");
    bool mqttEnabled = false;
    if (mqttEnabledItem) {
        if (cJSON_IsBool(mqttEnabledItem)) {
            mqttEnabled = cJSON_IsTrue(mqttEnabledItem);
        } else if (cJSON_IsNumber(mqttEnabledItem)) {
            mqttEnabled = mqttEnabledItem->valueint != 0;
        } else if (cJSON_IsString(mqttEnabledItem)) {
            std::string val = mqttEnabledItem->valuestring;
            mqttEnabled = (val == "true" || val == "1" || val == "yes" || val == "on");
        }
    }
    std::string wifiMode = wifiModeItem && cJSON_IsString(wifiModeItem) ? wifiModeItem->valuestring : "client";
    if (mqttEnabled && (wifiMode == "client" || wifiMode == "wifi")) {
        bool newState = (payload == "ON");
        // Relay hardware control: set relay state using global relayController instance
        extern RelayController relayController;
        relayController.setRelayMode(relayIndex, newState ? Relay::ON : Relay::OFF);
        publishRelayState(relayIndex, newState);
        Serial.printf("[MqttManager] Relay %d set to %s\n", relayIndex, newState ? "ON" : "OFF");
    }
}

void MqttManager::publish(const std::string& topic, cJSON* payload) {
    // Publish to MQTT broker
// ...existing code...
// Overload publish for raw string payloads (for relay state)

    if (!mqttClient.connected()) {
        Serial.printf("[MQTT] Not connected, attempting to connect to %s:%d as user '%s'\n", mqttServer.c_str(), mqttPort, mqttUser.c_str());
        if (mqttClient.connect(deviceName.c_str(), mqttUser.c_str(), mqttPass.c_str())) {
            Serial.println("[MQTT] Connected to broker.");
        } else {
            Serial.println("[MQTT] Connection to broker failed!");
            cJSON_Delete(payload);
            return;
        }
    }
    char* msg = cJSON_PrintUnformatted(payload);
    bool retain = false;
    // Retain config, state, and availability topic messages for Home Assistant
    if (topic.find("/status") != std::string::npos ||
        topic.find("/config") != std::string::npos ||
        topic.find("/state") != std::string::npos) {
        retain = true;
    }
    bool pubResult = mqttClient.publish(topic.c_str(), msg, retain);
    Serial.printf("[MQTT] Publishing to topic '%s': %s (retain=%s)\n", topic.c_str(), msg, retain ? "true" : "false");
    Serial.printf("[MQTT] Publish result: %s\n", pubResult ? "success" : "fail");
    free(msg); // Use free, not cJSON_free
    // Only delete if payload is object or array
    if (cJSON_IsObject(payload) || cJSON_IsArray(payload)) {
        cJSON_Delete(payload);
    }
}

void MqttManager::loop() {
    mqttClient.loop();
}

// Implementation of publish for raw string payloads (for relay state)
void MqttManager::publish(const std::string& topic, const char* payload) {
    if (!mqttClient.connected()) {
        Serial.printf("[MQTT] Not connected, attempting to connect to %s:%d as user '%s'\n", mqttServer.c_str(), mqttPort, mqttUser.c_str());
        if (mqttClient.connect(deviceName.c_str(), mqttUser.c_str(), mqttPass.c_str())) {
            Serial.println("[MQTT] Connected to broker.");
        } else {
            Serial.println("[MQTT] Connection to broker failed!");
            return;
        }
    }
    bool retain = false;
    if (topic.find("/status") != std::string::npos ||
        topic.find("/config") != std::string::npos ||
        topic.find("/state") != std::string::npos) {
        retain = true;
    }
    bool pubResult = mqttClient.publish(topic.c_str(), payload, retain);
    Serial.printf("[MQTT] Publishing to topic '%s': %s (retain=%s)\n", topic.c_str(), payload, retain ? "true" : "false");
    Serial.printf("[MQTT] Publish result: %s\n", pubResult ? "success" : "fail");
}

// Removed: publishDiscoveryForSoilMoistureAvg()
