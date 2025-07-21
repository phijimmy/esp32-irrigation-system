
#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <Arduino.h>
#include "filesystem/FileSystemManager.h"
#include <cJSON.h>

class DiagnosticManager; // Forward declaration

class ConfigManager {
public:
    // Sunday watering flag
    bool sundayWatering = false;
    bool getSundayWatering() const { return sundayWatering; }
    void setSundayWatering(bool value) { sundayWatering = value; }
    void setRoot(cJSON* newRoot);
    // Setting: remove config file (default false)
    bool getRemoveConfigFile() const { return removeConfigFile; }
    void setRemoveConfigFile(bool value) { removeConfigFile = value; }
    bool begin(FileSystemManager& fsManager, DiagnosticManager* diag = nullptr);
    // Accessors for config reset endpoint
    const char* getConfigPath() const { return configPath; }
    FileSystemManager* getFileSystemManager() const { return fsManager; }
    void setDiagnosticManager(DiagnosticManager* diag);
    bool load();
    bool save();
    void resetToDefaults();
    const char* get(const char* key);
    int getInt(const char* key, int defaultValue = 0);
    bool getBool(const char* key, bool defaultValue = false);
    void setBool(const char* key, bool value); // Added to support RTC migration flag
    void set(const char* key, const char* value);
    cJSON* getSection(const char* section); // Returns a cJSON object for a config section
    cJSON* getRoot() { return configRoot; } // Returns the root cJSON object for direct access

private:
    bool removeConfigFile = false;
    void loadDefaults();
    void mergeDefaults(); // Merge missing keys from defaults into existing config
    cJSON* configRoot = nullptr;
    FileSystemManager* fsManager = nullptr;
    DiagnosticManager* diagnosticManager = nullptr;
    const char* configPath = "/config.json";
};


#endif // CONFIG_MANAGER_H
