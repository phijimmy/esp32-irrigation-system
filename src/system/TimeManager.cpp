
#include "system/TimeManager.h"
#include "devices/RelayController.h"
#include <WiFi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "system/MqttManager.h"

// Use the global instance from main.cpp
extern MqttManager mqttManager;

TimeManager::TimeManager() {}

void TimeManager::begin(I2CManager* i2c, ConfigManager* config, DiagnosticManager* diag) {
    i2cManager = i2c;
    configManager = config;
    diagnosticManager = diag;
    
    // Load INT/SQW GPIO from config
    intSqwGpio = configManager ? configManager->getInt("int_sqw_gpio", 27) : 27;
    if (diagnosticManager) {
        diagnosticManager->log(DiagnosticManager::LOG_INFO, "Time", "INT/SQW GPIO set to %d from config", intSqwGpio);
    }
    
    // Initialize RTC
    if (i2cManager && i2cManager->getI2CMutex()) xSemaphoreTake(i2cManager->getI2CMutex(), portMAX_DELAY);
    rtcFound = rtc.begin();
    if (i2cManager && i2cManager->getI2CMutex()) xSemaphoreGive(i2cManager->getI2CMutex());
    if (rtcFound) {
        if (diagnosticManager) {
            diagnosticManager->log(DiagnosticManager::LOG_INFO, "Time", "DS3231 RTC detected and initialized");
        }
        
        // Check if RTC lost power and set build time if needed
        setBuildTimeIfNeeded();
        
        // Enable alarm interrupts on INT/SQW pin
        // Clear any existing alarm flags first
        if (i2cManager && i2cManager->getI2CMutex()) xSemaphoreTake(i2cManager->getI2CMutex(), portMAX_DELAY);
        rtc.clearAlarm(1);
        rtc.clearAlarm(2);
        if (i2cManager && i2cManager->getI2CMutex()) xSemaphoreGive(i2cManager->getI2CMutex());
        alarm1Active = false; // Reset alarm state
        // Enable alarm interrupts (this will control the INT/SQW pin)
        if (i2cManager && i2cManager->getI2CMutex()) xSemaphoreTake(i2cManager->getI2CMutex(), portMAX_DELAY);
        rtc.writeSqwPinMode(DS3231_OFF); // Disable square wave, enable alarm interrupts
        rtc.disable32K(); // Disable 32K output to save power
        if (i2cManager && i2cManager->getI2CMutex()) xSemaphoreGive(i2cManager->getI2CMutex());
        // The INT/SQW pin will now be controlled by alarm interrupts
        if (diagnosticManager) {
            diagnosticManager->log(DiagnosticManager::LOG_INFO, "Time", "DS3231 INT/SQW pin configured for alarm interrupts");
        }
        
        // Initialize interrupt pin
        initializeInterruptPin();
        
        // Initialize DST status based on current time
        DateTime localTime = rtc.now(); // RTC stores local time
        bool dstEnabled = configManager ? configManager->getBool("dst_enabled", true) : true;
        bool dstActive = dstEnabled && isDSTActive(localTime);
        lastDSTState = dstActive;
        if (diagnosticManager) {
            diagnosticManager->log(DiagnosticManager::LOG_INFO, "Time", "DST status initialized: %s", 
                dstActive ? "Active" : "Inactive");
        }
        
        // Set alarms for today's schedule
        setAlarmsForToday();
        
        // Enable alarm interrupts in DS3231 control register
        enableAlarmInterrupts();
        
        // Log current time with timezone info
        if (diagnosticManager) {
            // Get timezone configuration
            int timezoneOffset = configManager ? configManager->getInt("timezone_offset", 1) : 1;
            bool dstEnabled = configManager ? configManager->getBool("dst_enabled", true) : true;
            bool dstActive = dstEnabled && isDSTActive(localTime);
            int dstOffset = dstActive ? (configManager ? configManager->getInt("dst_offset", 1) : 1) : 0;
            int totalOffset = timezoneOffset + dstOffset;
            
            const char* tzName = (timezoneOffset == 1) ? (dstActive ? "CEST" : "CET") : 
                                (dstActive ? "DST" : "STD");
            
            diagnosticManager->log(DiagnosticManager::LOG_INFO, "Time", "Timezone: UTC%+d, DST: %s (%s)", 
                totalOffset, dstActive ? "Active" : "Inactive", tzName);
            
            diagnosticManager->log(DiagnosticManager::LOG_INFO, "Time", "Current date: %s", getCurrentDateString().c_str());
            diagnosticManager->log(DiagnosticManager::LOG_INFO, "Time", "Current time: %s", getCurrentTimeString().c_str());
            
            // Log DST period for current year
            DateTime dstStart = calculateDSTStart(localTime.year());
            DateTime dstEnd = calculateDSTEnd(localTime.year());
            
            diagnosticManager->log(DiagnosticManager::LOG_INFO, "Time", "DST %d: %02d/%02d 02:00 to %02d/%02d 03:00", 
                localTime.year(),
                dstStart.month(), dstStart.day(),
                dstEnd.month(), dstEnd.day());
        }
        
        // Initialize lastDayId to prevent false "new day" detection during startup
        DateTime now = getLocalTime();
        lastDayId = now.year() * 10000 + now.month() * 100 + now.day();
        if (diagnosticManager) {
            diagnosticManager->log(DiagnosticManager::LOG_INFO, "Time", "begin: initialized lastDayId=%d", lastDayId);
        }
    } else {
        if (diagnosticManager) {
            diagnosticManager->log(DiagnosticManager::LOG_WARN, "Time", "DS3231 RTC not found!");
        }
    }
}

