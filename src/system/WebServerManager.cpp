#include "system/WebServerManager.h"
#include <Arduino.h>


#include <LittleFS.h>
#include "devices/LedDevice.h"

extern LedDevice led;
extern SystemManager systemManager;

WebServerManager::WebServerManager(DashboardManager* dashMgr, DiagnosticManager* diagMgr)
    : dashboardManager(dashMgr), diagnosticManager(diagMgr), server(nullptr) {}

void WebServerManager::begin() {
    // Create server before registering any routes
    server = new AsyncWebServer(80);
    // MQ135 Air Quality trigger API
    extern MQ135Sensor mq135Sensor;
    server->on("/api/mq135/trigger", HTTP_POST, [](AsyncWebServerRequest* request){}, NULL,
        [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
            extern MQ135Sensor mq135Sensor;
            mq135Sensor.startReading();
            // Set state machine so main loop will process the reading
            extern bool mq135ReadingRequested;
            mq135ReadingRequested = true;
            cJSON* resp = cJSON_CreateObject();
            cJSON_AddStringToObject(resp, "result", "started");
            cJSON_AddStringToObject(resp, "message", "Air quality reading started. Poll /api/status for result.");
            char* respStr = cJSON_PrintUnformatted(resp);
            request->send(200, "application/json", respStr);
            cJSON_free(respStr);
            cJSON_Delete(resp);
        });
    // Soil Moisture trigger API
    extern SoilMoistureSensor soilMoistureSensor;
    server->on("/api/soilmoisture/trigger", HTTP_POST, [](AsyncWebServerRequest* request){}, NULL,
        [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
            extern SoilMoistureSensor soilMoistureSensor;
            soilMoistureSensor.beginStabilisation();
            // Set state machine so main loop will process the reading
            extern bool soilReadingTaken;
            extern int sensorState;
            // SensorState enum: IDLE=0, SOIL_STABILISING=1, ...
            soilReadingTaken = false;
            sensorState = 1; // SOIL_STABILISING
            cJSON* resp = cJSON_CreateObject();
            cJSON_AddStringToObject(resp, "result", "started");
            cJSON_AddStringToObject(resp, "message", "Soil moisture reading started. Poll /api/status for result.");
            char* respStr = cJSON_PrintUnformatted(resp);
            request->send(200, "application/json", respStr);
            cJSON_free(respStr);
            cJSON_Delete(resp);
        });
    // BME280 trigger API
    server->on("/api/bme280/trigger", HTTP_POST, [](AsyncWebServerRequest* request){}, NULL,
        [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
            BME280Device* bme = systemManager.getDeviceManager().getBME280Device();
            cJSON* resp = cJSON_CreateObject();
            if (bme) {
                BME280Reading r = bme->readData();
                cJSON_AddStringToObject(resp, "result", r.valid ? "ok" : "error");
                cJSON_AddNumberToObject(resp, "temperature", r.temperature);
                cJSON_AddNumberToObject(resp, "humidity", r.humidity);
                cJSON_AddNumberToObject(resp, "pressure", r.pressure);
                cJSON_AddNumberToObject(resp, "heat_index", r.heatIndex);
                cJSON_AddNumberToObject(resp, "dew_point", r.dewPoint);
            } else {
                cJSON_AddStringToObject(resp, "result", "error");
            }
            char* respStr = cJSON_PrintUnformatted(resp);
            request->send(200, "application/json", respStr);
            cJSON_free(respStr);
            cJSON_Delete(resp);
        });

    // Relay control API
    extern RelayController relayController;
    RelayController* relayControllerPtr = &relayController;

    // Irrigation trigger API
    extern IrrigationManager irrigationManager;
    server->on("/api/irrigation/trigger", HTTP_POST, [](AsyncWebServerRequest* request){}, NULL,
        [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
            int result = 0;
            irrigationManager.setSkipAirQualityThisRun(true);
            irrigationManager.trigger();
            cJSON* resp = cJSON_CreateObject();
            cJSON_AddStringToObject(resp, "result", "ok");
            char* respStr = cJSON_PrintUnformatted(resp);
            request->send(200, "application/json", respStr);
            cJSON_free(respStr);
            cJSON_Delete(resp);
        });
    server->on("/api/relay", HTTP_POST, [](AsyncWebServerRequest* request){}, NULL,
        [relayControllerPtr](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
            String body = String((char*)data).substring(0, len);
            int relay = -1;
            String command;
            #if defined(ARDUINO_ARCH_ESP32)
            cJSON* root = cJSON_Parse(body.c_str());
            if (root) {
                cJSON* relayIdx = cJSON_GetObjectItem(root, "relay");
                if (relayIdx && cJSON_IsNumber(relayIdx)) {
                    relay = relayIdx->valueint;
                }
                cJSON* cmd = cJSON_GetObjectItem(root, "command");
                if (cmd && cJSON_IsString(cmd)) {
                    command = cmd->valuestring;
                }
                cJSON_Delete(root);
            }
            #endif
            int result = 0;
            if (relay < 0 || relay > 3) {
                result = -2;
            } else if (command == "on") {
                relayControllerPtr->setRelayMode(relay, Relay::ON);
            } else if (command == "off") {
                relayControllerPtr->setRelayMode(relay, Relay::OFF);
            } else if (command == "toggle") {
                relayControllerPtr->toggleRelay(relay);
            } else {
                result = -1;
            }
            cJSON* resp = cJSON_CreateObject();
            cJSON_AddStringToObject(resp, "result", result == 0 ? "ok" : "error");
            cJSON_AddNumberToObject(resp, "relay", relay);
            cJSON_AddStringToObject(resp, "command", command.c_str());
            char* respStr = cJSON_PrintUnformatted(resp);
            request->send(200, "application/json", respStr);
            cJSON_free(respStr);
            cJSON_Delete(resp);
        });
    // ...existing code...
    if (!dashboardManager) {
        if (diagnosticManager) {
            diagnosticManager->log(DiagnosticManager::LOG_ERROR, "WebServer", "DashboardManager not set");
        }
        return;
    }

    // Initialize LittleFS
    initializeLittleFS();

    // API routes
    server->on("/api/status", HTTP_GET, [this](AsyncWebServerRequest* request) {
        handleAPI(request);
    });

    // LED control API
    server->on("/api/led", HTTP_POST, [](AsyncWebServerRequest* request){}, NULL,
        [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
            String body = String((char*)data).substring(0, len);
            String command;
            #if defined(ARDUINO_ARCH_ESP32)
            cJSON* root = cJSON_Parse(body.c_str());
            if (root) {
                cJSON* cmd = cJSON_GetObjectItem(root, "command");
                if (cmd && cJSON_IsString(cmd)) {
                    command = cmd->valuestring;
                }
                cJSON_Delete(root);
            }
            #endif
            int result = 0;
            if (command == "on") {
                led.setMode(LedDevice::ON);
            } else if (command == "off") {
                led.setMode(LedDevice::OFF);
            } else if (command == "toggle") {
                led.setMode(LedDevice::TOGGLE);
            } else if (command == "blink") {
                led.setMode(LedDevice::BLINK);
            } else {
                result = -1;
            }
            cJSON* resp = cJSON_CreateObject();
            cJSON_AddStringToObject(resp, "result", result == 0 ? "ok" : "error");
            cJSON_AddStringToObject(resp, "mode", command.c_str());
            char* respStr = cJSON_PrintUnformatted(resp);
            request->send(200, "application/json", respStr);
            cJSON_free(respStr);
            cJSON_Delete(resp);
        });

    // Static file routes
    server->on("/", HTTP_GET, [this](AsyncWebServerRequest* request) {
        handleRoot(request);
    });

    server->on("/style.css", HTTP_GET, [this](AsyncWebServerRequest* request) {
        handleStaticFile(request);
    });

    server->on("/script.js", HTTP_GET, [this](AsyncWebServerRequest* request) {
        handleStaticFile(request);
    });

    server->on("/index.html", HTTP_GET, [this](AsyncWebServerRequest* request) {
        handleStaticFile(request);
    });
    server->on("/sensors.html", HTTP_GET, [this](AsyncWebServerRequest* request) {
        handleStaticFile(request);
    });
    server->on("/relay.html", HTTP_GET, [this](AsyncWebServerRequest* request) {
        handleStaticFile(request);
    });
    server->on("/relay.js", HTTP_GET, [this](AsyncWebServerRequest* request) {
        handleStaticFile(request);
    });
    server->on("/led.html", HTTP_GET, [this](AsyncWebServerRequest* request) {
        handleStaticFile(request);
    });
    server->on("/led.js", HTTP_GET, [this](AsyncWebServerRequest* request) {
        handleStaticFile(request);
    });
    server->on("/index.js", HTTP_GET, [this](AsyncWebServerRequest* request) {
        handleStaticFile(request);
    });
    server->on("/touch.js", HTTP_GET, [this](AsyncWebServerRequest* request) {
        handleStaticFile(request);
    });
    server->on("/sensors.js", HTTP_GET, [this](AsyncWebServerRequest* request) {
        handleStaticFile(request);
    });
    server->on("/irrigation.js", HTTP_GET, [this](AsyncWebServerRequest* request) {
        handleStaticFile(request);
    });
    server->on("/config.js", HTTP_GET, [this](AsyncWebServerRequest* request) {
        handleStaticFile(request);
    });
    server->on("/touch.html", HTTP_GET, [this](AsyncWebServerRequest* request) {
        handleStaticFile(request);
    });
    server->on("/irrigation.html", HTTP_GET, [this](AsyncWebServerRequest* request) {
        handleStaticFile(request);
    });
    server->on("/config.html", HTTP_GET, [this](AsyncWebServerRequest* request) {
        handleStaticFile(request);
    });
    
    server->onNotFound([this](AsyncWebServerRequest* request) {
        handleNotFound(request);
    });
    
    server->begin();
    running = true;
    
    if (diagnosticManager) {
        diagnosticManager->log(DiagnosticManager::LOG_INFO, "WebServer", "HTTP server started on port 80");
    }
    Serial.println("[WebServer] HTTP server started on port 80");
}

