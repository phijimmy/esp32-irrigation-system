#include "config/ConfigManager.h"
#include "diagnostics/DiagnosticManager.h"

void ConfigManager::setRoot(cJSON* newRoot) {
    if (configRoot) cJSON_Delete(configRoot);
    configRoot = newRoot;
}

bool ConfigManager::begin(FileSystemManager& fsMgr, DiagnosticManager* diag) {
    fsManager = &fsMgr;
    diagnosticManager = diag;
    // Load defaults, then try to load config file if it exists
    loadDefaults();
    load();
    // ...removed automatic config.json deletion...
    return true;
}

bool ConfigManager::load() {
    if (configRoot) {
        cJSON_Delete(configRoot);
        configRoot = nullptr;
    }
    if (fsManager && fsManager->exists(configPath)) {
        String content = fsManager->readFile(configPath);
        if (content.length() > 0) {
            if (diagnosticManager) diagnosticManager->log(DiagnosticManager::LOG_DEBUG, "Config", "Loaded config: %s", content.c_str());
            configRoot = cJSON_Parse(content.c_str());
            if (configRoot) {
                // Remove all duplicate ap_timeout and brownout_threshold keys (string or number)
                // Always keep only the last occurrence as a number
                double apTimeoutVal = 1800;
                double brownoutVal = 2.5;
                cJSON* apTimeout = NULL;
                cJSON* brownout = NULL;
                cJSON* item = NULL;
                cJSON_ArrayForEach(item, configRoot) {
                    if (item->string && strcmp(item->string, "ap_timeout") == 0) {
                        if (cJSON_IsNumber(item)) apTimeoutVal = item->valuedouble;
                        else if (cJSON_IsString(item)) apTimeoutVal = atof(item->valuestring);
                    }
                    if (item->string && strcmp(item->string, "brownout_threshold") == 0) {
                        if (cJSON_IsNumber(item)) brownoutVal = item->valuedouble;
                        else if (cJSON_IsString(item)) brownoutVal = atof(item->valuestring);
                    }
                }
                // Remove all occurrences
                while ((apTimeout = cJSON_GetObjectItemCaseSensitive(configRoot, "ap_timeout"))) {
                    cJSON_DeleteItemFromObjectCaseSensitive(configRoot, "ap_timeout");
                }
                while ((brownout = cJSON_GetObjectItemCaseSensitive(configRoot, "brownout_threshold"))) {
                    cJSON_DeleteItemFromObjectCaseSensitive(configRoot, "brownout_threshold");
                }
                // Add back as number
                cJSON_AddNumberToObject(configRoot, "ap_timeout", apTimeoutVal);
                cJSON_AddNumberToObject(configRoot, "brownout_threshold", brownoutVal);
                // Merge missing keys from defaults
                mergeDefaults();
                return true;
            } else {
                if (diagnosticManager) diagnosticManager->log(DiagnosticManager::LOG_WARN, "Config", "Failed to parse config, loading defaults");
            }
        }
    } else {
        if (diagnosticManager) diagnosticManager->log(DiagnosticManager::LOG_INFO, "Config", "Config file does not exist, loading defaults");
    }
    // If file does not exist or failed to parse, use hardcoded defaults
    loadDefaults();
    return false;
}

bool ConfigManager::save() {
    if (!configRoot) return false;
    char* content = cJSON_PrintUnformatted(configRoot);
    if (!content) {
        if (diagnosticManager) diagnosticManager->log(DiagnosticManager::LOG_ERROR, "Config", "Failed to serialize configRoot to JSON");
        return false;
    }
    bool result = fsManager->writeFile(configPath, content);
    cJSON_free(content);
    if (diagnosticManager) {
        if (result) {
            diagnosticManager->log(DiagnosticManager::LOG_INFO, "Config", "Saved config to %s", configPath);
        } else {
            diagnosticManager->log(DiagnosticManager::LOG_ERROR, "Config", "Failed to write config to %s", configPath);
        }
    }
    return result;
}

void ConfigManager::resetToDefaults() {
    loadDefaults();
    save();
}

