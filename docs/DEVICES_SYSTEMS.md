# Device and System Class Documentation

---

## SoilMoistureSensor

### Overview
Measures soil moisture using an ADS1115 ADC. Supports calibration, percent calculation, timestamping, and non-blocking stabilization. Designed for robust, repeatable readings with outlier rejection and averaging.

### Configuration Keys
- `soil_moisture.ads_channel`: ADS1115 channel (default: 0)
- `soil_moisture.gain`: Gain setting (default: 0 = ±6.144V)
- `soil_moisture.stabilisation_time`: Time in seconds to wait before reading (default: 10)
- `soil_moisture.wet`: ADC value for 100% moisture (default: 2100)
- `soil_moisture.dry`: ADC value for 0% moisture (default: 8700)

### Public Methods
- `void begin(ADS1115Manager*, ConfigManager*)`
- `void beginStabilisation()`
- `bool readyForReading()`
- `void takeReading()`
- `const Reading& getLastReading()`
- `void printReading()`

### Reading Struct
- `int16_t raw`: Raw ADC value
- `float voltage`: Measured voltage
- `float percent`: Calculated percent (clamped [0,100])
- `float avgRaw`, `avgVoltage`, `avgPercent`: Averaged values
- `time_t timestamp`: RTC local time
- `bool valid`: Reading validity

### Data Flow
1. `beginStabilisation()` starts a timer.
2. `readyForReading()` returns true after stabilization time.
3. `takeReading()` samples 10 times, rejects outliers, averages, and stores results.
4. `getLastReading()` provides the latest data.

---

## MQ135Sensor

### Overview
Measures air quality using an MQ135 sensor and ADS1115. Powered via relay, supports warmup, averaging, and qualitative AQI label.

### Configuration Keys
- `mq135.ads_channel`: ADS1115 channel (default: 1)
- `mq135.gain`: Gain setting (default: 0 = ±6.144V)
- `mq135.warmup_time`: Warmup time in seconds (default: 60)

### Public Methods
- `void begin(ADS1115Manager*, ConfigManager*, RelayController*)`
- `void startReading()`
- `bool readyForReading()`
- `void takeReading()`
- `const Reading& getLastReading()`
- `static const char* getAirQualityLabel(float voltage)`

### Reading Struct
- `int16_t raw`: Raw ADC value
- `float voltage`: Measured voltage
- `float avgRaw`, `avgVoltage`: Averaged values
- `time_t timestamp`: RTC local time
- `bool valid`: Reading validity

### Data Flow
1. `startReading()` powers sensor and starts warmup.
2. `readyForReading()` returns true after warmup.
3. `takeReading()` samples 10 times, rejects outliers, averages, and stores results.
4. `getLastReading()` provides the latest data.
5. `getAirQualityLabel()` maps voltage to qualitative label.

---

## BME280Device

### Overview
Measures temperature, humidity, and pressure. Supports averaging, outlier rejection, and timestamping.

### Configuration Keys
- None required; auto-detects on I2C bus.

### Public Methods
- `void begin(I2CManager*, ConfigManager*)`
- `BME280Reading readData()`
- `const BME280Reading& getLastReading()`

### BME280Reading Struct
- `float temperature, humidity, pressure, heatIndex, dewPoint`
- `float avgTemperature, avgHumidity, avgPressure, avgHeatIndex, avgDewPoint`
- `DateTime timestamp`
- `bool valid`

### Data Flow
1. `readData()` samples 10 times, rejects outliers, averages, and stores results.
2. `getLastReading()` provides the latest data.

---

## ADS1115Manager

### Overview
Manages all ADS1115 ADC operations. Supports single-ended reads, gain selection, and error handling.

### Public Methods
- `int16_t readRaw(uint8_t channel, Gain gain, int timeoutMs)`
- `float readVoltage(uint8_t channel, Gain gain, int timeoutMs)`
- `void begin(I2CManager*, ConfigManager*)`

### Data Flow
- Used by all analog sensors (soil, air quality).
- Handles I2C errors and retries.

---

## I2CManager

### Overview
Provides a Wire-compatible API for I2C operations. Used by all I2C devices.

### Public Methods
- `void begin()`
- `void scan()`
- `bool devicePresent(uint8_t address)`

---

## RelayController

### Overview
Manages up to 4 relays, supports active high/low logic, integrates with config for GPIO assignment.

### Configuration Keys
- `relay_count`: Number of relays
- `relay_gpio_0` ... `relay_gpio_3`: GPIOs for each relay
- `relay_active_high_0` ... `relay_active_high_3`: Logic for each relay

### Public Methods
- `void begin(ConfigManager*, DiagnosticManager*)`
- `void setRelayMode(int index, Relay::Mode mode)`
- `void activateRelay(int index)`
- `void deactivateRelay(int index)`
- `int getRelayGpio(int index)`

---

## IrrigationManager

### Overview
Orchestrates all readings and watering logic. Applies temperature correction to soil moisture, prints all results, and controls watering relay.

### Configuration Keys
- `watering_threshold`: Soil moisture percent threshold
- `watering_duration_sec`: Watering duration in seconds

### Public Methods
- `void begin(BME280Device*, SoilMoistureSensor*, MQ135Sensor*, TimeManager*)`
- `void trigger()`
- `void update()`
- `void waterNow()`
- `void reset()`
- `bool isRunning()`, `bool isComplete()`

### Data Flow
1. `trigger()` starts the full reading/watering sequence.
2. `update()` advances the state machine.
3. `waterNow()` immediately waters for the configured duration.
4. All results are printed to serial.

---

## SystemManager

### Overview
Initializes and connects all managers and devices. Entry point for system startup.

### Public Methods
- `void begin()`
- `void update()`

---

For further details, see code comments and the rest of the documentation in this directory.