void TimeManager::initializeInterruptPin() {
    if (intSqwGpio != 255) {
        pinMode(intSqwGpio, INPUT_PULLUP);
        lastIntSqwState = digitalRead(intSqwGpio);
        if (diagnosticManager) {
            diagnosticManager->log(DiagnosticManager::LOG_INFO, "Time", 
                "INT/SQW GPIO %d initialized as INPUT_PULLUP, initial state: %s", 
                intSqwGpio, lastIntSqwState ? "HIGH" : "LOW");
        }
    }
}

bool TimeManager::pollInterruptPin() {
    if (intSqwGpio == 255 || !rtcFound) return false;
    
    bool currentState = digitalRead(intSqwGpio);
    if (currentState != lastIntSqwState) {
        lastIntSqwState = currentState;
        
        // Log state change for debugging (but limit spam)
        static bool lastLogState = true;
        if (lastLogState != currentState && diagnosticManager) {
            diagnosticManager->log(DiagnosticManager::LOG_INFO, "Time", 
                "INT/SQW GPIO %d state changed to: %s", intSqwGpio, currentState ? "HIGH" : "LOW");
            lastLogState = currentState;
        }
        
        // Update relay if RelayController is connected
        updateRelayFromIntSqw();
        
        return true; // State changed
    }
    return false; // No change
}

bool TimeManager::getHardwareIntSqwState() const {
    if (intSqwGpio == 255) return true; // Default to HIGH if not configured
    return digitalRead(intSqwGpio);
}

void TimeManager::updateRelayFromIntSqw() {
    if (!relayController) return;
    
    bool currentState = getHardwareIntSqwState();
    
    // Control relay 0 (GPIO 32) based on INT/SQW state (INVERTED LOGIC)
    // INT/SQW is LOW during alarm1 (relay ON), HIGH during alarm2 (relay OFF)
    if (currentState != lastRelayControlState) {
        extern MqttManager mqttManager;
        if (currentState) {
            // INT/SQW is HIGH (alarm2 period) - deactivate relay
            relayController->deactivateRelay(0);
            if (diagnosticManager) {
                diagnosticManager->log(DiagnosticManager::LOG_INFO, "Time", "INT/SQW HIGH - deactivated relay 0");
            }
            // Relay state will be published by RelayController
        } else {
            // INT/SQW is LOW (alarm1 period) - activate relay
            relayController->activateRelay(0);
            if (diagnosticManager) {
                diagnosticManager->log(DiagnosticManager::LOG_INFO, "Time", "INT/SQW LOW - activated relay 0");
            }
            // Relay state will be published by RelayController
        }
        lastRelayControlState = currentState;
    }
}

void TimeManager::update() {
    if (!rtcFound) return;
    
    // Check for daily schedule updates (new day)
    updateDailySchedule();
    
    // Check for DST transitions
    updateDSTTransition();
    
    // Check for periodic NTP sync
    updateNTPSync();
    
    // Handle alarm interrupts
    handleAlarmInterrupt();
}

DateTime TimeManager::getTime() {
    // Debug: log warnedInvalidRTC and this pointer
    if (diagnosticManager) {
        diagnosticManager->log(DiagnosticManager::LOG_DEBUG, "Time", "getTime: warnedInvalidRTC=%d, this=%p", warnedInvalidRTC, this);
    }
    static DateTime lastGoodTime;
    DateTime dt;
    if (rtcFound) {
        if (i2cManager && i2cManager->getI2CMutex()) xSemaphoreTake(i2cManager->getI2CMutex(), portMAX_DELAY);
        dt = rtc.now();
        if (i2cManager && i2cManager->getI2CMutex()) xSemaphoreGive(i2cManager->getI2CMutex());
        // Check for valid/sane date
        bool valid = dt.isValid() && dt.year() >= 2000 && dt.year() < 2100 && dt.month() >= 1 && dt.month() <= 12 && dt.day() >= 1 && dt.day() <= 31;
        if (valid) {
            lastGoodTime = dt;
            // Do NOT reset warnedInvalidRTC here; only set it true on first invalid
        } else {
            if (!warnedInvalidRTC && diagnosticManager) {
                diagnosticManager->log(DiagnosticManager::LOG_WARN, "Time", "RTC returned invalid/corrupt date: %04d-%02d-%02d %02d:%02d:%02d (valid=%d) - using last known good time", dt.year(), dt.month(), dt.day(), dt.hour(), dt.minute(), dt.second(), dt.isValid());
                warnedInvalidRTC = true;
            }
            dt = lastGoodTime;
        }
        if (diagnosticManager) {
            diagnosticManager->log(DiagnosticManager::LOG_DEBUG, "Time", "getTime: RTC now: %04d-%02d-%02d %02d:%02d:%02d (valid=%d)",
                dt.year(), dt.month(), dt.day(), dt.hour(), dt.minute(), dt.second(), dt.isValid());
        }
    } else {
        dt = buildTime();
        if (diagnosticManager) {
            diagnosticManager->log(DiagnosticManager::LOG_DEBUG, "Time", "getTime: buildTime fallback: %04d-%02d-%02d %02d:%02d:%02d (valid=%d)",
                dt.year(), dt.month(), dt.day(), dt.hour(), dt.minute(), dt.second(), dt.isValid());
        }
    }
    return dt;
}

