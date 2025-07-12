#include "system/WebServerManager.h"
#include <Arduino.h>
#include <LittleFS.h>

WebServerManager::WebServerManager(DashboardManager* dashMgr, DiagnosticManager* diagMgr)
    : dashboardManager(dashMgr), diagnosticManager(diagMgr), server(nullptr) {}

void WebServerManager::begin() {
    // ...existing code...
    if (!dashboardManager) {
        if (diagnosticManager) {
            diagnosticManager->log(DiagnosticManager::LOG_ERROR, "WebServer", "DashboardManager not set");
        }
        return;
    }

    // Initialize LittleFS
    initializeLittleFS();

    server = new AsyncWebServer(80);

    // API routes
    server->on("/api/status", HTTP_GET, [this](AsyncWebServerRequest* request) {
        handleAPI(request);
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