void ConfigManager::loadDefaults() {
    if (configRoot) cJSON_Delete(configRoot);
    configRoot = cJSON_CreateObject();
    
    // Basic device settings
    cJSON_AddNumberToObject(configRoot, "led_gpio", 23);
    cJSON_AddNumberToObject(configRoot, "led_blink_rate", 500);
    cJSON_AddNumberToObject(configRoot, "touch_gpio", 4);
    cJSON_AddNumberToObject(configRoot, "touch_long_press", 5000);
    cJSON_AddNumberToObject(configRoot, "int_sqw_gpio", 27); // DS3231 INT/SQW pin GPIO for interrupt polling
    
    
    // 7-day alarm schedule
    cJSON* weeklySchedule = cJSON_CreateObject();
    
    // Create schedule for each day (0=Sunday, 1=Monday, ..., 6=Saturday)
    const char* dayNames[] = {"sunday", "monday", "tuesday", "wednesday", "thursday", "friday", "saturday"};
    for (int day = 0; day < 7; day++) {
        cJSON* daySchedule = cJSON_CreateObject();
        
        // Alarm1 settings for this day (always 18:00)
        cJSON* dayAlarm1 = cJSON_CreateObject();
        cJSON_AddNumberToObject(dayAlarm1, "hour", 18);
        cJSON_AddNumberToObject(dayAlarm1, "minute", 0);
        cJSON_AddNumberToObject(dayAlarm1, "second", 0);
        cJSON_AddBoolToObject(dayAlarm1, "enabled", true);
        cJSON_AddItemToObject(daySchedule, "alarm1", dayAlarm1);
        
        // Alarm2 settings for this day (always 22:00)
        cJSON* dayAlarm2 = cJSON_CreateObject();
        cJSON_AddNumberToObject(dayAlarm2, "hour", 22);
        cJSON_AddNumberToObject(dayAlarm2, "minute", 0);
        cJSON_AddBoolToObject(dayAlarm2, "enabled", true); // All days enabled
        cJSON_AddItemToObject(daySchedule, "alarm2", dayAlarm2);
        
        cJSON_AddItemToObject(weeklySchedule, dayNames[day], daySchedule);
    }
    cJSON_AddItemToObject(configRoot, "weekly_schedule", weeklySchedule);
    
    // Schedule settings
    cJSON_AddBoolToObject(configRoot, "use_weekly_schedule", true); // Use 7-day schedule instead of single alarms
    cJSON_AddBoolToObject(configRoot, "auto_update_daily", true); // Auto-update alarms at midnight
    
    // Legacy string-based configs for backward compatibility
    cJSON_AddStringToObject(configRoot, "wifi_mode", "ap"); // "ap" for Access Point, "client" for WiFi Client
    cJSON_AddStringToObject(configRoot, "wifi_ssid", "M1M-LS-MR-82");
    cJSON_AddStringToObject(configRoot, "wifi_pass", "BqY3#sTgKu$Ve5D2cMhAw[FnXrPz(8J7");
    cJSON_AddStringToObject(configRoot, "wifi_reconnect_interval", "60"); // seconds between reconnection attempts
    cJSON_AddStringToObject(configRoot, "wifi_max_reconnect_attempts", "5"); // max attempts before fallback to AP
    cJSON_AddStringToObject(configRoot, "device_name", "esp32_device");
    cJSON_AddStringToObject(configRoot, "debug_level", "3"); // LOG_INFO default
    cJSON_AddNumberToObject(configRoot, "touch_threshold", 40);
    // Relay defaults
    cJSON_AddNumberToObject(configRoot, "relay_count", 4);
    cJSON_AddNumberToObject(configRoot, "relay_gpio_0", 32);
    cJSON_AddNumberToObject(configRoot, "relay_gpio_1", 33);
    cJSON_AddNumberToObject(configRoot, "relay_gpio_2", 25);
    cJSON_AddNumberToObject(configRoot, "relay_gpio_3", 26);
    cJSON_AddBoolToObject(configRoot, "relay_active_high_0", true);
    cJSON_AddBoolToObject(configRoot, "relay_active_high_1", true);
    cJSON_AddBoolToObject(configRoot, "relay_active_high_2", true);
    cJSON_AddBoolToObject(configRoot, "relay_active_high_3", true);
    // Relay names (default: Zone 1-4)
    static const char* defaultRelayNames[4] = {"Zone 1", "Zone 2", "Zone 3", "Zone 4"};
    cJSON* relayNames = cJSON_CreateStringArray(defaultRelayNames, 4);
    cJSON_AddItemToObject(configRoot, "relay_names", relayNames);
    // GPIO for manual/interrupt control of relay 2
    cJSON_AddNumberToObject(configRoot, "relay2_control_gpio", 18); // GPIO 18 for relay 2 manual control
    // Power/brownout defaults
    cJSON_AddNumberToObject(configRoot, "brownout_threshold", 2.5);
    cJSON_AddNumberToObject(configRoot, "cpu_speed", 160);
    // Network/AP defaults
    cJSON_AddStringToObject(configRoot, "ap_ssid", "ESP32-Iot-DeV");
    cJSON_AddStringToObject(configRoot, "ap_password", "irrigation123");
    cJSON_AddNumberToObject(configRoot, "ap_timeout", 300); // 5 minutes (300 seconds)
    // I2C defaults
    cJSON_AddNumberToObject(configRoot, "i2c_sda", 21);
    cJSON_AddNumberToObject(configRoot, "i2c_scl", 22);
    // Time defaults
    cJSON_AddStringToObject(configRoot, "default_time", "");
    // NTP server configuration
    cJSON_AddStringToObject(configRoot, "ntp_server_1", "192.168.1.1"); // Primary NTP server (local router)
    cJSON_AddStringToObject(configRoot, "ntp_server_2", "0.de.pool.ntp.org"); // Secondary NTP server (German pool)
    cJSON_AddBoolToObject(configRoot, "ntp_enabled", true); // Enable NTP synchronization
    cJSON_AddNumberToObject(configRoot, "ntp_sync_interval", 3600); // Sync every hour (3600 seconds)
    cJSON_AddNumberToObject(configRoot, "ntp_timeout", 5000); // NTP request timeout in milliseconds
    // Timezone and DST defaults (CEST/CET for Central European Time)
    cJSON_AddNumberToObject(configRoot, "timezone_offset", 1); // CET is UTC+1
    cJSON_AddBoolToObject(configRoot, "dst_enabled", true); // Enable automatic DST transitions
    cJSON_AddBoolToObject(configRoot, "dst_auto_adjust", true); // Enable automatic time adjustment during DST transitions
    cJSON_AddNumberToObject(configRoot, "dst_offset", 1); // DST adds 1 hour (CEST = UTC+2)
    cJSON_AddBoolToObject(configRoot, "rtc_stores_utc", false); // Flag to track RTC time format (false = legacy local time)
    cJSON_AddBoolToObject(configRoot, "force_build_time", true); // Force setting build time on first boot (fresh defaults)
    // DST rules for Europe (last Sunday in March to last Sunday in October)
    cJSON_AddNumberToObject(configRoot, "dst_start_month", 3); // March
    cJSON_AddNumberToObject(configRoot, "dst_start_week", -1); // Last week (-1 = last)
    cJSON_AddNumberToObject(configRoot, "dst_start_dow", 0); // Sunday (0=Sunday, 1=Monday, etc.)
    cJSON_AddNumberToObject(configRoot, "dst_start_hour", 2); // 2:00 AM
    cJSON_AddNumberToObject(configRoot, "dst_end_month", 10); // October
    cJSON_AddNumberToObject(configRoot, "dst_end_week", -1); // Last week
    cJSON_AddNumberToObject(configRoot, "dst_end_dow", 0); // Sunday
    cJSON_AddNumberToObject(configRoot, "dst_end_hour", 3); // 3:00 AM (CEST time)
    
    
    // Soil moisture calibration values
    cJSON* soilMoisture = cJSON_CreateObject();
    cJSON_AddNumberToObject(soilMoisture, "wet", 4400); // 4400 = fully wet (glass of water)
    cJSON_AddNumberToObject(soilMoisture, "dry", 10700); // 10700 = fully dry
    cJSON_AddNumberToObject(soilMoisture, "stabilisation_time", 10); // 10 seconds default
    cJSON_AddItemToObject(configRoot, "soil_moisture", soilMoisture);

    // Soil moisture power control GPIO (default 16)
    cJSON_AddNumberToObject(configRoot, "soil_power_gpio", 16);

    // Watering config
    cJSON_AddNumberToObject(configRoot, "watering_threshold", 50.0); // percent
    cJSON_AddNumberToObject(configRoot, "watering_duration_sec", 60); // 1 minute for fast testing
    // Irrigation schedule config
    cJSON_AddNumberToObject(configRoot, "irrigation_scheduled_hour", 11); // Set to 13:00
    cJSON_AddNumberToObject(configRoot, "irrigation_scheduled_minute", 30); // Set to 13:15
}

