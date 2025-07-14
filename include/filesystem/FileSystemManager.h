#ifndef FILE_SYSTEM_MANAGER_H
#define FILE_SYSTEM_MANAGER_H

#include <Arduino.h>
#include <FS.h>
#include <LittleFS.h>
#include <cJSON.h> // Include cJSON library for JSON handling

class DiagnosticManager; // Forward declaration

class FileSystemManager {
public:
    bool begin(DiagnosticManager* diag = nullptr);
    void setDiagnosticManager(DiagnosticManager* diag);
    bool format();
    bool exists(const char* path);
    String readFile(const char* path);
    bool writeFile(const char* path, const String& data);
    bool removeFile(const char* path);
    cJSON* getFileSystemInfoJson(); // Returns file system info as a cJSON object
    // Deletes config.json from the filesystem. Returns true if deleted, false otherwise.
    bool deleteConfigJson();
private:
    DiagnosticManager* diagnosticManager = nullptr;
};

#endif // FILE_SYSTEM_MANAGER_H
