// sensors.js - JS for sensors.html
// Only handles BME280, Soil Moisture, and MQ135 sensors

async function updateSensors() {
    try {
        const res = await fetch('/api/status');
        const data = await res.json();
        // BME280
        if (data.bme280 && data.bme280.last_reading) {
            const r = data.bme280.last_reading;
            let bmeHtml = '';
            bmeHtml += `<b>Temperature:</b> ${r.temperature?.toFixed(2)} °C<br>`;
            bmeHtml += `<b>Humidity:</b> ${r.humidity?.toFixed(2)} %<br>`;
            bmeHtml += `<b>Pressure:</b> ${r.pressure?.toFixed(2)} hPa<br>`;
            bmeHtml += `<b>Heat Index:</b> ${r.heat_index?.toFixed(2)} °C<br>`;
            bmeHtml += `<b>Dew Point:</b> ${r.dew_point?.toFixed(2)} °C<br>`;
            bmeHtml += `<b>Timestamp:</b> ${r.timestamp || '--'}<br>`;
            document.getElementById('bme280-data').innerHTML = bmeHtml;
        } else {
            document.getElementById('bme280-data').textContent = '--';
        }
        // Soil Moisture
        if (data.soil_moisture) {
            const s = data.soil_moisture;
            let soilHtml = '';
            soilHtml += `<b>Percent:</b> ${s.percent !== undefined ? s.percent.toFixed(2) + ' %' : '--'}<br>`;
            soilHtml += `<b>Raw Value:</b> ${s.raw !== undefined ? s.raw : '--'}<br>`;
            soilHtml += `<b>Timestamp:</b> ${s.timestamp || '--'}<br>`;
            document.getElementById('soilmoisture-data').innerHTML = soilHtml;
        } else {
            document.getElementById('soilmoisture-data').textContent = '--';
        }
        // Air Quality
        if (data.mq135) {
            let mqHtml = '';
            mqHtml += `<b>AQI:</b> ${data.mq135.aqi_label || '--'}<br>`;
            mqHtml += `<b>Timestamp:</b> ${data.mq135.timestamp || '--'}<br>`;
            document.getElementById('mq135-data').innerHTML = mqHtml;
        } else {
            document.getElementById('mq135-data').textContent = '--';
        }
    } catch (e) {
        // Optionally show error
    }
}

window.addEventListener('DOMContentLoaded', () => {
    updateSensors();
    setInterval(updateSensors, 5000);
});