void ConfigManager::mergeDefaults() {
    if (!configRoot) return;
    
    // Helper function to merge missing keys from defaults
    auto mergeIfMissing = [this](const char* key, cJSON* defaultValue) {
        if (!cJSON_HasObjectItem(configRoot, key)) {
            cJSON* copy = cJSON_Duplicate(defaultValue, true);
            cJSON_AddItemToObject(configRoot, key, copy);
            if (diagnosticManager) {
                diagnosticManager->log(DiagnosticManager::LOG_INFO, "Config", 
                    "Added missing config section: %s", key);
            }
        }
    };
    
    // Create temporary defaults object to get missing sections
    cJSON* tempDefaults = cJSON_CreateObject();
    
    // Basic device settings
    if (!cJSON_HasObjectItem(configRoot, "led_gpio")) {
        cJSON_AddNumberToObject(configRoot, "led_gpio", 23);
    }
    if (!cJSON_HasObjectItem(configRoot, "led_blink_rate")) {
        cJSON_AddNumberToObject(configRoot, "led_blink_rate", 500);
    }
    if (!cJSON_HasObjectItem(configRoot, "touch_gpio")) {
        cJSON_AddNumberToObject(configRoot, "touch_gpio", 4);
    }
    if (!cJSON_HasObjectItem(configRoot, "touch_long_press")) {
        cJSON_AddNumberToObject(configRoot, "touch_long_press", 5000);
    }
    if (!cJSON_HasObjectItem(configRoot, "int_sqw_gpio")) {
        cJSON_AddNumberToObject(configRoot, "int_sqw_gpio", 27);
    }
    
    // Soil moisture sensor config
    if (!cJSON_HasObjectItem(configRoot, "soil_moisture")) {
        cJSON* soilMoisture = cJSON_CreateObject();
        cJSON_AddNumberToObject(soilMoisture, "ads_channel", 0);
        cJSON_AddNumberToObject(soilMoisture, "gain", 0); // GAIN_TWOTHIRDS = ±6.144V range (won't saturate at 2.1V)
        cJSON_AddNumberToObject(soilMoisture, "stabilisation_time", 10); // 10 seconds
        cJSON_AddNumberToObject(soilMoisture, "wet", 4400); // 4400 = fully wet (glass of water)
        cJSON_AddNumberToObject(soilMoisture, "dry", 10700); // 10700 = fully dry
        cJSON_AddItemToObject(configRoot, "soil_moisture", soilMoisture);
        if (diagnosticManager) {
            diagnosticManager->log(DiagnosticManager::LOG_INFO, "Config", 
                "Added missing config section: soil_moisture");
        }
    }
    
    // MQ135 sensor config
    if (!cJSON_HasObjectItem(configRoot, "mq135")) {
        cJSON* mq135 = cJSON_CreateObject();
        cJSON_AddNumberToObject(mq135, "ads_channel", 1);
        cJSON_AddNumberToObject(mq135, "gain", 0);
        cJSON_AddNumberToObject(mq135, "warmup_time", 60);
        cJSON_AddItemToObject(configRoot, "mq135", mq135);
        if (diagnosticManager) {
            diagnosticManager->log(DiagnosticManager::LOG_INFO, "Config", 
                "Added missing config section: mq135");
        }
    }
    
    // Alarm1 config
    if (!cJSON_HasObjectItem(configRoot, "alarm1")) {
        cJSON* alarm1 = cJSON_CreateObject();
        cJSON_AddNumberToObject(alarm1, "hour", 10);
        cJSON_AddNumberToObject(alarm1, "minute", 30);
        cJSON_AddNumberToObject(alarm1, "second", 0);
        cJSON_AddBoolToObject(alarm1, "enabled", true);
        cJSON_AddItemToObject(configRoot, "alarm1", alarm1);
        if (diagnosticManager) {
            diagnosticManager->log(DiagnosticManager::LOG_INFO, "Config", 
                "Added missing config section: alarm1");
        }
    }
    
    // Alarm2 config
    if (!cJSON_HasObjectItem(configRoot, "alarm2")) {
        cJSON* alarm2 = cJSON_CreateObject();
        cJSON_AddNumberToObject(alarm2, "hour", 10);
        cJSON_AddNumberToObject(alarm2, "minute", 35);
        cJSON_AddBoolToObject(alarm2, "enabled", true);
        cJSON_AddItemToObject(configRoot, "alarm2", alarm2);
        if (diagnosticManager) {
            diagnosticManager->log(DiagnosticManager::LOG_INFO, "Config", 
                "Added missing config section: alarm2");
        }
    }
    
    // Weekly schedule defaults
    if (!cJSON_HasObjectItem(configRoot, "weekly_schedule")) {
        cJSON* weeklySchedule = cJSON_CreateObject();
        const char* dayNames[] = {"sunday", "monday", "tuesday", "wednesday", "thursday", "friday", "saturday"};
        for (int day = 0; day < 7; day++) {
            cJSON* daySchedule = cJSON_CreateObject();
            cJSON* dayAlarm1 = cJSON_CreateObject();
            cJSON_AddNumberToObject(dayAlarm1, "hour", 18);
            cJSON_AddNumberToObject(dayAlarm1, "second", 0);
            cJSON_AddBoolToObject(dayAlarm1, "enabled", true);
            cJSON_AddItemToObject(daySchedule, "alarm1", dayAlarm1);
            cJSON* dayAlarm2 = cJSON_CreateObject();
            cJSON_AddNumberToObject(dayAlarm2, "minute", 0);
            cJSON_AddBoolToObject(dayAlarm2, "enabled", true);
            cJSON_AddItemToObject(daySchedule, "alarm2", dayAlarm2);
            cJSON_AddItemToObject(weeklySchedule, dayNames[day], daySchedule);
        }
        cJSON_AddItemToObject(configRoot, "weekly_schedule", weeklySchedule);
        if (diagnosticManager) {
            diagnosticManager->log(DiagnosticManager::LOG_INFO, "Config", 
                "Added missing config section: weekly_schedule");
        }
    }
    // Add these keys only once, outside the loop
    if (!cJSON_HasObjectItem(configRoot, "brownout_threshold")) {
        cJSON_AddNumberToObject(configRoot, "brownout_threshold", 2.5);
    }
    if (!cJSON_HasObjectItem(configRoot, "cpu_speed")) {
        cJSON_AddNumberToObject(configRoot, "cpu_speed", 160);
    }
    if (!cJSON_HasObjectItem(configRoot, "ap_timeout")) {
        cJSON_AddNumberToObject(configRoot, "ap_timeout", 300);
    }
    
    // Schedule control flags
    if (!cJSON_HasObjectItem(configRoot, "use_weekly_schedule")) {
        cJSON_AddBoolToObject(configRoot, "use_weekly_schedule", true);
    }
    if (!cJSON_HasObjectItem(configRoot, "auto_update_daily")) {
        cJSON_AddBoolToObject(configRoot, "auto_update_daily", true);
    }
    
    // Legacy string-based configs for backward compatibility
    if (!cJSON_HasObjectItem(configRoot, "wifi_mode")) {
        cJSON_AddStringToObject(configRoot, "wifi_mode", "ap");
    }
    if (!cJSON_HasObjectItem(configRoot, "wifi_ssid")) {
        cJSON_AddStringToObject(configRoot, "wifi_ssid", "M1M-LS-MR-82");
    }
    if (!cJSON_HasObjectItem(configRoot, "wifi_pass")) {
        cJSON_AddStringToObject(configRoot, "wifi_pass", "BqY3#sTgKu$Ve5D2cMhAw[FnXrPz(8J7");
    }
    if (!cJSON_HasObjectItem(configRoot, "wifi_reconnect_interval")) {
        cJSON_AddStringToObject(configRoot, "wifi_reconnect_interval", "60");
    }
    if (!cJSON_HasObjectItem(configRoot, "wifi_max_reconnect_attempts")) {
        cJSON_AddStringToObject(configRoot, "wifi_max_reconnect_attempts", "5");
    }
    if (!cJSON_HasObjectItem(configRoot, "device_name")) {
        cJSON_AddStringToObject(configRoot, "device_name", "esp32_device");
    }
    if (!cJSON_HasObjectItem(configRoot, "debug_level")) {
        cJSON_AddStringToObject(configRoot, "debug_level", "3");
    }
    if (!cJSON_HasObjectItem(configRoot, "touch_threshold")) {
        cJSON_AddNumberToObject(configRoot, "touch_threshold", 40);
    }
    // Do not add brownout_threshold here; handled in load()
    
    // Add other missing keys as needed
    if (!cJSON_HasObjectItem(configRoot, "relay_count")) {
        cJSON_AddNumberToObject(configRoot, "relay_count", 4);
    }
    if (!cJSON_HasObjectItem(configRoot, "relay_gpio_0")) {
        cJSON_AddNumberToObject(configRoot, "relay_gpio_0", 32);
    }
    if (!cJSON_HasObjectItem(configRoot, "relay_gpio_1")) {
        cJSON_AddNumberToObject(configRoot, "relay_gpio_1", 33);
    }
    if (!cJSON_HasObjectItem(configRoot, "relay_gpio_2")) {
        cJSON_AddNumberToObject(configRoot, "relay_gpio_2", 25);
    }
    if (!cJSON_HasObjectItem(configRoot, "relay_gpio_3")) {
        cJSON_AddNumberToObject(configRoot, "relay_gpio_3", 26);
    }
    if (!cJSON_HasObjectItem(configRoot, "relay_active_high_0")) {
        cJSON_AddBoolToObject(configRoot, "relay_active_high_0", true);
    }
    if (!cJSON_HasObjectItem(configRoot, "relay_active_high_1")) {
        cJSON_AddBoolToObject(configRoot, "relay_active_high_1", true);
    }
    if (!cJSON_HasObjectItem(configRoot, "relay_active_high_2")) {
        cJSON_AddBoolToObject(configRoot, "relay_active_high_2", true);
    }
    if (!cJSON_HasObjectItem(configRoot, "relay_active_high_3")) {
        cJSON_AddBoolToObject(configRoot, "relay_active_high_3", true);
    }
    if (!cJSON_HasObjectItem(configRoot, "ap_ssid")) {
        cJSON_AddStringToObject(configRoot, "ap_ssid", "ESP32-Iot-DeV");
    }
    if (!cJSON_HasObjectItem(configRoot, "ap_password")) {
        cJSON_AddStringToObject(configRoot, "ap_password", "irrigation123");
    }
    // Do not add ap_timeout here; handled in load()
    
    // NTP configuration defaults
    if (!cJSON_HasObjectItem(configRoot, "ntp_server_1")) {
        cJSON_AddStringToObject(configRoot, "ntp_server_1", "192.168.1.1");
    }
    if (!cJSON_HasObjectItem(configRoot, "ntp_server_2")) {
        cJSON_AddStringToObject(configRoot, "ntp_server_2", "0.de.pool.ntp.org");
    }
    if (!cJSON_HasObjectItem(configRoot, "ntp_enabled")) {
        cJSON_AddBoolToObject(configRoot, "ntp_enabled", true);
    }
    if (!cJSON_HasObjectItem(configRoot, "ntp_sync_interval")) {
        cJSON_AddNumberToObject(configRoot, "ntp_sync_interval", 3600);
    }
    if (!cJSON_HasObjectItem(configRoot, "ntp_timeout")) {
        cJSON_AddNumberToObject(configRoot, "ntp_timeout", 5000);
    }
    
    // Timezone and DST defaults
    if (!cJSON_HasObjectItem(configRoot, "timezone_offset")) {
        cJSON_AddNumberToObject(configRoot, "timezone_offset", 1);
    }
    if (!cJSON_HasObjectItem(configRoot, "dst_enabled")) {
        cJSON_AddBoolToObject(configRoot, "dst_enabled", true);
    }
    if (!cJSON_HasObjectItem(configRoot, "dst_auto_adjust")) {
        cJSON_AddBoolToObject(configRoot, "dst_auto_adjust", true);
    }
    if (!cJSON_HasObjectItem(configRoot, "rtc_stores_utc")) {
        cJSON_AddBoolToObject(configRoot, "rtc_stores_utc", false);
    }
    if (!cJSON_HasObjectItem(configRoot, "force_build_time")) {
        cJSON_AddBoolToObject(configRoot, "force_build_time", false);
    }
    if (!cJSON_HasObjectItem(configRoot, "dst_offset")) {
        cJSON_AddNumberToObject(configRoot, "dst_offset", 1);
    }
    if (!cJSON_HasObjectItem(configRoot, "dst_start_month")) {
        cJSON_AddNumberToObject(configRoot, "dst_start_month", 3);
    }
    if (!cJSON_HasObjectItem(configRoot, "dst_start_week")) {
        cJSON_AddNumberToObject(configRoot, "dst_start_week", -1);
    }
    if (!cJSON_HasObjectItem(configRoot, "dst_start_dow")) {
        cJSON_AddNumberToObject(configRoot, "dst_start_dow", 0);
    }
    if (!cJSON_HasObjectItem(configRoot, "dst_start_hour")) {
        cJSON_AddNumberToObject(configRoot, "dst_start_hour", 2);
    }
    if (!cJSON_HasObjectItem(configRoot, "dst_end_month")) {
        cJSON_AddNumberToObject(configRoot, "dst_end_month", 10);
    }
    if (!cJSON_HasObjectItem(configRoot, "dst_end_week")) {
        cJSON_AddNumberToObject(configRoot, "dst_end_week", -1);
    }
    if (!cJSON_HasObjectItem(configRoot, "dst_end_dow")) {
        cJSON_AddNumberToObject(configRoot, "dst_end_dow", 0);
    }
    if (!cJSON_HasObjectItem(configRoot, "dst_end_hour")) {
        cJSON_AddNumberToObject(configRoot, "dst_end_hour", 3);
    }
    
    // Environment Manager configuration defaults
    if (!cJSON_HasObjectItem(configRoot, "environment_enabled")) {
        cJSON_AddBoolToObject(configRoot, "environment_enabled", true);
    }
    if (!cJSON_HasObjectItem(configRoot, "environment_scheduled_hour")) {
        cJSON_AddNumberToObject(configRoot, "environment_scheduled_hour", 13);
    }
    if (!cJSON_HasObjectItem(configRoot, "environment_scheduled_minute")) {
        cJSON_AddNumberToObject(configRoot, "environment_scheduled_minute", 5);
    }
    
    cJSON_Delete(tempDefaults);
}