DateTime TimeManager::getLocalTime() {
    DateTime dt = getTime();
    if (diagnosticManager) {
        diagnosticManager->log(DiagnosticManager::LOG_DEBUG, "Time", "getLocalTime: %04d-%02d-%02d %02d:%02d:%02d (valid=%d)",
            dt.year(), dt.month(), dt.day(), dt.hour(), dt.minute(), dt.second(), dt.isValid());
    }
    return dt;
}

void TimeManager::setTime(const DateTime& dt) {
    if (rtcFound) {
        if (i2cManager && i2cManager->getI2CMutex()) xSemaphoreTake(i2cManager->getI2CMutex(), portMAX_DELAY);
        rtc.adjust(dt);
        if (i2cManager && i2cManager->getI2CMutex()) xSemaphoreGive(i2cManager->getI2CMutex());
        // Reinitialize lastDayId after time adjustment to prevent false "new day" detection
        int newDayId = dt.year() * 10000 + dt.month() * 100 + dt.day();
        if (diagnosticManager) {
            diagnosticManager->log(DiagnosticManager::LOG_INFO, "Time", "setTime: updating lastDayId from %d to %d", lastDayId, newDayId);
        }
        lastDayId = newDayId;
    }
}

bool TimeManager::isDSTActive(const DateTime& localTime) {
    DateTime dstStart = calculateDSTStart(localTime.year());
    DateTime dstEnd = calculateDSTEnd(localTime.year());
    
    // Simple comparison with local time - DST is active between last Sunday in March and last Sunday in October
    return (localTime >= dstStart && localTime < dstEnd);
}

DateTime TimeManager::calculateDSTStart(int year) {
    // Last Sunday in March at 2:00 AM
    DateTime marchEnd(year, 4, 1, 2, 0, 0); // First day of April
    marchEnd = marchEnd - TimeSpan(1, 0, 0, 0); // Last day of March
    
    while (marchEnd.dayOfTheWeek() != 0) { // 0 = Sunday
        marchEnd = marchEnd - TimeSpan(1, 0, 0, 0);
    }
    return DateTime(year, 3, marchEnd.day(), 2, 0, 0);
}

DateTime TimeManager::calculateDSTEnd(int year) {
    // Last Sunday in October at 3:00 AM
    DateTime octoberEnd(year, 11, 1, 3, 0, 0); // First day of November
    octoberEnd = octoberEnd - TimeSpan(1, 0, 0, 0); // Last day of October
    
    while (octoberEnd.dayOfTheWeek() != 0) { // 0 = Sunday
        octoberEnd = octoberEnd - TimeSpan(1, 0, 0, 0);
    }
    return DateTime(year, 10, octoberEnd.day(), 3, 0, 0);
}

void TimeManager::updateDSTTransition() {
    if (!rtcFound || !configManager) return;
    
    // Check if DST is enabled in configuration
    bool dstEnabled = configManager->getBool("dst_enabled", true);
    if (!dstEnabled) {
        return; // DST disabled, no transitions to handle
    }
    
    // Only check for DST transitions periodically (every 10 minutes) to avoid false positives
    unsigned long now = millis();
    if (lastDSTCheck != 0 && (now - lastDSTCheck) < 600000) { // 10 minutes
        return;
    }
    lastDSTCheck = now;
    
    DateTime localTime = getTime(); // RTC stores local time
    bool currentDSTState = isDSTActive(localTime);
    
    // Only perform DST transitions during the actual transition periods (March/October)
    // and only if we're within a few days of the transition
    if (localTime.month() != 3 && localTime.month() != 10) {
        // Not in transition months, just update the state without transitions
        lastDSTState = currentDSTState;
        return;
    }
    
    // Check if DST state changed and perform actual time adjustment
    if (currentDSTState != lastDSTState) {
        bool autoAdjust = configManager->getBool("dst_auto_adjust", true);
        if (autoAdjust) {
            int dstOffset = configManager->getInt("dst_offset", 1);
            if (currentDSTState) {
                // Spring forward - advance time by 1 hour
                if (diagnosticManager) {
                    diagnosticManager->log(DiagnosticManager::LOG_INFO, "Time", "DST transition: Spring forward - advancing time by %d hour(s)", dstOffset);
                }
                setTime(localTime + TimeSpan(0, dstOffset, 0, 0));
            } else {
                // Fall back - set time back by 1 hour
                if (diagnosticManager) {
                    diagnosticManager->log(DiagnosticManager::LOG_INFO, "Time", "DST transition: Fall back - setting time back by %d hour(s)", dstOffset);
                }
                setTime(localTime - TimeSpan(0, dstOffset, 0, 0));
            }
        } else {
            if (diagnosticManager) {
                diagnosticManager->log(DiagnosticManager::LOG_INFO, "Time", "DST transition detected but auto-adjust is disabled");
            }
        }
        lastDSTState = currentDSTState;
    }
}

bool TimeManager::syncWithNTP() {
    if (!isWiFiConnected()) return false;
    
    // Try primary server first
    String server1 = configManager ? String(configManager->get("ntp_server_1")) : "192.168.1.1";
    String server2 = configManager ? String(configManager->get("ntp_server_2")) : "0.de.pool.ntp.org";
    int timeout = configManager ? configManager->getInt("ntp_timeout", 5000) : 5000;
    
    if (diagnosticManager) {
        diagnosticManager->log(DiagnosticManager::LOG_INFO, "Time", "Trying NTP sync with primary server: %s", server1.c_str());
    }
    
    if (performNTPSync(server1, timeout)) {
        lastNTPSync = millis();
        if (diagnosticManager) {
            diagnosticManager->log(DiagnosticManager::LOG_INFO, "Time", "Initial NTP synchronization successful");
        }
        return true;
    }
    
    // Try secondary server
    if (diagnosticManager) {
        diagnosticManager->log(DiagnosticManager::LOG_INFO, "Time", "Primary server failed, trying secondary server: %s", server2.c_str());
    }
    
    if (performNTPSync(server2, timeout)) {
        lastNTPSync = millis();
        if (diagnosticManager) {
            diagnosticManager->log(DiagnosticManager::LOG_INFO, "Time", "NTP synchronization successful with secondary server");
        }
        return true;
    }
    
    if (diagnosticManager) {
        diagnosticManager->log(DiagnosticManager::LOG_WARN, "Time", "NTP synchronization failed with both servers");
    }
    return false;
}

