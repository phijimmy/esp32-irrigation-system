#include "diagnostics/DiagnosticManager.h"
#include "filesystem/FileSystemManager.h"

bool FileSystemManager::deleteConfigJson() {
    const char* configPath = "/config.json";
    if (exists(configPath)) {
        return removeFile(configPath);
    }
    return false;
}
#include "diagnostics/DiagnosticManager.h"
#include "filesystem/FileSystemManager.h"

bool FileSystemManager::begin(DiagnosticManager* diag) {
    diagnosticManager = diag;
    if (!LittleFS.begin(false)) {
        if (diagnosticManager) diagnosticManager->log(DiagnosticManager::LOG_WARN, "FS", "LittleFS Mount Failed, formatting...");
        if (!LittleFS.format() || !LittleFS.begin(true)) {
            if (diagnosticManager) diagnosticManager->log(DiagnosticManager::LOG_ERROR, "FS", "LittleFS format or remount failed");
            return false;
        }
        if (diagnosticManager) diagnosticManager->log(DiagnosticManager::LOG_INFO, "FS", "LittleFS formatted and mounted successfully");
        return true;
    }
    if (diagnosticManager) diagnosticManager->log(DiagnosticManager::LOG_INFO, "FS", "LittleFS Mounted Successfully");
    return true;
}

void FileSystemManager::setDiagnosticManager(DiagnosticManager* diag) {
    diagnosticManager = diag;
}

bool FileSystemManager::format() {
    return LittleFS.format();
}

bool FileSystemManager::exists(const char* path) {
    return LittleFS.exists(path);
}

String FileSystemManager::readFile(const char* path) {
    File file = LittleFS.open(path, FILE_READ);
    if (!file) {
        if (diagnosticManager) diagnosticManager->log(DiagnosticManager::LOG_ERROR, "FS", "Failed to open file for reading: %s", path);
        return String();
    }
    String content = file.readString();
    file.close();
    return content;
}

bool FileSystemManager::writeFile(const char* path, const String& data) {
    File file = LittleFS.open(path, FILE_WRITE);
    if (!file) {
        if (diagnosticManager) diagnosticManager->log(DiagnosticManager::LOG_ERROR, "FS", "Failed to open file for writing: %s", path);
        return false;
    }
    if (data.length() == 0) {
        if (diagnosticManager) diagnosticManager->log(DiagnosticManager::LOG_ERROR, "FS", "No data provided to write to file: %s", path);
        file.close();
        return false;
    }
    file.print(data);
    file.close();
    return true;
}

bool FileSystemManager::removeFile(const char* path) {
    return LittleFS.remove(path);
}

cJSON* FileSystemManager::getFileSystemInfoJson() {
    cJSON* info = cJSON_CreateObject();
    size_t total = LittleFS.totalBytes();
    size_t used = LittleFS.usedBytes();
    cJSON_AddNumberToObject(info, "total_bytes", (double)total);
    cJSON_AddNumberToObject(info, "used_bytes", (double)used);
    cJSON_AddNumberToObject(info, "free_bytes", (double)(total - used));
    return info;
}