void ConfigManager::set(const char* key, const char* value) {
    if (!configRoot) return;
    // For ap_timeout and brownout_threshold, only ever allow a single number value, never a string, not even temporarily
    if (strcmp(key, "ap_timeout") == 0 || strcmp(key, "brownout_threshold") == 0) {
        // Remove all occurrences (string or number)
        while (cJSON_GetObjectItemCaseSensitive(configRoot, key)) {
            cJSON_DeleteItemFromObjectCaseSensitive(configRoot, key);
        }
        double numVal = atof(value);
        cJSON_AddNumberToObject(configRoot, key, numVal);
        return;
    }
    cJSON* item = cJSON_GetObjectItemCaseSensitive(configRoot, key);
    if (item) {
        // Only set string value for non-special keys
        cJSON_SetValuestring(item, value);
    } else {
        cJSON_AddStringToObject(configRoot, key, value);
    }
}

const char* ConfigManager::get(const char* key) {
    if (!configRoot) return "";
    cJSON* item = cJSON_GetObjectItemCaseSensitive(configRoot, key);
    // Special handling for ap_timeout and brownout_threshold: always return as string, but store as number
    if (strcmp(key, "ap_timeout") == 0 || strcmp(key, "brownout_threshold") == 0) {
        // Remove all string versions if present (should not exist, but for safety)
        while (item && cJSON_IsString(item)) {
            cJSON_DeleteItemFromObjectCaseSensitive(configRoot, key);
            item = cJSON_GetObjectItemCaseSensitive(configRoot, key);
        }
        if (!item) {
            // Add default if missing
            double defVal = (strcmp(key, "ap_timeout") == 0) ? 1800 : 2.5;
            cJSON_AddNumberToObject(configRoot, key, defVal);
            item = cJSON_GetObjectItemCaseSensitive(configRoot, key);
        }
        if (cJSON_IsNumber(item)) {
            static char numBuf[32];
            snprintf(numBuf, sizeof(numBuf), "%g", item->valuedouble);
            return numBuf;
        }
        return "";
    }
    if (cJSON_IsString(item) && (item->valuestring != nullptr)) {
        return item->valuestring;
    }
    // If missing, add from defaults if known
    if (strcmp(key, "ap_ssid") == 0) {
        cJSON_AddStringToObject(configRoot, "ap_ssid", "ESP32-Iot-DeV");
        return "ESP32-Iot-DeV";
    }
    if (strcmp(key, "ap_password") == 0) {
        cJSON_AddStringToObject(configRoot, "ap_password", "irrigation123");
        return "irrigation123";
    }
    // ...add more defaults as needed...
    return "";
}