bool TimeManager::performNTPSync(const String& server, int timeoutMs) {
    WiFiUDP udp;
    udp.begin(123); // NTP port
    
    IPAddress serverIP;
    if (!WiFi.hostByName(server.c_str(), serverIP)) {
        if (diagnosticManager) {
            diagnosticManager->log(DiagnosticManager::LOG_WARN, "Time", "Failed to resolve NTP server: %s", server.c_str());
        }
        udp.stop();
        return false;
    }
    
    // Send NTP request
    byte ntpPacket[48];
    memset(ntpPacket, 0, 48);
    ntpPacket[0] = 0b11100011; // LI, Version, Mode
    ntpPacket[1] = 0;          // Stratum
    ntpPacket[2] = 6;          // Polling Interval
    ntpPacket[3] = 0xEC;       // Peer Clock Precision
    
    udp.beginPacket(serverIP, 123);
    udp.write(ntpPacket, 48);
    udp.endPacket();
    
    // Wait for response
    unsigned long startTime = millis();
    while (millis() - startTime < timeoutMs) {
        int packetSize = udp.parsePacket();
        if (packetSize >= 48) {
            udp.read(ntpPacket, 48);
            
            // Extract timestamp from packet (seconds since 1900)
            unsigned long secsSince1900 = (ntpPacket[40] << 24) | (ntpPacket[41] << 16) | (ntpPacket[42] << 8) | ntpPacket[43];
            
            // Convert to Unix timestamp (seconds since 1970)
            unsigned long unixTime = secsSince1900 - 2208988800UL;
            
            // Log NTP UTC time
            DateTime utcTime(unixTime);
            if (diagnosticManager) {
                diagnosticManager->log(DiagnosticManager::LOG_INFO, "Time", "NTP UTC time: %04d-%02d-%02d %02d:%02d:%02d UTC",
                    utcTime.year(), utcTime.month(), utcTime.day(),
                    utcTime.hour(), utcTime.minute(), utcTime.second());
            }
            
            // Convert UTC to local time for storage in RTC
            int timezoneOffset = configManager ? configManager->getInt("timezone_offset", 1) : 1;
            bool dstEnabled = configManager ? configManager->getBool("dst_enabled", true) : true;
            
            // Apply timezone offset
            DateTime localTime = utcTime + TimeSpan(0, timezoneOffset, 0, 0);
            
            // Apply DST offset if active
            if (dstEnabled && isDSTActive(localTime)) {
                int dstOffset = configManager ? configManager->getInt("dst_offset", 1) : 1;
                localTime = localTime + TimeSpan(0, dstOffset, 0, 0);
            }
            
            // Store local time in RTC
            setTime(localTime);
            
            if (diagnosticManager) {
                diagnosticManager->log(DiagnosticManager::LOG_INFO, "Time", "RTC set to local time: %04d-%02d-%02d %02d:%02d:%02d",
                    localTime.year(), localTime.month(), localTime.day(),
                    localTime.hour(), localTime.minute(), localTime.second());
            }
            
            // Verify RTC was set correctly
            DateTime rtcReadback = getTime();
            if (diagnosticManager) {
                diagnosticManager->log(DiagnosticManager::LOG_INFO, "Time", "RTC readback verification: %04d-%02d-%02d %02d:%02d:%02d (local)",
                    rtcReadback.year(), rtcReadback.month(), rtcReadback.day(),
                    rtcReadback.hour(), rtcReadback.minute(), rtcReadback.second());
            }
            
            // Check time difference (compare local times)
            long timeDiff = abs((long)rtcReadback.unixtime() - (long)localTime.unixtime());
            if (diagnosticManager) {
                if (timeDiff <= 2) { // Allow 2 seconds tolerance
                    diagnosticManager->log(DiagnosticManager::LOG_INFO, "Time", "RTC update verification: SUCCESS (time difference: %ld seconds)", timeDiff);
                } else {
                    diagnosticManager->log(DiagnosticManager::LOG_WARN, "Time", "RTC update verification: FAILED (time difference: %ld seconds)", timeDiff);
                }
            }
            
            udp.stop();
            return true;
        }
        unsigned long start = millis();
        while (millis() - start < 10) {
            vTaskDelay(1); // Yield to RTOS, non-blocking
        }
    }
    
    udp.stop();
    return false;
}

void TimeManager::updateNTPSync() {
    if (!configManager) return;
    
    bool ntpEnabled = configManager->getBool("ntp_enabled", true);
    int syncInterval = configManager->getInt("ntp_sync_interval", 3600);
    
    if (ntpEnabled && isWiFiConnected() && (millis() - lastNTPSync) > (syncInterval * 1000UL)) {
        if (diagnosticManager) {
            diagnosticManager->log(DiagnosticManager::LOG_INFO, "Time", "Performing periodic NTP synchronization...");
        }
        syncWithNTP();
    }
}

