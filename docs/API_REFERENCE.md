# API Reference: ESP32 IoT Irrigation Platform

---

## ConfigManager

### Methods
- **void loadDefaults()**: Sets all default config values in code.
- **bool load()**: Loads config from file (future support).
- **bool save()**: Saves config to file (future support).
- **void resetToDefaults()**: Resets config to defaults and saves.
- **int getInt(const char* key, int defaultValue)**: Get integer config value.
- **bool getBool(const char* key, bool defaultValue)**: Get boolean config value.
- **const char* get(const char* key)**: Get string config value.
- **void set(const char* key, const char* value)**: Set string config value.
- **void setBool(const char* key, bool value)**: Set boolean config value.
- **cJSON* getSection(const char* section)**: Get a config section (object).

### Configuration Keys
- **wifi_mode**: "ap" or "client"
- **wifi_ssid**, **wifi_pass**: WiFi credentials
- **timezone_offset**: Integer, e.g. 1 for CET
- **dst_enabled**: Boolean, enable DST
- **ntp_server_1**, **ntp_server_2**: NTP servers
- **watering_threshold**: Soil moisture percent threshold
- **watering_duration_sec**: Watering duration in seconds
- **soil_moisture**: Object with calibration and timing
- **mq135**: Object with channel, gain, warmup
- ...and many more (see code for full list)

---

## TimeManager

### Methods
- **void begin(...)**: Initialize with I2C, config, diagnostics
- **void update()**: Main loop handler (call frequently)
- **void setAlarm1(int hour, int min, int sec, bool enabled)**
- **void setAlarm2(int hour, int min, bool enabled)**
- **void updateAlarmsFromConfig()**: Apply config alarms
- **void setAlarmsForToday()**: Apply weekly schedule
- **DateTime getLocalTime()**: Get current local time
- **bool isDSTActive(const DateTime& localTime)**: DST status
- **void forceNTPSync()**: Force NTP sync

### Alarm Logic
- Weekly and single-alarm support
- INT/SQW pin controls relay 0
- DST and NTP logic fully integrated

---

## RelayController

### Methods
- **void begin(ConfigManager*, DiagnosticManager*)**
- **void setRelayMode(int index, Relay::Mode mode)**: ON/OFF/AUTO
- **void activateRelay(int index)**
- **void deactivateRelay(int index)**
- **int getRelayGpio(int index)**: Get GPIO for relay

---

## SoilMoistureSensor

### Methods
- **void begin(ADS1115Manager*, ConfigManager*)**
- **void beginStabilisation()**: Start non-blocking stabilization
- **bool readyForReading()**: True if ready
- **void takeReading()**: Take and store reading
- **const Reading& getLastReading()**: Get last reading
- **void printReading()**: Print to serial

### Reading Struct
- **raw**: int, ADC value
- **voltage**: float, measured voltage
- **percent**: float, calculated percent
- **avgRaw/avgVoltage/avgPercent**: Averaged values
- **timestamp**: time_t, RTC local time

---

## MQ135Sensor

### Methods
- **void begin(ADS1115Manager*, ConfigManager*, RelayController*)**
- **void startReading()**: Power on, start warmup
- **bool readyForReading()**: True if warmup done
- **void takeReading()**: Take and store reading
- **const Reading& getLastReading()**: Get last reading
- **static const char* getAirQualityLabel(float voltage)**: Get AQI label

### Reading Struct
- **raw**: int, ADC value
- **voltage**: float, measured voltage
- **avgRaw/avgVoltage**: Averaged values
- **timestamp**: time_t, RTC local time

---

## BME280Device

### Methods
- **void begin(I2CManager*, ConfigManager*)**
- **BME280Reading readData()**: Take and return reading
- **const BME280Reading& getLastReading()**: Get last reading

### BME280Reading Struct
- **temperature, humidity, pressure, heatIndex, dewPoint**: float
- **avgTemperature, avgHumidity, avgPressure, avgHeatIndex, avgDewPoint**: float
- **timestamp**: DateTime
- **valid**: bool

---

## IrrigationManager

### Methods
- **void begin(BME280Device*, SoilMoistureSensor*, MQ135Sensor*, TimeManager*)**
- **void trigger()**: Start full reading/watering sequence
- **void update()**: Main loop handler
- **void waterNow()**: Manual watering for configured duration
- **void reset()**: Reset state
- **bool isRunning()**, **bool isComplete()**

### Logic
- State machine: IDLE, START, BME, SOIL, MQ135, COMPLETE, WATER_NOW
- Applies temperature correction to soil moisture
- Clamps percent values to [0, 100]
- Prints all results and progress

---

## SystemManager

### Methods
- **void begin()**: Initializes all managers and devices
- **void update()**: Main loop handler

---

For more details, see code comments and the rest of the documentation in this directory.