void ConfigManager::setBool(const char* key, bool value) {
    if (!configRoot) return;
    cJSON* item = cJSON_GetObjectItemCaseSensitive(configRoot, key);
    if (item) {
        // Replace existing item
        cJSON_SetBoolValue(item, value);
    } else {
        // Add new item
        cJSON_AddBoolToObject(configRoot, key, value);
    }
}

void ConfigManager::setDiagnosticManager(DiagnosticManager* diag) {
    diagnosticManager = diag;
}

cJSON* ConfigManager::getSection(const char* section) {
    if (!configRoot) return nullptr;
    return cJSON_GetObjectItem(configRoot, section);
}

int ConfigManager::getInt(const char* key, int defaultValue) {
    if (!configRoot) return defaultValue;
    cJSON* item = cJSON_GetObjectItemCaseSensitive(configRoot, key);
    if (cJSON_IsNumber(item)) {
        return item->valueint;
    }
    // If it's a string, try to parse it as an integer
    if (cJSON_IsString(item) && (item->valuestring != nullptr)) {
        return atoi(item->valuestring);
    }
    return defaultValue;
}

bool ConfigManager::getBool(const char* key, bool defaultValue) {
    if (!configRoot) return defaultValue;
    cJSON* item = cJSON_GetObjectItemCaseSensitive(configRoot, key);
    if (cJSON_IsBool(item)) {
        return cJSON_IsTrue(item);
    }
    // If it's a string, try to parse it as a boolean
    if (cJSON_IsString(item) && (item->valuestring != nullptr)) {
        return (strcmp(item->valuestring, "1") == 0 || strcasecmp(item->valuestring, "true") == 0);
    }
    // If it's a number, treat non-zero as true
    if (cJSON_IsNumber(item)) {
        return item->valueint != 0;
    }
    return defaultValue;
}
