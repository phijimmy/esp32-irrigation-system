#include "diagnostics/DiagnosticManager.h"
#include "filesystem/FileSystemManager.h"

bool FileSystemManager::begin(DiagnosticManager* diag) {
    diagnosticManager = diag;
    if (!SPIFFS.begin(false)) {
        if (diagnosticManager) diagnosticManager->log(DiagnosticManager::LOG_WARN, "FS", "SPIFFS Mount Failed, formatting...");
        if (!SPIFFS.format() || !SPIFFS.begin(true)) {
            if (diagnosticManager) diagnosticManager->log(DiagnosticManager::LOG_ERROR, "FS", "SPIFFS format or remount failed");
            return false;
        }
        if (diagnosticManager) diagnosticManager->log(DiagnosticManager::LOG_INFO, "FS", "SPIFFS formatted and mounted successfully");
        return true;
    }
    if (diagnosticManager) diagnosticManager->log(DiagnosticManager::LOG_INFO, "FS", "SPIFFS Mounted Successfully");
    return true;
}

void FileSystemManager::setDiagnosticManager(DiagnosticManager* diag) {
    diagnosticManager = diag;
}

bool FileSystemManager::format() {
    return SPIFFS.format();
}

bool FileSystemManager::exists(const char* path) {
    return SPIFFS.exists(path);
}

String FileSystemManager::readFile(const char* path) {
    File file = SPIFFS.open(path, FILE_READ);
    if (!file) {
        if (diagnosticManager) diagnosticManager->log(DiagnosticManager::LOG_ERROR, "FS", "Failed to open file for reading: %s", path);
        return String();
    }
    String content = file.readString();
    file.close();
    return content;
}

bool FileSystemManager::writeFile(const char* path, const String& data) {
    File file = SPIFFS.open(path, FILE_WRITE);
    if (!file) {
        if (diagnosticManager) diagnosticManager->log(DiagnosticManager::LOG_ERROR, "FS", "Failed to open file for writing: %s", path);
        return false;
    }
    file.print(data);
    file.close();
    return true;
}

bool FileSystemManager::removeFile(const char* path) {
    return SPIFFS.remove(path);
}