void TimeManager::forceNTPSync() {
    if (diagnosticManager) {
        diagnosticManager->log(DiagnosticManager::LOG_INFO, "Time", "Forcing immediate NTP synchronization...");
    }
    syncWithNTP();
}

void TimeManager::onWiFiConnected() {
    if (diagnosticManager) {
        diagnosticManager->log(DiagnosticManager::LOG_INFO, "Time", "onWiFiConnected() called - attempting initial NTP sync");
    }
    
    if (!ntpInitialSyncDone) {
        if (diagnosticManager) {
            diagnosticManager->log(DiagnosticManager::LOG_INFO, "Time", "WiFi connected, performing initial NTP synchronization...");
        }
        if (syncWithNTP()) {
            ntpInitialSyncDone = true;
        }
    }
}

bool TimeManager::isWiFiConnected() {
    return WiFi.isConnected();
}

bool TimeManager::setAlarm1(int hour, int minute, int second, bool enabled) {
    if (!rtcFound) return false;
    
    if (enabled) {
        // Clear any existing alarm flag
        rtc.clearAlarm(1);
        // Set alarm to trigger on hour:minute:second match (daily)
        rtc.setAlarm1(DateTime(2000, 1, 1, hour, minute, second), DS3231_A1_Hour);
        if (diagnosticManager) {
            diagnosticManager->log(DiagnosticManager::LOG_INFO, "Time", "Alarm1 set: %02d:%02d:%02d (enabled)", hour, minute, second);
        }
    } else {
        rtc.disableAlarm(1);
        rtc.clearAlarm(1);
        alarm1Active = false; // Only reset when disabled
        if (diagnosticManager) {
            diagnosticManager->log(DiagnosticManager::LOG_INFO, "Time", "Alarm1 disabled");
        }
    }
    return true;
}

bool TimeManager::setAlarm2(int hour, int minute, bool enabled) {
    if (!rtcFound) return false;
    
    if (enabled) {
        // Clear any existing alarm flag
        rtc.clearAlarm(2);
        // Set alarm to trigger on hour:minute match (daily)
        rtc.setAlarm2(DateTime(2000, 1, 1, hour, minute, 0), DS3231_A2_Hour);
        if (diagnosticManager) {
            diagnosticManager->log(DiagnosticManager::LOG_INFO, "Time", "Alarm2 set: %02d:%02d (enabled)", hour, minute);
        }
    } else {
        rtc.disableAlarm(2);
        rtc.clearAlarm(2);
        if (diagnosticManager) {
            diagnosticManager->log(DiagnosticManager::LOG_INFO, "Time", "Alarm2 disabled");
        }
    }
    return true;
}

bool TimeManager::readAlarm1(int &hour, int &minute, int &second, bool &enabled) {
    // This would require reading from the DS3231 registers
    // For now, return false as it's not implemented
    return false;
}

bool TimeManager::readAlarm2(int &hour, int &minute, bool &enabled) {
    // This would require reading from the DS3231 registers
    // For now, return false as it's not implemented
    return false;
}

void TimeManager::clearAlarm1() {
    if (rtcFound) {
        rtc.clearAlarm(1);
    }
}

void TimeManager::clearAlarm2() {
    if (rtcFound) {
        rtc.clearAlarm(2);
    }
}

void TimeManager::updateAlarmsFromConfig() {
    if (!configManager || !rtcFound) return;
    
    // Reset alarm state when setting new alarms
    alarm1Active = false;
    
    if (diagnosticManager) {
        diagnosticManager->log(DiagnosticManager::LOG_INFO, "Time", "updateAlarmsFromConfig() called, rtcFound=%s", rtcFound ? "true" : "false");
    }
    
    // Update Alarm1
    cJSON* alarm1Config = configManager->getSection("alarm1");
    if (alarm1Config) {
        if (diagnosticManager) {
            diagnosticManager->log(DiagnosticManager::LOG_INFO, "Time", "alarm1 config section found");
        }
        int hour = cJSON_GetObjectItem(alarm1Config, "hour") ? cJSON_GetObjectItem(alarm1Config, "hour")->valueint : 19;
        int minute = cJSON_GetObjectItem(alarm1Config, "minute") ? cJSON_GetObjectItem(alarm1Config, "minute")->valueint : 26;
        int second = cJSON_GetObjectItem(alarm1Config, "second") ? cJSON_GetObjectItem(alarm1Config, "second")->valueint : 0;
        bool enabled = cJSON_GetObjectItem(alarm1Config, "enabled") ? cJSON_IsTrue(cJSON_GetObjectItem(alarm1Config, "enabled")) : true;
        
        if (diagnosticManager) {
            diagnosticManager->log(DiagnosticManager::LOG_INFO, "Time", "alarm1 config: hour=%d, minute=%d, second=%d, enabled=%s", hour, minute, second, enabled ? "true" : "false");
        }
        setAlarm1(hour, minute, second, enabled);
    }
    
    // Update Alarm2
    cJSON* alarm2Config = configManager->getSection("alarm2");
    if (alarm2Config) {
        if (diagnosticManager) {
            diagnosticManager->log(DiagnosticManager::LOG_INFO, "Time", "alarm2 config section found");
        }
        int hour = cJSON_GetObjectItem(alarm2Config, "hour") ? cJSON_GetObjectItem(alarm2Config, "hour")->valueint : 19;
        int minute = cJSON_GetObjectItem(alarm2Config, "minute") ? cJSON_GetObjectItem(alarm2Config, "minute")->valueint : 30;
        bool enabled = cJSON_GetObjectItem(alarm2Config, "enabled") ? cJSON_IsTrue(cJSON_GetObjectItem(alarm2Config, "enabled")) : true;
        
        if (diagnosticManager) {
            diagnosticManager->log(DiagnosticManager::LOG_INFO, "Time", "alarm2 config: hour=%d, minute=%d, enabled=%s", hour, minute, enabled ? "true" : "false");
        }
        setAlarm2(hour, minute, enabled);
    }
}

