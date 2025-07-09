#ifndef TIME_MANAGER_H
#define TIME_MANAGER_H

#include <Arduino.h>
#include <RTClib.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include "system/I2CManager.h"
#include "config/ConfigManager.h"
#include "diagnostics/DiagnosticManager.h"

class RelayController; // Forward declaration

class TimeManager {
public:
    void begin(I2CManager* i2c, ConfigManager* config, DiagnosticManager* diag);
    void update();
    DateTime getTime();
    void setTime(const DateTime& dt);
    bool isRTCFound() const { return rtcFound; }

    // Alarm management
    bool setAlarm1(int hour, int minute, int second, bool enabled);
    bool setAlarm2(int hour, int minute, bool enabled);
    bool readAlarm1(int &hour, int &minute, int &second, bool &enabled);
    bool readAlarm2(int &hour, int &minute, bool &enabled);
    void clearAlarm1();
    void clearAlarm2();
    void updateAlarmsFromConfig();
    void handleAlarmInterrupt();
    void setIntSqwPin(bool high);
    bool getIntSqwPinState() const;
    void enableAlarmInterrupts(); // Enable alarm interrupts in DS3231 control register
    
    // GPIO interrupt polling support
    void initializeInterruptPin();
    bool pollInterruptPin(); // Returns true if interrupt detected
    bool getHardwareIntSqwState() const; // Returns current hardware GPIO state (true=HIGH, false=LOW)
    void setRelayController(class RelayController* controller) { relayController = controller; }
    void updateRelayFromIntSqw(); // Updates relay 1 based on INT/SQW state changes
    
    // DST and timezone support
    DateTime getLocalTime(); // Get time adjusted for timezone and DST
    bool isDSTActive(const DateTime& utcTime); // Check if DST is currently active
    DateTime calculateDSTStart(int year); // Calculate DST start date for given year
    DateTime calculateDSTEnd(int year); // Calculate DST end date for given year
    void updateDSTTransition(); // Check and handle DST transitions
    
    // NTP synchronization
    bool syncWithNTP(); // Sync RTC with NTP server
    void updateNTPSync(); // Check if NTP sync is needed and perform it
    void forceNTPSync(); // Force immediate NTP sync regardless of interval
    void onWiFiConnected(); // Call this when WiFi connects to trigger initial NTP sync
    bool isWiFiConnected(); // Check if WiFi is connected
    
    // Weekly schedule support
    void updateDailySchedule(); // Check if we need to update alarms for new day (call at midnight)
    void setAlarmsForToday(); // Set alarms based on current day's schedule
    String getCurrentDayName(); // Get current day name (e.g., "Monday")
    String getCurrentDateString(); // Get current date string (e.g., "Wednesday 9th July 2025")
    String getCurrentTimeString(); // Get current time string (e.g., "10:43:25")
    int getCurrentDayOfWeek(); // Get current day of week (0=Sunday, 1=Monday, etc.)
    bool isNewDay(); // Check if we've crossed midnight since last check

private:
    I2CManager* i2cManager = nullptr;
    ConfigManager* configManager = nullptr;
    DiagnosticManager* diagnosticManager = nullptr;
    RTC_DS3231 rtc;
    bool rtcFound = false;
    int intSqwGpio = 27; // Default GPIO for DS3231 INT/SQW pin
    bool lastIntSqwState = true; // Track previous state for edge detection
    bool lastRelayControlState = true; // Track previous state for relay control
    class RelayController* relayController = nullptr; // For INT/SQW relay control
    bool lastDSTState = false; // Track previous DST state for transition detection
    unsigned long lastDSTCheck = 0; // Last time we checked for DST transitions (millis)
    unsigned long lastNTPSync = 0; // Last time we performed NTP sync (millis)
    bool ntpInitialSyncDone = false; // Track if initial NTP sync was completed
    int lastDayId = -1; // Track last day ID (YYYYMMDD) to detect day changes
    bool alarm1Active = false; // Track if alarm1 has triggered and we're waiting for alarm2
    void setBuildTimeIfNeeded();
    DateTime buildTime();
    bool performNTPSync(const String& server, int timeoutMs); // Internal NTP sync method
};

#endif // TIME_MANAGER_H
