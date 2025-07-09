# ESP32 IoT Irrigation Platform - Documentation

## Overview
This project is a comprehensive ESP32-based IoT irrigation and environment monitoring platform. It features centralized configuration, robust time and alarm management, sensor integration, and relay control for automated watering and air quality monitoring.

---

## Table of Contents
- [System Architecture](#system-architecture)
- [Configuration Management](#configuration-management)
- [Time and Alarm Logic](#time-and-alarm-logic)
- [Sensor Integration](#sensor-integration)
- [Relay and Irrigation Control](#relay-and-irrigation-control)
- [Air Quality Index (AQI) Mapping](#air-quality-index-aqi-mapping)
- [WiFi and Network](#wifi-and-network)
- [Extending and Customizing](#extending-and-customizing)
- [Troubleshooting](#troubleshooting)

---

## System Architecture
- **PlatformIO project** for ESP32.
- All configuration is centralized in `ConfigManager`.
- Modular managers for time, sensors, relays, and irrigation.

## Configuration Management
- All defaults are set in `ConfigManager::loadDefaults()`.
- Configurable via code or (future) file/web interface.
- Key config options:
  - WiFi credentials and mode
  - Timezone, DST, NTP
  - Alarm schedules (single and weekly)
  - Sensor calibration and timings
  - Watering threshold and duration

## Time and Alarm Logic
- RTC (DS3231) always stores local time.
- DST and NTP logic handled in `TimeManager`.
- Weekly alarm schedule supported.
- INT/SQW pin controls relay 0 for alarm events.

## Sensor Integration
- **SoilMoistureSensor**: Uses ADS1115, supports calibration, percent calculation, and timestamping.
- **MQ135Sensor**: Air quality sensor, powered via relay, warmup logic, and qualitative AQI label.
- **BME280Device**: Temperature, humidity, and pressure with averaging and outlier rejection.

## Relay and Irrigation Control
- **RelayController**: Manages all relays, supports active high/low logic.
- **IrrigationManager**: Orchestrates readings, applies corrections, and controls watering based on soil moisture.
- Supports manual watering via `waterNow()`.

## Air Quality Index (AQI) Mapping
- MQ135 voltage is mapped to qualitative labels:
  - `< 0.25V`: Excellent
  - `0.25–0.35V`: Good
  - `0.35–0.45V`: Moderate
  - `0.45–0.60V`: Poor
  - `> 0.60V`: Very Poor
- See `MQ135Sensor::getAirQualityLabel()` for details.

## WiFi and Network
- WiFi reconnect logic is robust.
- NTP syncs RTC to network time.
- All network settings are configurable.

## Extending and Customizing
- Add new sensors by creating a device class and integrating with `DeviceManager`.
- All config and logic is centralized for easy extension.

## Troubleshooting
- Use serial monitor for detailed logs.
- All config and error messages are printed at startup.
- For sensor or relay issues, check wiring and config values.

---

For detailed API and code documentation, see additional files in this `docs/` directory.