// Weekly schedule implementation

void TimeManager::updateDailySchedule() {
    if (!configManager || !configManager->getBool("auto_update_daily", true)) return;
    
    if (isNewDay()) {
        if (diagnosticManager) {
            diagnosticManager->log(DiagnosticManager::LOG_INFO, "Time", "New day detected - updating daily schedule");
        }
        setAlarmsForToday();
    }
}

void TimeManager::setAlarmsForToday() {
    if (!configManager || !rtcFound) return;
    
    // Reset alarm state when setting new alarms
    alarm1Active = false;
    
    bool useWeeklySchedule = configManager->getBool("use_weekly_schedule", true);
    if (!useWeeklySchedule) {
        // Use legacy single alarm settings
        updateAlarmsFromConfig();
        return;
    }
    
    // Get current day of week
    int dayOfWeek = getCurrentDayOfWeek();
    String dayName = getCurrentDayName();
    
    if (diagnosticManager) {
        diagnosticManager->log(DiagnosticManager::LOG_INFO, "Time", "Setting alarms for %s (day %d)", dayName.c_str(), dayOfWeek);
    }
    
    // Get weekly schedule
    cJSON* weeklySchedule = configManager->getSection("weekly_schedule");
    if (!weeklySchedule) {
        if (diagnosticManager) {
            diagnosticManager->log(DiagnosticManager::LOG_WARN, "Time", "Weekly schedule not found, using legacy alarms");
        }
        updateAlarmsFromConfig();
        return;
    }
    
    // Get today's schedule
    const char* dayNames[] = {"sunday", "monday", "tuesday", "wednesday", "thursday", "friday", "saturday"};
    cJSON* todaySchedule = cJSON_GetObjectItem(weeklySchedule, dayNames[dayOfWeek]);
    if (!todaySchedule) {
        if (diagnosticManager) {
            diagnosticManager->log(DiagnosticManager::LOG_WARN, "Time", "Schedule for %s not found", dayNames[dayOfWeek]);
        }
        return;
    }
    
    // Use day-level enabled flag for both alarms
    bool dayEnabled = cJSON_GetObjectItem(todaySchedule, "enabled") ? cJSON_IsTrue(cJSON_GetObjectItem(todaySchedule, "enabled")) : false;

    // Set Alarm1 for today only if scheduled time is in the future and day is enabled
    cJSON* alarm1Config = cJSON_GetObjectItem(todaySchedule, "alarm1");
    if (alarm1Config) {
        int hour = cJSON_GetObjectItem(alarm1Config, "hour") ? cJSON_GetObjectItem(alarm1Config, "hour")->valueint : 8;
        int minute = cJSON_GetObjectItem(alarm1Config, "minute") ? cJSON_GetObjectItem(alarm1Config, "minute")->valueint : 0;
        int second = cJSON_GetObjectItem(alarm1Config, "second") ? cJSON_GetObjectItem(alarm1Config, "second")->valueint : 0;

        bool shouldSetAlarm = false;
        if (dayEnabled) {
            // Compare scheduled time to current time
            DateTime now = rtc.now();
            int nowSec = now.hour() * 3600 + now.minute() * 60 + now.second();
            int alarmSec = hour * 3600 + minute * 60 + second;
            if (alarmSec > nowSec) {
                shouldSetAlarm = true;
            }
        }

        if (diagnosticManager) {
            diagnosticManager->log(DiagnosticManager::LOG_INFO, "Time", "%s Alarm1: %02d:%02d:%02d %s (will %s)",
                dayName.c_str(), hour, minute, second, dayEnabled ? "enabled" : "disabled", shouldSetAlarm ? "set" : "clear");
        }
        if (shouldSetAlarm) {
            setAlarm1(hour, minute, second, true);
        } else {
            setAlarm1(hour, minute, second, false); // disables and clears alarm
        }
    }

    // Set Alarm2 for today, using day-level enabled
    cJSON* alarm2Config = cJSON_GetObjectItem(todaySchedule, "alarm2");
    if (alarm2Config) {
        int hour = cJSON_GetObjectItem(alarm2Config, "hour") ? cJSON_GetObjectItem(alarm2Config, "hour")->valueint : 8;
        int minute = cJSON_GetObjectItem(alarm2Config, "minute") ? cJSON_GetObjectItem(alarm2Config, "minute")->valueint : 5;

        if (diagnosticManager) {
            diagnosticManager->log(DiagnosticManager::LOG_INFO, "Time", "%s Alarm2: %02d:%02d %s", 
                dayName.c_str(), hour, minute, dayEnabled ? "enabled" : "disabled");
        }
        setAlarm2(hour, minute, dayEnabled);
    }
}

