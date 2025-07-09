# ConfigManager

Centralizes all configuration for the ESP32 IoT platform. All defaults are set in code (no external JSON required). Supports runtime get/set for all config values.

- **Key Methods:**
  - `loadDefaults()`: Sets all default config values.
  - `getInt`, `getBool`, `get`, `set`, `setBool`: Access and modify config values.
  - `save()`, `load()`, `resetToDefaults()`: Manage config persistence (future support).
- **Configurable Items:**
  - WiFi, timezone, DST, NTP, device/sensor settings, alarm schedules, relay GPIOs, watering logic, etc.

---

# TimeManager

Handles all timekeeping, RTC, DST, NTP, and alarm logic.

- **RTC:** DS3231, always stores local time.
- **DST:** Automatic transitions, configurable offset, European rules by default.
- **NTP:** Periodic sync, configurable servers and interval.
- **Alarms:** Supports single and weekly schedule, INT/SQW pin controls relay 0.
- **Key Methods:**
  - `update()`: Main loop handler.
  - `setAlarm1`, `setAlarm2`, `updateAlarmsFromConfig`, `setAlarmsForToday`.
  - `getLocalTime()`, `isDSTActive()`, `calculateDSTStart/End()`.

---

# RelayController

Manages all relays (default 4), supports active high/low logic, integrates with config for GPIO assignment.

- **Key Methods:**
  - `setRelayMode(index, mode)`, `activateRelay(index)`, `deactivateRelay(index)`.
  - Loads relay GPIOs and logic from config.

---

# SoilMoistureSensor

Reads soil moisture via ADS1115 ADC. Supports calibration, percent calculation, timestamping, and non-blocking stabilization.

- **Calibration:** Configurable wet/dry ADC values.
- **Averaging:** 10 readings, outlier rejection, clamped to [0, 100]%.
- **Key Methods:**
  - `beginStabilisation()`, `readyForReading()`, `takeReading()`, `getLastReading()`.
  - `Reading` struct: `raw`, `voltage`, `percent`, `avgRaw`, `avgVoltage`, `avgPercent`, `timestamp`.

---

# MQ135Sensor

Air quality sensor using ADS1115. Powered via relay, supports warmup, averaging, and qualitative AQI label.

- **Warmup:** Configurable, non-blocking.
- **Averaging:** 10 readings, outlier rejection.
- **AQI Label:** `getAirQualityLabel(voltage)` returns "Excellent", "Good", "Moderate", "Poor", "Very Poor".
- **Key Methods:**
  - `startReading()`, `readyForReading()`, `takeReading()`, `getLastReading()`.

---

# BME280Device

Reads temperature, humidity, and pressure. Supports averaging, outlier rejection, and timestamping.

- **Key Methods:**
  - `readData()`, `getLastReading()`.
  - `Reading` struct: `temperature`, `humidity`, `pressure`, `heatIndex`, `dewPoint`, `avg*`, `timestamp`.

---

# IrrigationManager

Orchestrates all readings and watering logic. Applies temperature correction to soil moisture, prints all results, and controls watering relay.

- **Watering:**
  - Configurable threshold and duration.
  - Supports manual watering via `waterNow()`.
  - Clamps percent values to [0, 100].
- **Key Methods:**
  - `trigger()`, `update()`, `waterNow()`, `reset()`.
  - State machine for reading and watering sequence.

---

# SystemManager

Initializes and connects all managers and devices. Entry point for system startup.

---

# File Structure
- `src/` - Main source code
- `include/` - Header files
- `docs/` - Documentation
- `platformio.ini` - PlatformIO project config

---

# Adding New Features
- Add config defaults in `ConfigManager`.
- Add new device/manager class in `src/devices` or `src/system`.
- Integrate with `SystemManager` and update documentation here.

---

# Troubleshooting & Tips
- Use serial monitor for logs and diagnostics.
- All config and error messages are printed at startup.
- For sensor or relay issues, check wiring and config values.
- All JSON operations use cJSON (v7 compliant).

---

For further details, see code comments and this documentation directory.
