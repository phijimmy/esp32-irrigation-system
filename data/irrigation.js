// irrigation.js - JS for irrigation.html
// Add your irrigation page logic here

// Example: fetch and display irrigation status
function updateIrrigationState() {
    fetch('/api/status')
        .then(response => response.json())
        .then(data => {
            let state = 'Unknown';
            let details = '';
            if (data.irrigation) {
                if (typeof data.irrigation.state === 'string') {
                    state = data.irrigation.state.charAt(0).toUpperCase() + data.irrigation.state.slice(1);
                }
                details += 'Running: ' + (data.irrigation.running ? 'Yes' : 'No') + '<br>';
                details += 'Watering Active: ' + (data.irrigation.watering_active ? 'Yes' : 'No') + '<br>';
                details += 'Scheduled Time: ' + (data.irrigation.scheduled_hour !== undefined && data.irrigation.scheduled_minute !== undefined ? (data.irrigation.scheduled_hour + ':' + (data.irrigation.scheduled_minute < 10 ? '0' : '') + data.irrigation.scheduled_minute) : 'N/A') + '<br>';
                details += 'Last Run: ' + (data.irrigation.last_run_time || 'N/A') + '<br>';
                details += 'Watering Threshold: ' + (data.irrigation.watering_threshold !== undefined ? data.irrigation.watering_threshold + '%' : 'N/A') + '<br>';
            } else {
                state = 'No irrigation data';
            }
            document.getElementById('irrigation-state').textContent = state;
            document.getElementById('irrigation-details').innerHTML = details;

            // BME280
            let bmeHtml = '';
            if (data.bme280 && data.bme280.last_reading) {
                const b = data.bme280;
                const r = b.last_reading;
                bmeHtml += `<b>Temperature:</b> ${r.temperature?.toFixed(2)} °C` + '<br>';
                bmeHtml += `<b>Humidity:</b> ${r.humidity?.toFixed(2)} %` + '<br>';
                bmeHtml += `<b>Pressure:</b> ${r.pressure?.toFixed(2)} hPa` + '<br>';
                bmeHtml += `<b>Heat Index:</b> ${r.heat_index?.toFixed(2)} °C` + '<br>';
                bmeHtml += `<b>Dew Point:</b> ${r.dew_point?.toFixed(2)} °C` + '<br>';
                bmeHtml += `<b>Valid:</b> ${r.valid ? 'Yes' : 'No'}` + '<br>';
                bmeHtml += `<b>Avg Temperature:</b> ${r.avg_temperature?.toFixed(2)} °C` + '<br>';
                bmeHtml += `<b>Avg Humidity:</b> ${r.avg_humidity?.toFixed(2)} %` + '<br>';
                bmeHtml += `<b>Avg Pressure:</b> ${r.avg_pressure?.toFixed(2)} hPa` + '<br>';
                bmeHtml += `<b>Avg Heat Index:</b> ${r.avg_heat_index?.toFixed(2)} °C` + '<br>';
                bmeHtml += `<b>Avg Dew Point:</b> ${r.avg_dew_point?.toFixed(2)} °C` + '<br>';
                bmeHtml += `<b>Timestamp:</b> ${r.timestamp || 'N/A'}` + '<br>';
            } else {
                bmeHtml = 'No BME280 data';
            }
            document.getElementById('bme280-data').innerHTML = bmeHtml;

            // Soil Moisture
            let soilHtml = '';
            if (data.soil_moisture) {
                const s = data.soil_moisture;
                soilHtml += `<b>State:</b> ${s.state || 'N/A'}` + '<br>';
                soilHtml += `<b>Raw:</b> ${s.raw !== undefined ? s.raw : 'N/A'}` + '<br>';
                soilHtml += `<b>Voltage:</b> ${s.voltage !== undefined ? s.voltage.toFixed(4) + ' V' : 'N/A'}` + '<br>';
                soilHtml += `<b>Percent:</b> ${s.percent !== undefined ? s.percent.toFixed(2) + ' %' : 'N/A'}` + '<br>';
                soilHtml += `<b>Avg Raw:</b> ${s.avg_raw !== undefined ? s.avg_raw : 'N/A'}` + '<br>';
                soilHtml += `<b>Avg Voltage:</b> ${s.avg_voltage !== undefined ? s.avg_voltage.toFixed(4) + ' V' : 'N/A'}` + '<br>';
                soilHtml += `<b>Avg Percent:</b> ${s.avg_percent !== undefined ? s.avg_percent.toFixed(2) + ' %' : 'N/A'}` + '<br>';
                soilHtml += `<b>Timestamp:</b> ${s.timestamp || 'N/A'}` + '<br>';
            } else {
                soilHtml = 'No soil moisture data';
            }
            // Add corrected soil moisture from irrigation manager if available
            if (data.irrigation && typeof data.irrigation.soil_corrected === 'number') {
                soilHtml += `<b>Corrected Soil Moisture (Irrigation):</b> ${data.irrigation.soil_corrected.toFixed(2)} %<br>`;
            }
            document.getElementById('soilmoisture-data').innerHTML = soilHtml;
        })
        .catch(() => {
            document.getElementById('irrigation-state').textContent = 'Error fetching state';
            document.getElementById('irrigation-details').textContent = '';
            document.getElementById('bme280-data').textContent = '';
            document.getElementById('soilmoisture-data').textContent = '';
        });
}
document.addEventListener('DOMContentLoaded', function() {
    updateIrrigationState();
    setInterval(updateIrrigationState, 2000);

    const btn = document.getElementById('start-irrigation-btn');
    if (btn) {
        btn.addEventListener('click', function() {
            btn.disabled = true;
            btn.textContent = 'Start';
            fetch('/api/irrigation/trigger', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: '{}'
            })
            .then(res => res.json())
            .then(data => {
                btn.textContent = data.result === 'ok' ? 'Start' : 'Error';
                setTimeout(() => {
                    btn.textContent = 'Start';
                    btn.disabled = false;
                }, 2000);
            })
            .catch(() => {
                btn.textContent = 'Error';
                setTimeout(() => {
                    btn.textContent = 'Start';
                    btn.disabled = false;
                }, 2000);
            });
        });
    }

    const waterNowBtn = document.getElementById('water-now-btn');
    if (waterNowBtn) {
        waterNowBtn.addEventListener('click', function() {
            waterNowBtn.disabled = true;
            waterNowBtn.textContent = 'Water';
            fetch('/api/irrigation/waternow', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: '{}'
            })
            .then(res => res.json())
            .then(data => {
                waterNowBtn.textContent = data.result === 'ok' ? 'Water' : 'Error';
                setTimeout(() => {
                    waterNowBtn.textContent = 'Water';
                    waterNowBtn.disabled = false;
                }, 2000);
            })
            .catch(() => {
                waterNowBtn.textContent = 'Error';
                setTimeout(() => {
                    waterNowBtn.textContent = 'Water';
                    waterNowBtn.disabled = false;
                }, 2000);
            });
        });
    }

    // Stop Irrigation button logic
    const stopBtn = document.getElementById('stop-irrigation-btn');
    if (stopBtn) {
        stopBtn.addEventListener('click', function() {
            stopBtn.disabled = true;
            stopBtn.textContent = 'Stop';
            fetch('/api/irrigation/stop', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: '{}'
            })
            .then(res => res.json())
            .then(data => {
                stopBtn.textContent = data.result === 'ok' ? 'Stop' : 'Error';
                setTimeout(() => {
                    stopBtn.textContent = 'Stop';
                    stopBtn.disabled = false;
                }, 2000);
            })
            .catch(() => {
                stopBtn.textContent = 'Error';
                setTimeout(() => {
                    stopBtn.textContent = 'Stop';
                    stopBtn.disabled = false;
                }, 2000);
            });
        });
    }
});
