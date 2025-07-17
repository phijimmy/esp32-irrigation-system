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
    // ...existing code...
    // Clear Config API
    server->on("/api/clearconfig", HTTP_POST, [](AsyncWebServerRequest* request) {
        bool ok = systemManager.getFileSystemManager().deleteConfigJson();
        cJSON* resp = cJSON_CreateObject();
        if (ok) {
            cJSON_AddStringToObject(resp, "result", "ok");
            cJSON_AddStringToObject(resp, "message", "Config cleared. Reboot to restore defaults.");
        } else {
            cJSON_AddStringToObject(resp, "result", "error");
            cJSON_AddStringToObject(resp, "message", "Failed to clear config.");
        }
        char* respStr = cJSON_PrintUnformatted(resp);
        request->send(200, "application/json", respStr);
        cJSON_free(respStr);
        cJSON_Delete(resp);
    });

    // Restart API (schedule restart after delay)
    server->on("/api/restart", HTTP_POST, [](AsyncWebServerRequest* request) {
        cJSON* resp = cJSON_CreateObject();
        cJSON_AddStringToObject(resp, "result", "ok");
        cJSON_AddStringToObject(resp, "message", "System will restart in 3 seconds.");
        char* respStr = cJSON_PrintUnformatted(resp);
        request->send(200, "application/json", respStr);
        cJSON_free(respStr);
        cJSON_Delete(resp);
        // Schedule restart via SystemManager
        systemManager.scheduleRestart(3000);
    });
    // Config save API
    server->on("/api/config", HTTP_POST, [](AsyncWebServerRequest* request){}, NULL,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
            static String bodyAccum;
            if (index == 0) bodyAccum = "";
            bodyAccum += String((const char*)data, len);
            if (index + len < total) {
                // Wait for more chunks
                return;
            }
            // Now we have the full body
            Serial.print("[ConfigAPI] Raw config POST body: ");
            Serial.println(bodyAccum);
            cJSON* incoming = cJSON_Parse(bodyAccum.c_str());
            if (!incoming) {
                if (diagnosticManager) {
                    diagnosticManager->log(DiagnosticManager::LOG_ERROR, "ConfigAPI", "Failed to parse config JSON!");
                }
                request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
                return;
            }
            // Try to load config from filesystem
            ConfigManager& cfgMgr = systemManager.getConfigManager();
            bool loaded = cfgMgr.load();
            cJSON* current = nullptr;
            if (loaded) {
                current = cfgMgr.getRoot();
            } else if (dashboardManager) {
                // Fallback: get config from status API
                String statusStr = dashboardManager->getStatusString();
                cJSON* status = cJSON_Parse(statusStr.c_str());
                if (status) {
                    cJSON* conf = cJSON_GetObjectItem(status, "config");
                    if (conf) {
                        current = cJSON_Duplicate(conf, 1);
                    }
                    cJSON_Delete(status);
                }
            }
            if (!current) {
                cJSON_Delete(incoming);
                request->send(500, "application/json", "{\"error\":\"Unable to load current config\"}");
                return;
            }
            // Merge incoming into current (overwrite only provided keys)
            cJSON* item = nullptr;
            cJSON_ArrayForEach(item, incoming) {
                const char* key = item->string;
                cJSON* existing = cJSON_GetObjectItem(current, key);
                if (existing) {
                    cJSON_ReplaceItemInObject(current, key, cJSON_Duplicate(item, 1));
                } else {
                    cJSON_AddItemToObject(current, key, cJSON_Duplicate(item, 1));
                }
            }
            // Save merged config
            if (cfgMgr.getRoot() != current) {
                cfgMgr.setRoot(current);
            }
            // Always set force_build_time to false when saving config
            cfgMgr.setBool("force_build_time", false);
            bool ok = cfgMgr.save();
            cJSON_Delete(incoming);
            if (ok) {
                request->send(200, "application/json", "{\"result\":\"ok\"}");
            } else {
                request->send(500, "application/json", "{\"error\":\"Failed to save config\"}");
            }
        });
    // MQ135 Air Quality trigger API
    extern MQ135Sensor mq135Sensor;
    server->on("/api/mq135/trigger", HTTP_POST, [](AsyncWebServerRequest* request){}, NULL,
        [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
            extern MQ135Sensor mq135Sensor;
            extern bool mq135ReadingRequested;
            extern int sensorState;
            // Only start if not already running
            if (sensorState == 0 /* IDLE */) {
                mq135Sensor.startReading();
                mq135ReadingRequested = false;
                // Set state machine to MQ135_WARMUP (3)
                sensorState = 3;
            } else {
                // If busy, just set the request flag and it will be handled when idle
                mq135ReadingRequested = true;
            }
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
                // Print to terminal for manual reading
                if (r.valid) {
                    char timeStr[32] = "";
                    if (r.timestamp.isValid()) {
                        snprintf(timeStr, sizeof(timeStr), "%04d-%02d-%02d %02d:%02d:%02d", r.timestamp.year(), r.timestamp.month(), r.timestamp.day(), r.timestamp.hour(), r.timestamp.minute(), r.timestamp.second());
                    } else {
                        strcpy(timeStr, "N/A");
                    }
                    Serial.printf("[BME280] Manual reading: T=%.2fC, H=%.2f%%, P=%.2fhPa, HI=%.2fC, DP=%.2fC | avgT=%.2fC, avgH=%.2f%%, avgP=%.2fhPa, avgHI=%.2fC, avgDP=%.2fC, time=%s\n",
                        r.temperature, r.humidity, r.pressure, r.heatIndex, r.dewPoint,
                        r.avgTemperature, r.avgHumidity, r.avgPressure, r.avgHeatIndex, r.avgDewPoint, timeStr);
                } else {
                    Serial.println("[BME280] Manual reading: not valid");
                }
            } else {
                cJSON_AddStringToObject(resp, "result", "error");
                Serial.println("[BME280] Manual reading: device not found");
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
            // irrigationManager.setSkipAirQualityThisRun(true); // Removed: no longer needed
            irrigationManager.trigger();
            cJSON* resp = cJSON_CreateObject();
            cJSON_AddStringToObject(resp, "result", "ok");
            char* respStr = cJSON_PrintUnformatted(resp);
            request->send(200, "application/json", respStr);
            cJSON_free(respStr);
            cJSON_Delete(resp);
        });

    // Irrigation Water Now API
    server->on("/api/irrigation/waternow", HTTP_POST, [](AsyncWebServerRequest* request){}, NULL,
        [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
            extern IrrigationManager irrigationManager;
            irrigationManager.waterNow();
            cJSON* resp = cJSON_CreateObject();
            cJSON_AddStringToObject(resp, "result", "ok");
            char* respStr = cJSON_PrintUnformatted(resp);
            request->send(200, "application/json", respStr);
            cJSON_free(respStr);
            cJSON_Delete(resp);
        });

    // Irrigation Stop Now API (grouped with other irrigation endpoints)
    server->on("/api/irrigation/stop", HTTP_POST, [](AsyncWebServerRequest* request){}, NULL,
        [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
            extern IrrigationManager irrigationManager;
            irrigationManager.stopNow();
            cJSON* resp = cJSON_CreateObject();
            cJSON_AddStringToObject(resp, "result", "ok");
            cJSON_AddStringToObject(resp, "message", "Irrigation stopped and relay 1 deactivated.");
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

    // Manual RTC time set API
    server->on("/api/settime", HTTP_POST, [](AsyncWebServerRequest* request){}, NULL,
        [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
            String body = String((char*)data).substring(0, len);
            cJSON* json = cJSON_Parse(body.c_str());
            cJSON* resp = cJSON_CreateObject();
            if (!json) {
                cJSON_AddStringToObject(resp, "result", "error");
                cJSON_AddStringToObject(resp, "error", "Invalid JSON");
                char* respStr = cJSON_PrintUnformatted(resp);
                request->send(400, "application/json", respStr);
                cJSON_free(respStr);
                cJSON_Delete(resp);
                return;
            }
            int year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0;
            cJSON* y = cJSON_GetObjectItem(json, "year");
            cJSON* mo = cJSON_GetObjectItem(json, "month");
            cJSON* d = cJSON_GetObjectItem(json, "day");
            cJSON* h = cJSON_GetObjectItem(json, "hour");
            cJSON* mi = cJSON_GetObjectItem(json, "minute");
            cJSON* s = cJSON_GetObjectItem(json, "second");
            if (y && cJSON_IsNumber(y)) year = y->valueint;
            if (mo && cJSON_IsNumber(mo)) month = mo->valueint;
            if (d && cJSON_IsNumber(d)) day = d->valueint;
            if (h && cJSON_IsNumber(h)) hour = h->valueint;
            if (mi && cJSON_IsNumber(mi)) minute = mi->valueint;
            if (s && cJSON_IsNumber(s)) second = s->valueint;
            bool valid = (year >= 2000 && year < 2100 && month >= 1 && month <= 12 && day >= 1 && day <= 31 && hour >= 0 && hour < 24 && minute >= 0 && minute < 60 && second >= 0 && second < 60);
            if (!valid) {
                cJSON_AddStringToObject(resp, "result", "error");
                cJSON_AddStringToObject(resp, "error", "Invalid date/time values");
                char* respStr = cJSON_PrintUnformatted(resp);
                request->send(400, "application/json", respStr);
                cJSON_free(respStr);
                cJSON_Delete(resp);
                cJSON_Delete(json);
                return;
            }
            DateTime dt(year, month, day, hour, minute, second);
            systemManager.getTimeManager().setTime(dt);
            cJSON_AddStringToObject(resp, "result", "ok");
            cJSON_AddStringToObject(resp, "message", "RTC time set");
            char* respStr = cJSON_PrintUnformatted(resp);
            request->send(200, "application/json", respStr);
            cJSON_free(respStr);
            cJSON_Delete(resp);
            cJSON_Delete(json);
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
    server->on("/info.html", HTTP_GET, [this](AsyncWebServerRequest* request) {
        handleStaticFile(request);
    });
    server->on("/info.js", HTTP_GET, [this](AsyncWebServerRequest* request) {
        handleStaticFile(request);
    });

    // Removed schedule.html and schedule.js routes (files deleted)
    
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


