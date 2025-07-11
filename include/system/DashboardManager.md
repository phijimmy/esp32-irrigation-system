# DashboardManager

The `DashboardManager` centralizes system status and data for dashboard or API use. It provides JSON-formatted status on demand, using cJSON and v7-compliant output.

## Features
- Returns current datetime in ISO 8601 format
- Returns human-readable date (e.g., Thursday 10th July 2025)
- Returns human-readable time (e.g., 16:22:00)
- Designed for easy extension with more system/device info

## Usage Example
```cpp
#include "system/DashboardManager.h"
DashboardManager dashboard(&timeManager, &configManager, &diagnosticManager);
dashboard.begin();
String json = dashboard.getStatusString();
Serial.println(json);
```

## API
- `cJSON* getStatusJson()`: Returns a cJSON object with status info.
- `String getStatusString()`: Returns the status as a JSON string.

## Dependencies
- TimeManager (for current time)
- ConfigManager (for future extensions)
- DiagnosticManager (optional, for logging)

## Extending
Add more fields to `getStatusJson()` as needed for your dashboard or API.