void WebServerManager::handleClient() {
    // AsyncWebServer handles clients automatically, no manual handling needed
}

void WebServerManager::handleRoot(AsyncWebServerRequest* request) {
    if (LittleFS.exists("/index.html")) {
        request->send(LittleFS, "/index.html", "text/html");
    } else {
        request->send(404, "text/plain", "index.html not found in LittleFS");
    }
    
    if (diagnosticManager) {
        diagnosticManager->log(DiagnosticManager::LOG_DEBUG, "WebServer", "Served index page to %s", request->client()->remoteIP().toString().c_str());
    }
}

void WebServerManager::handleAPI(AsyncWebServerRequest* request) {
    if (!dashboardManager) {
        request->send(500, "application/json", "{\"error\":\"Dashboard manager not available\"}");
        return;
    }
    
    String jsonResponse = dashboardManager->getStatusString();
    request->send(200, "application/json", jsonResponse);
    
    if (diagnosticManager) {
        diagnosticManager->log(DiagnosticManager::LOG_DEBUG, "WebServer", "Served API status to %s", request->client()->remoteIP().toString().c_str());
    }
}

void WebServerManager::handleNotFound(AsyncWebServerRequest* request) {
    request->send(404, "text/plain", "Not Found");
}

void WebServerManager::handleStaticFile(AsyncWebServerRequest* request) {
    String path = request->url();
    
    if (LittleFS.exists(path)) {
        String contentType = "text/plain";
        
        if (path.endsWith(".css")) {
            contentType = "text/css";
        } else if (path.endsWith(".js")) {
            contentType = "application/javascript";
        } else if (path.endsWith(".html")) {
            contentType = "text/html";
        }
        
        request->send(LittleFS, path, contentType);
        
        if (diagnosticManager) {
            diagnosticManager->log(DiagnosticManager::LOG_DEBUG, "WebServer", "Served static file %s to %s", 
                                   path.c_str(), request->client()->remoteIP().toString().c_str());
        }
    } else {
        request->send(404, "text/plain", "File not found in LittleFS");
    }
}

void WebServerManager::initializeLittleFS() {
    if (!LittleFS.begin(true)) {
        Serial.println("[WebServer] LittleFS mount failed");
        if (diagnosticManager) {
            diagnosticManager->log(DiagnosticManager::LOG_ERROR, "WebServer", "LittleFS mount failed");
        }
        return;
    }
    
    Serial.println("[WebServer] LittleFS mounted successfully");
    if (diagnosticManager) {
        diagnosticManager->log(DiagnosticManager::LOG_INFO, "WebServer", "LittleFS mounted successfully");
    }
    
    // List files in LittleFS for debugging
    Serial.println("[WebServer] Files in LittleFS:");
    File root = LittleFS.open("/");
    File file = root.openNextFile();
    while (file) {
        Serial.printf("  %s (%d bytes)\n", file.name(), file.size());
        file = root.openNextFile();
    }
}


