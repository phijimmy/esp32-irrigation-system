// irrigation.js - JS for irrigation.html
// Add your irrigation page logic here

// Example: fetch and display irrigation status
function updateIrrigationState() {
    fetch('/api/status')
        .then(response => response.json())
        .then(data => {
            let state = 'Unbekannt';
            let details = '';
            if (data.irrigation) {
                if (typeof data.irrigation.state === 'string') {
                    state = data.irrigation.state.charAt(0).toUpperCase() + data.irrigation.state.slice(1);
                }
                details += 'Läuft: ' + (data.irrigation.running ? 'Ja' : 'Nein') + '<br>';
                details += 'Bewässerung aktiv: ' + (data.irrigation.watering_active ? 'Ja' : 'Nein') + '<br>';
                details += 'Geplante Zeit: ' + (data.irrigation.scheduled_hour !== undefined && data.irrigation.scheduled_minute !== undefined ? (data.irrigation.scheduled_hour + ':' + (data.irrigation.scheduled_minute < 10 ? '0' : '') + data.irrigation.scheduled_minute) : 'N/V') + '<br>';
                details += 'Letzter Lauf: ' + (data.irrigation.last_run_time || 'N/V') + '<br>';
                details += 'Bewässerungsschwelle: ' + (data.irrigation.watering_threshold !== undefined ? data.irrigation.watering_threshold + '%' : 'N/V') + '<br>';
            } else {
                state = 'Keine Bewässerungsdaten';
            }
            // Set status color class based on state
            const stateElem = document.getElementById('irrigation-state');
            stateElem.textContent = state;
            // Remove previous status classes
            stateElem.classList.remove('status-idle', 'status-watering', 'status-reading', 'status-error');
            // Map state to class
            let stateClass = '';
            switch (state.toLowerCase()) {
                case 'idle':
                    stateClass = 'status-idle';
                    state = 'Bereit';
                    break;
                case 'watering':
                    stateClass = 'status-watering';
                    state = 'Bewässerung läuft';
                    break;
                case 'reading':
                    stateClass = 'status-reading';
                    state = 'Messung läuft...';
                    break;
                case 'error':
                case 'no irrigation data':
                    stateClass = 'status-error';
                    state = 'Fehler';
                    break;
                default:
                    stateClass = 'status-idle';
            }
            stateElem.classList.add(stateClass);
            document.getElementById('irrigation-details').innerHTML = details;

            // BME280
            let bmeHtml = '';
            if (data.bme280 && data.bme280.last_reading) {
                const b = data.bme280;
                const r = b.last_reading;
                bmeHtml += `<b>Temperatur:</b> ${r.temperature?.toFixed(2)} °C` + '<br>';
                bmeHtml += `<b>Feuchtigkeit:</b> ${r.humidity?.toFixed(2)} %` + '<br>';
                bmeHtml += `<b>Luftdruck:</b> ${r.pressure?.toFixed(2)} hPa` + '<br>';
                bmeHtml += `<b>Hitzeindex:</b> ${r.heat_index?.toFixed(2)} °C` + '<br>';
                bmeHtml += `<b>Taupunkt:</b> ${r.dew_point?.toFixed(2)} °C` + '<br>';
                bmeHtml += `<b>Gültig:</b> ${r.valid ? 'Ja' : 'Nein'}` + '<br>';
                bmeHtml += `<b>Ø Temperatur:</b> ${r.avg_temperature?.toFixed(2)} °C` + '<br>';
                bmeHtml += `<b>Ø Feuchtigkeit:</b> ${r.avg_humidity?.toFixed(2)} %` + '<br>';
                bmeHtml += `<b>Ø Luftdruck:</b> ${r.avg_pressure?.toFixed(2)} hPa` + '<br>';
                bmeHtml += `<b>Ø Hitzeindex:</b> ${r.avg_heat_index?.toFixed(2)} °C` + '<br>';
                bmeHtml += `<b>Ø Taupunkt:</b> ${r.avg_dew_point?.toFixed(2)} °C` + '<br>';
                bmeHtml += `<b>Zeitstempel:</b> ${r.timestamp || 'N/V'}` + '<br>';
            } else {
                bmeHtml = 'Keine BME280-Daten';
            }
            document.getElementById('bme280-data').innerHTML = bmeHtml;

            // Soil Moisture
            let soilHtml = '';
            let soilStateClass = 'soil-status-unknown';
            let soilStateText = 'N/V';
            if (data.soil_moisture) {
                const s = data.soil_moisture;
                soilStateText = s.state || 'N/V';
                if (soilStateText.toLowerCase() === 'ok' || soilStateText.toLowerCase() === 'normal') {
                    soilStateClass = 'soil-status-ok';
                    soilStateText = 'Normal';
                } else if (soilStateText.toLowerCase() === 'dry') {
                    soilStateClass = 'soil-status-dry';
                    soilStateText = 'Trocken';
                } else if (soilStateText.toLowerCase() === 'wet') {
                    soilStateClass = 'soil-status-wet';
                    soilStateText = 'Nass';
                } else {
                    soilStateClass = 'soil-status-unknown';
                }
                soilHtml += `<b>Status:</b> <span id="soil-state-span" class="${soilStateClass}">${soilStateText}</span><br>`;
                soilHtml += `<b>Rohwert:</b> ${s.raw !== undefined ? s.raw : 'N/V'}` + '<br>';
                soilHtml += `<b>Spannung:</b> ${s.voltage !== undefined ? s.voltage.toFixed(4) + ' V' : 'N/V'}` + '<br>';
                soilHtml += `<b>Prozent:</b> ${s.percent !== undefined ? s.percent.toFixed(2) + ' %' : 'N/V'}` + '<br>';
                soilHtml += `<b>Ø Rohwert:</b> ${s.avg_raw !== undefined ? s.avg_raw : 'N/V'}` + '<br>';
                soilHtml += `<b>Ø Spannung:</b> ${s.avg_voltage !== undefined ? s.avg_voltage.toFixed(4) + ' V' : 'N/V'}` + '<br>';
                soilHtml += `<b>Ø Prozent:</b> ${s.avg_percent !== undefined ? s.avg_percent.toFixed(2) + ' %' : 'N/V'}` + '<br>';
                soilHtml += `<b>Zeitstempel:</b> ${s.timestamp || 'N/V'}` + '<br>';
            } else {
                soilHtml = 'Keine Bodenfeuchte-Daten';
            }
            // Add corrected soil moisture from irrigation manager if available
            if (data.irrigation && typeof data.irrigation.soil_corrected === 'number') {
                soilHtml += `<b>Korrigierte Bodenfeuchte (Bewässerung):</b> ${data.irrigation.soil_corrected.toFixed(2)} %<br>`;
            }
            document.getElementById('soilmoisture-data').innerHTML = soilHtml;
        })
        .catch(() => {
            document.getElementById('irrigation-state').textContent = 'Fehler beim Abrufen des Status';
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
            btn.textContent = 'Starten';
            fetch('/api/irrigation/trigger', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: '{}'
            })
            .then(res => res.json())
            .then(data => {
                btn.textContent = data.result === 'ok' ? 'Starten' : 'Fehler';
                setTimeout(() => {
                    btn.textContent = 'Starten';
                    btn.disabled = false;
                }, 2000);
            })
            .catch(() => {
                btn.textContent = 'Fehler';
                setTimeout(() => {
                    btn.textContent = 'Starten';
                    btn.disabled = false;
                }, 2000);
            });
        });
    }

    const waterNowBtn = document.getElementById('water-now-btn');
    if (waterNowBtn) {
        waterNowBtn.addEventListener('click', function() {
            waterNowBtn.disabled = true;
            waterNowBtn.textContent = 'Jetzt bewässern';
            fetch('/api/irrigation/waternow', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: '{}'
            })
            .then(res => res.json())
            .then(data => {
                waterNowBtn.textContent = data.result === 'ok' ? 'Jetzt bewässern' : 'Fehler';
                setTimeout(() => {
                    waterNowBtn.textContent = 'Jetzt bewässern';
                    waterNowBtn.disabled = false;
                }, 2000);
            })
            .catch(() => {
                waterNowBtn.textContent = 'Fehler';
                setTimeout(() => {
                    waterNowBtn.textContent = 'Jetzt bewässern';
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
            stopBtn.textContent = 'Stopp';
            fetch('/api/irrigation/stop', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: '{}'
            })
            .then(res => res.json())
            .then(data => {
                stopBtn.textContent = data.result === 'ok' ? 'Stopp' : 'Fehler';
                setTimeout(() => {
                    stopBtn.textContent = 'Stopp';
                    stopBtn.disabled = false;
                }, 2000);
            })
            .catch(() => {
                stopBtn.textContent = 'Fehler';
                setTimeout(() => {
                    stopBtn.textContent = 'Stopp';
                    stopBtn.disabled = false;
                }, 2000);
            });
        });
    }
});
