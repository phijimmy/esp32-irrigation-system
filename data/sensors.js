// sensors.js - JS for sensors.html
// Only handles BME280, Soil Moisture, and MQ135 sensors

async function updateSensors() {
    try {
        const res = await fetch('/api/status');
        const data = await res.json();
        // BME280
        if (data.bme280 && data.bme280.last_reading) {
            document.getElementById('bme280-temp').textContent = data.bme280.last_reading.temperature.toFixed(2) + ' \u00b0C';
            document.getElementById('bme280-hum').textContent = data.bme280.last_reading.humidity.toFixed(2) + ' %';
            document.getElementById('bme280-press').textContent = data.bme280.last_reading.pressure.toFixed(2) + ' hPa';
            if (typeof data.bme280.last_reading.heat_index === 'number') {
                document.getElementById('bme280-heat-index').textContent = data.bme280.last_reading.heat_index.toFixed(2) + ' \u00b0C';
            } else {
                document.getElementById('bme280-heat-index').textContent = '-- \u00b0C';
            }
            if (typeof data.bme280.last_reading.dew_point === 'number') {
                document.getElementById('bme280-dew-point').textContent = data.bme280.last_reading.dew_point.toFixed(2) + ' \u00b0C';
            } else {
                document.getElementById('bme280-dew-point').textContent = '-- \u00b0C';
            }
            document.getElementById('bme280-meta').textContent = 'Last update: ' + (data.bme280.last_reading.timestamp || '--');
        }
        // Soil Moisture
        if (data.soil_moisture) {
            document.getElementById('soilmoisture-value').textContent = (data.soil_moisture.percent !== undefined ? data.soil_moisture.percent.toFixed(2) + ' %' : '--');
            document.getElementById('soilmoisture-raw').textContent = (data.soil_moisture.raw !== undefined ? data.soil_moisture.raw : '--');
            document.getElementById('soilmoisture-meta').textContent = 'Last update: ' + (data.soil_moisture.timestamp || '--');
        }
        // Air Quality
        if (data.mq135) {
            document.getElementById('mq135-value').textContent = (data.mq135.aqi_label || '--');
            document.getElementById('mq135-meta').textContent = 'Last update: ' + (data.mq135.timestamp || '--');
        }
    } catch (e) {
        // Optionally show error
    }
}

window.addEventListener('DOMContentLoaded', () => {
    updateSensors();
    setInterval(updateSensors, 5000);
});