String TimeManager::getCurrentDayName() {
    const char* dayNames[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
    int dayOfWeek = getCurrentDayOfWeek();
    return String(dayNames[dayOfWeek]);
}

String TimeManager::getCurrentDateString() {
    DateTime now = getLocalTime();
    const char* monthNames[] = {"January", "February", "March", "April", "May", "June", 
                               "July", "August", "September", "October", "November", "December"};
    
    // Get ordinal suffix for day (1st, 2nd, 3rd, 4th, etc.)
    int day = now.day();
    String suffix = "th";
    if (day == 1 || day == 21 || day == 31) suffix = "st";
    else if (day == 2 || day == 22) suffix = "nd";
    else if (day == 3 || day == 23) suffix = "rd";
    
    return getCurrentDayName() + " " + String(day) + suffix + " " + String(monthNames[now.month()-1]) + " " + String(now.year());
}

String TimeManager::getCurrentTimeString() {
    DateTime now = getLocalTime();
    char timeStr[10];
    snprintf(timeStr, sizeof(timeStr), "%02d:%02d:%02d", now.hour(), now.minute(), now.second());
    return String(timeStr);
}

int TimeManager::getCurrentDayOfWeek() {
    DateTime now = getLocalTime();
    return now.dayOfTheWeek(); // 0=Sunday, 1=Monday, ..., 6=Saturday
}

bool TimeManager::isNewDay() {
    DateTime now = getLocalTime();
    static bool hasSeenValidDate = false;
    // Check for invalid date (e.g., RTC not set or time adjustment in progress)
    if (!now.isValid() || now.year() < 2000 || now.month() < 1 || now.month() > 12 || now.day() < 1 || now.day() > 31) {
        if (diagnosticManager && hasSeenValidDate) {
            diagnosticManager->log(DiagnosticManager::LOG_WARN, "Time", "isNewDay: Invalid date detected (year=%d, month=%d, day=%d) - skipping new day check", now.year(), now.month(), now.day());
        }
        return false;
    }
    hasSeenValidDate = true;
    // Create a unique day identifier using year, month, and day
    int currentDayId = now.year() * 10000 + now.month() * 100 + now.day();
    
    if (lastDayId == -1) {
        // First check, initialize but don't consider it a "new day"
        lastDayId = currentDayId;
        if (diagnosticManager) {
            diagnosticManager->log(DiagnosticManager::LOG_INFO, "Time", "isNewDay: Initialized lastDayId=%d (not a new day)", lastDayId);
        }
        return false;
    }
    
    if (currentDayId != lastDayId) {
        // Extract year, month, day from both IDs to compare properly
        int lastYear = lastDayId / 10000;
        int lastMonth = (lastDayId % 10000) / 100;
        int lastDay = lastDayId % 100;
        
        int currentYear = now.year();
        int currentMonth = now.month();
        int currentDay = now.day();
        
        if (diagnosticManager) {
            diagnosticManager->log(DiagnosticManager::LOG_INFO, "Time", "isNewDay: dayId changed from %d to %d (%04d-%02d-%02d vs %04d-%02d-%02d)", 
                lastDayId, currentDayId, lastYear, lastMonth, lastDay, currentYear, currentMonth, currentDay);
        }
        
        // Only consider it a new day if we've actually moved to a different calendar day
        // This prevents false positives from time adjustments within the same day
        if (currentYear > lastYear || 
            (currentYear == lastYear && currentMonth > lastMonth) ||
            (currentYear == lastYear && currentMonth == lastMonth && currentDay > lastDay)) {
            lastDayId = currentDayId;
            if (diagnosticManager) {
                diagnosticManager->log(DiagnosticManager::LOG_INFO, "Time", "isNewDay: TRUE - calendar day advanced");
            }
            return true;
        } else {
            // Time adjustment within same day or backwards, just update without triggering "new day"
            lastDayId = currentDayId;
            if (diagnosticManager) {
                diagnosticManager->log(DiagnosticManager::LOG_INFO, "Time", "isNewDay: FALSE - time adjustment within same/previous day");
            }
            return false;
        }
    }
    
    return false;
}

void TimeManager::setBuildTimeIfNeeded() {
    if (!configManager || !rtcFound) return;
    
    bool forceBuildTime = configManager->getBool("force_build_time", false);
    
    if (forceBuildTime || rtc.lostPower()) {
        DateTime buildDateTime = buildTime();
        rtc.adjust(buildDateTime);
        
        // Reinitialize lastDayId after time adjustment
        int newDayId = buildDateTime.year() * 10000 + buildDateTime.month() * 100 + buildDateTime.day();
        if (diagnosticManager) {
            diagnosticManager->log(DiagnosticManager::LOG_INFO, "Time", "setBuildTimeIfNeeded: updating lastDayId from %d to %d", lastDayId, newDayId);
        }
        lastDayId = newDayId;
        
        if (diagnosticManager) {
            diagnosticManager->log(DiagnosticManager::LOG_INFO, "Time", 
                "RTC set to build time: %04d-%02d-%02d %02d:%02d:%02d%s", 
                buildDateTime.year(), buildDateTime.month(), buildDateTime.day(),
                buildDateTime.hour(), buildDateTime.minute(), buildDateTime.second(),
                forceBuildTime ? " (forced)" : " (lost power)");
        }
        
        // Clear the force flag after setting time
        if (forceBuildTime) {
            configManager->setBool("force_build_time", false);
        }
    }
}

DateTime TimeManager::buildTime() {
#ifdef BUILD_TIME
    // BUILD_TIME format: "YYYY-MM-DD HH:MM:SS"
    int year = 2000, month = 1, day = 1, hour = 0, minute = 0, second = 0;
    if (sscanf(BUILD_TIME, "%d-%d-%d %d:%d:%d", &year, &month, &day, &hour, &minute, &second) == 6) {
        return DateTime(year, month, day, hour, minute, second);
    }
#endif
    // Fallback to __DATE__ and __TIME__ if BUILD_TIME is not defined
    char month_str[4] = {0};
    int day = 1, year = 2000, hour = 0, minute = 0, second = 0;
    if (sscanf(__DATE__, "%3s %d %d", month_str, &day, &year) != 3) {
        strcpy(month_str, "Jan");
        day = 1;
        year = 2000;
    }
    if (sscanf(__TIME__, "%d:%d:%d", &hour, &minute, &second) != 3) {
        hour = 0;
        minute = 0;
        second = 0;
    }
    const char* months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                           "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    int month = 1;
    for (int i = 0; i < 12; i++) {
        if (strcmp(month_str, months[i]) == 0) {
            month = i + 1;
            break;
        }
    }
    if (year < 2000 || month < 1 || month > 12 || day < 1 || day > 31) {
        year = 2000;
        month = 1;
        day = 1;
    }
    return DateTime(year, month, day, hour, minute, second);
}

void TimeManager::enableAlarmInterrupts() {
    if (!rtcFound) return;
    
    // Clear any existing alarm flags BEFORE enabling interrupts
    rtc.clearAlarm(1);
    rtc.clearAlarm(2);
    alarm1Active = false; // Reset alarm state
    
    // Configure DS3231 control register to enable alarm interrupts
    // This will make the INT/SQW pin respond to alarm triggers
    rtc.writeSqwPinMode(DS3231_OFF); // Disable square wave output
    rtc.disable32K(); // Disable 32K output to save power
    
    // Set alarms for today's schedule
    setAlarmsForToday();
    
    // Clear alarm flags AGAIN after setting alarms to ensure clean start
    rtc.clearAlarm(1);
    rtc.clearAlarm(2);
    alarm1Active = false;
    
    if (diagnosticManager) {
        diagnosticManager->log(DiagnosticManager::LOG_INFO, "Time", "DS3231 alarm interrupts enabled");
    }
}

void TimeManager::handleAlarmInterrupt() {
    if (!rtcFound || intSqwGpio == 255) return;
    
    // Check alarm flags, but only process them if they represent new triggers
    bool alarm1Triggered = rtc.alarmFired(1);
    bool alarm2Triggered = rtc.alarmFired(2);
    
    // Only process alarm1 if it wasn't already active and current time is actually at or past the alarm time
    if (alarm1Triggered && !alarm1Active) {
        DateTime currentTime = getTime();
        
        // Get today's alarm1 time from config to verify this is a legitimate trigger
        bool useWeeklySchedule = configManager ? configManager->getBool("use_weekly_schedule", true) : true;
        int expectedHour = 18; // Default
        int expectedMinute = 0; // Default
        
        if (useWeeklySchedule && configManager) {
            int dayOfWeek = currentTime.dayOfTheWeek();
            const char* dayNames[] = {"sunday", "monday", "tuesday", "wednesday", "thursday", "friday", "saturday"};
            cJSON* weeklySchedule = configManager->getSection("weekly_schedule");
            if (weeklySchedule) {
                cJSON* todaySchedule = cJSON_GetObjectItem(weeklySchedule, dayNames[dayOfWeek]);
                if (todaySchedule) {
                    cJSON* alarm1Config = cJSON_GetObjectItem(todaySchedule, "alarm1");
                    if (alarm1Config) {
                        expectedHour = cJSON_GetObjectItem(alarm1Config, "hour") ? cJSON_GetObjectItem(alarm1Config, "hour")->valueint : 18;
                        expectedMinute = cJSON_GetObjectItem(alarm1Config, "minute") ? cJSON_GetObjectItem(alarm1Config, "minute")->valueint : 0;
                    }
                }
            }
        }
        
        // Only trigger if current time is within ±2 minutes of the expected alarm time
        int currentTotal = currentTime.hour() * 60 + currentTime.minute();
        int expectedTotal = expectedHour * 60 + expectedMinute;
        int diff = abs(currentTotal - expectedTotal);
        if (diff <= 2) {
            if (diagnosticManager) {
                diagnosticManager->log(DiagnosticManager::LOG_INFO, "Time", "Alarm 1 triggered at %02d:%02d (expected %02d:%02d, diff=%d min) - INT/SQW should go LOW", 
                                       currentTime.hour(), currentTime.minute(), expectedHour, expectedMinute, diff);
            }
            alarm1Active = true;
            // DON'T clear alarm1 yet - this keeps INT/SQW LOW
        } else {
            // False alarm - clear it without processing
            if (diagnosticManager) {
                diagnosticManager->log(DiagnosticManager::LOG_DEBUG, "Time", "Alarm 1 flag set but time mismatch (%02d:%02d vs expected %02d:%02d, diff=%d min) - clearing flag", 
                                       currentTime.hour(), currentTime.minute(), expectedHour, expectedMinute, diff);
            }
            rtc.clearAlarm(1);
        }
    }
    
    if (alarm2Triggered) {
        if (diagnosticManager) {
            diagnosticManager->log(DiagnosticManager::LOG_INFO, "Time", "Alarm 2 triggered - clearing both alarms, INT/SQW should go HIGH");
        }
        // Clear both alarms when alarm2 triggers - this makes INT/SQW go HIGH
        rtc.clearAlarm(1);
        rtc.clearAlarm(2);
        alarm1Active = false;
    }
    
    // Check if interrupt pin state changed and update relay
    if (pollInterruptPin()) {
        updateRelayFromIntSqw();
    }
}
