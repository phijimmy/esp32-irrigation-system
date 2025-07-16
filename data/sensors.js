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
        const soilBtn = document.getElementById('soilmoisture-read-btn');
        if (data.soil_moisture) {
            const s = data.soil_moisture;
            let soilHtml = '';
            let soilStateClass = 'soil-status-unknown';
            let soilStateText = s.state || '--';
            if (soilStateText.toLowerCase() === 'ok' || soilStateText.toLowerCase() === 'normal') {
                soilStateClass = 'soil-status-ok';
            } else if (soilStateText.toLowerCase() === 'dry') {
                soilStateClass = 'soil-status-dry';
            } else if (soilStateText.toLowerCase() === 'wet') {
                soilStateClass = 'soil-status-wet';
            } else {
                soilStateClass = 'soil-status-unknown';
            }
            soilHtml += `<b>Percent:</b> ${s.percent !== undefined ? s.percent.toFixed(2) + ' %' : '--'}<br>`;
            soilHtml += `<b>Raw Value:</b> ${s.raw !== undefined ? s.raw : '--'}<br>`;
            soilHtml += `<b>Timestamp:</b> ${s.timestamp || '--'}<br>`;
            soilHtml += `<b>State:</b> <span class="${soilStateClass}">${soilStateText}</span><br>`;
            document.getElementById('soilmoisture-data').innerHTML = soilHtml;
            if (soilBtn) {
                if (s.state === 'stabilising' || s.state === 'reading') {
                    soilBtn.disabled = true;
                    soilBtn.textContent = 'Reading...';
                } else {
                    soilBtn.disabled = false;
                    soilBtn.textContent = 'Take Soil Moisture Reading';
                }
            }
        } else {
            document.getElementById('soilmoisture-data').textContent = '--';
            if (soilBtn) {
                soilBtn.disabled = false;
                soilBtn.textContent = 'Take Soil Moisture Reading';
            }
        }
        // Air Quality
        if (data.mq135) {
            let mqHtml = '';
            let airStateClass = 'air-status-unknown';
            let airStateText = data.mq135.state || '--';
            // Map state to class (customize as needed)
            if (airStateText.toLowerCase() === 'good' || airStateText.toLowerCase() === 'ok' || airStateText.toLowerCase() === 'normal') {
                airStateClass = 'air-status-good';
            } else if (airStateText.toLowerCase() === 'moderate') {
                airStateClass = 'air-status-moderate';
            } else if (airStateText.toLowerCase() === 'poor' || airStateText.toLowerCase() === 'bad' || airStateText.toLowerCase() === 'danger') {
                airStateClass = 'air-status-poor';
            } else {
                airStateClass = 'air-status-unknown';
            }
            mqHtml += `<b>AQI:</b> ${data.mq135.aqi_label || '--'}<br>`;
            mqHtml += `<b>Timestamp:</b> ${data.mq135.timestamp || '--'}<br>`;
            mqHtml += `<b>State:</b> <span class="${airStateClass}">${airStateText}</span><br>`;
            if (data.mq135.state === 'warming_up') {
                mqHtml += `<b>Warmup:</b> ${data.mq135.warmup_elapsed_sec || 0} / ${data.mq135.warmup_time_sec || 0} sec<br>`;
            }
            document.getElementById('mq135-data').innerHTML = mqHtml;
            if (window.mqBtn) {
                if (data.mq135.state === 'warming_up' || data.mq135.state === 'reading') {
                    window.mqBtn.disabled = true;
                    window.mqBtn.textContent = (data.mq135.state === 'warming_up') ? 'Warming up...' : 'Reading...';
                } else {
                    window.mqBtn.disabled = false;
                    window.mqBtn.textContent = 'Take Air Quality Reading';
                }
            }
        } else {
            document.getElementById('mq135-data').textContent = '--';
            if (window.mqBtn) {
                window.mqBtn.disabled = false;
                window.mqBtn.textContent = 'Take Air Quality Reading';
            }
        }
    } catch (e) {
        // Optionally show error
    }
}

window.addEventListener('DOMContentLoaded', () => {
    updateSensors();
    setInterval(updateSensors, 5000);

    const bmeBtn = document.getElementById('bme280-read-btn');
    if (bmeBtn) {
        bmeBtn.addEventListener('click', async function() {
            bmeBtn.disabled = true;
            bmeBtn.textContent = 'Reading...';
            try {
                const res = await fetch('/api/bme280/trigger', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: '{}' });
                const data = await res.json();
                if (data.result === 'ok') {
                    let bmeHtml = '';
                    bmeHtml += `<b>Temperature:</b> ${data.temperature?.toFixed(2)} °C<br>`;
                    bmeHtml += `<b>Humidity:</b> ${data.humidity?.toFixed(2)} %<br>`;
                    bmeHtml += `<b>Pressure:</b> ${data.pressure?.toFixed(2)} hPa<br>`;
                    bmeHtml += `<b>Heat Index:</b> ${data.heat_index?.toFixed(2)} °C<br>`;
                    bmeHtml += `<b>Dew Point:</b> ${data.dew_point?.toFixed(2)} °C<br>`;
                    document.getElementById('bme280-data').innerHTML = bmeHtml;
                    bmeBtn.textContent = 'Take BME280 Reading';
                } else {
                    bmeBtn.textContent = 'Error';
                }
            } catch (e) {
                bmeBtn.textContent = 'Error';
            }
            setTimeout(() => {
                bmeBtn.textContent = 'Take BME280 Reading';
                bmeBtn.disabled = false;
            }, 2000);
        });
    }

    const soilBtn = document.getElementById('soilmoisture-read-btn');
    if (soilBtn) {
        soilBtn.addEventListener('click', async function() {
            soilBtn.disabled = true;
            soilBtn.textContent = 'Reading...';
            try {
                await fetch('/api/soilmoisture/trigger', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: '{}' });
                // No need to handle response, updateSensors will poll and update state
            } catch (e) {
                soilBtn.textContent = 'Error';
                soilBtn.disabled = false;
            }
        });
    }

    // MQ135 Air Quality button handler - only attach once
    window.mqBtn = document.getElementById('mq135-read-btn');
    if (window.mqBtn) {
        window.mqBtn.addEventListener('click', async function() {
            window.mqBtn.disabled = true;
            window.mqBtn.textContent = 'Warming up...';
            try {
                await fetch('/api/mq135/trigger', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: '{}' });
                // No need to handle response, updateSensors will poll and update state
            } catch (e) {
                window.mqBtn.textContent = 'Error';
                window.mqBtn.disabled = false;
            }
        });
    }
});
