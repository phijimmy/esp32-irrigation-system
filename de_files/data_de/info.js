// Automatically load dashboard data on page load
window.addEventListener('DOMContentLoaded', async () => {
    try {
        const response = await fetch('/api/status');
        const data = await response.json();
        updateDeviceAndSystemInfo(data);
        updateSystemTime(data);
        updateNetworkInfo(data);
        updateFilesystemInfo(data);
        updateHealthInfo(data);
        updateI2CInfo(data);
        updateLedIndicator(data.led);
        updateTouchState(data);
    } catch (error) {
        console.error('Error fetching system info:', error);
        if (typeof showError === 'function') showError('Systeminformationen konnten nicht geladen werden');
    }
    // LED card event listeners (ensure only one set)
    const ledOnBtn = document.getElementById('led-on');
    const ledOffBtn = document.getElementById('led-off');
    const ledBlinkBtn = document.getElementById('led-blink');
    if (ledOnBtn) {
        ledOnBtn.onclick = function() { sendLedCommand('on'); };
    }
    if (ledOffBtn) {
        ledOffBtn.onclick = function() { sendLedCommand('off'); };
    }
    if (ledBlinkBtn) {
        ledBlinkBtn.onclick = function() { sendLedCommand('blink'); };
    }

    // Touch card periodic refresh
    setInterval(() => {
        fetch('/api/status')
            .then(response => response.json())
            .then(data => updateTouchState(data))
            .catch(() => updateTouchState(null));
    }, 2000);
});
// Touch card logic (migrated from touch.js)
function updateTouchState(data) {
    let state = 'Unbekannt';
    let longPress = 'Unbekannt';
    let indicatorColor = '#b0b0b0';
    if (data && data.touch && typeof data.touch.state === 'string') {
        if (data.touch.state.toLowerCase() === 'touched') {
            state = 'Berührt';
            indicatorColor = '#4caf50'; // grün
        } else if (data.touch.state.toLowerCase() === 'released') {
            state = 'Nicht berührt';
            indicatorColor = '#b0b0b0'; // grau
        } else {
            state = data.touch.state;
        }
        if (typeof data.touch.long_press === 'string') {
            if (data.touch.long_press.toLowerCase() === 'true') {
                longPress = 'Langer Druck: Aktiv';
                indicatorColor = '#e53935'; // rot
            } else if (data.touch.long_press.toLowerCase() === 'false') {
                longPress = 'Langer Druck: Inaktiv';
            } else {
                longPress = 'Langer Druck: ' + data.touch.long_press;
            }
        } else {
            longPress = 'Langer Druck: Unbekannt';
        }
    } else if (data) {
        state = 'Kein Touch-Status';
        longPress = 'Langer Druck: Unbekannt';
    }
    const stateEl = document.getElementById('touch-state');
    const longPressEl = document.getElementById('touch-long-press');
    const indicator = document.getElementById('touch-indicator');
    if (stateEl) stateEl.textContent = state;
    if (longPressEl) longPressEl.textContent = longPress;
    if (indicator) {
        indicator.style.background = indicatorColor;
        indicator.title = state + (longPress.includes('Active') ? ' (Long Press)' : '');
    }
    if (!data) {
        if (stateEl) stateEl.textContent = 'Fehler beim Abrufen des Status';
        if (longPressEl) longPressEl.textContent = '';
        if (indicator) {
            indicator.style.background = '#ccc';
            indicator.title = 'Fehler';
        }
    }
}
function sendLedCommand(command) {
    fetch('/api/led', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ command })
    })
    .then(r => r.json())
    .then(() => setTimeout(refreshLedStatus, 200)); // slight delay for backend update
}
function refreshLedStatus() {
    fetch('/api/status')
        .then(response => response.json())
        .then(data => {
            updateLedIndicator(data.led);
        })
        .catch(err => {
            const indicator = document.getElementById('led-indicator');
            if (indicator) {
                indicator.textContent = 'Failed to load LED data';
                indicator.style.background = '#ccc';
            }
        });
}
function updateLedIndicator(led) {
    const indicator = document.getElementById('led-indicator');
    const blinkRateDiv = document.getElementById('led-blink-rate');
    if (!indicator) return;
    indicator.textContent = '';
    indicator.className = 'led-indicator';
    if (!led) {
        indicator.style.background = '#ccc';
        indicator.title = 'Keine LED-Daten verfügbar';
        if (blinkRateDiv) blinkRateDiv.textContent = '';
        return;
    }
    // Set color and animation based on state/mode
    if (led.mode === 'blink') {
        indicator.classList.add('led-blink');
        indicator.style.background = '#ffd700'; // yellow for blinking
        indicator.title = 'Blinkt';
        // Set animation duration to match configured blink rate (full cycle = 2x blink_rate)
        if (typeof led.blink_rate === 'number' && led.blink_rate > 0) {
            indicator.style.animationDuration = (led.blink_rate * 2) + 'ms';
        } else {
            indicator.style.animationDuration = '';
        }
    } else if (led.state === 'on') {
        indicator.style.background = '#4caf50'; // green for on
        indicator.title = 'An';
        indicator.style.animationDuration = '';
    } else {
        indicator.style.background = '#b0b0b0'; // gray for off
        indicator.title = 'Aus';
        indicator.style.animationDuration = '';
    }
    // Show blink rate if available
    if (blinkRateDiv) {
        if (typeof led.blink_rate === 'number' && led.blink_rate > 0) {
            blinkRateDiv.textContent = `Blinkrate: ${led.blink_rate} ms`;
        } else {
            blinkRateDiv.textContent = '';
        }
    }
}
// index.js - JS for index.html

function updateDeviceAndSystemInfo(data) {
    const deviceInfoEl = document.getElementById('device-info');
    const deviceName = data.config?.device_name || 'Unbekanntes Gerät';
    const sys = data.system || {};
    deviceInfoEl.innerHTML = `
        <div><strong>Gerätename:</strong> <span class="value">${deviceName}</span></div>
        <div><strong>Chip-ID:</strong> <span class="value">${sys.chip_id || 'N/A'}</span></div>
        <div><strong>Freier Heap:</strong> <span class="value">${formatBytes(sys.free_heap || 0)}</span></div>
        <div><strong>Betriebszeit:</strong> <span class="value">${formatUptime(sys.uptime_ms || 0)}</span></div>
        <div><strong>Chip-Revision:</strong> <span class="value">${sys.chip_revision ?? 'N/A'}</span></div>
        <div><strong>CPU-Frequenz:</strong> <span class="value">${sys.cpu_freq_mhz ? sys.cpu_freq_mhz + ' MHz' : 'N/A'}</span></div>
        <div><strong>Flash-Größe:</strong> <span class="value">${formatBytes(sys.flash_chip_size || 0)}</span></div>
        <div><strong>SDK-Version:</strong> <span class="value">${sys.sdk_version || 'N/A'}</span></div>
        <div><strong>Sketch-Größe:</strong> <span class="value">${formatBytes(sys.sketch_size || 0)}</span></div>
        <div><strong>Sketch frei:</strong> <span class="value">${formatBytes(sys.sketch_free_space || 0)}</span></div>
    `;
}

function updateSystemTime(data) {
    const systemTimeEl = document.getElementById('system-time');
    const humanDate = data.date_human || 'Unbekannt';
    const humanTime = data.time_human || 'Unbekannt';
    systemTimeEl.innerHTML = `
        <div><strong>Datum:</strong> <span class="value">${humanDate}</span></div>
        <div><strong>Zeit:</strong> <span class="value">${humanTime}</span></div>
    `;
}

function updateNetworkInfo(data) {
    const net = data.network || {};
    const el = document.getElementById('network-info');
    if (!el) return;
    el.innerHTML = `
        <div><strong>Modus:</strong> <span class="value">${net.mode || 'N/A'}</span></div>
        <div><strong>SSID:</strong> <span class="value">${net.ssid || 'N/A'}</span></div>
        <div><strong>IP:</strong> <span class="value">${net.ip || 'N/A'}</span></div>
        <div><strong>MAC:</strong> <span class="value">${net.mac || 'N/A'}</span></div>
        <div><strong>Verbunden:</strong> <span class="value">${net.connected ? 'Ja' : 'Nein'}</span></div>
        <div><strong>AP aktiv:</strong> <span class="value">${net.ap_active ? 'Ja' : 'Nein'}</span></div>
        <div><strong>AP Clients:</strong> <span class="value">${net.ap_clients ?? 'N/A'}</span></div>
    `;
}

function updateFilesystemInfo(data) {
    const fs = data.filesystem || {};
    const el = document.getElementById('filesystem-info');
    if (!el) return;
    el.innerHTML = `
        <div><strong>Gesamt:</strong> <span class="value">${formatBytes(fs.total_bytes || 0)}</span></div>
        <div><strong>Verwendet:</strong> <span class="value">${formatBytes(fs.used_bytes || 0)}</span></div>
        <div><strong>Frei:</strong> <span class="value">${formatBytes(fs.free_bytes || 0)}</span></div>
    `;
}

function updateHealthInfo(data) {
    const health = data.health || {};
    const el = document.getElementById('health-info');
    if (!el) return;
    let healthHtml = '';
    if (Object.keys(health).length > 0) {
        healthHtml += `<div><strong>Konfiguration:</strong> <span class="value">${health.Config ? 'OK' : 'Fehler'}</span></div>`;
        healthHtml += `<div><strong>Dateisystem:</strong> <span class="value">${health.FileSystem ? 'OK' : 'Fehler'}</span></div>`;
        healthHtml += `<div><strong>Gesamt:</strong> <span class="value">${health.overall ? 'OK' : 'Fehler'}</span></div>`;
    } else {
        healthHtml = 'Keine Gesundheitsinformationen';
    }
    el.innerHTML = healthHtml;
}

function updateI2CInfo(data) {
    const i2c = data.i2c || {};
    const el = document.getElementById('i2c-info');
    if (!el) return;
    let i2cHtml = '';
    // Removed SDA/SCL pin display
    if (Array.isArray(i2c.devices) && i2c.devices.length > 0) {
        i2c.devices.forEach(dev => {
            i2cHtml += `<div><strong>Gerät:</strong> <span class="value">${dev.name || 'Unbekannt'} (${dev.address || 'N/A'})</span></div>`;
        });
    } else {
        i2cHtml += '<div>Keine I2C-Geräte gefunden</div>';
    }
    el.innerHTML = i2cHtml;
}

function formatBytes(bytes) {
    if (bytes < 1024) return bytes + ' B';
    if (bytes < 1024 * 1024) return (bytes / 1024).toFixed(1) + ' KB';
    return (bytes / (1024 * 1024)).toFixed(2) + ' MB';
}

function formatUptime(ms) {
    const sec = Math.floor(ms / 1000);
    const min = Math.floor(sec / 60);
    const hr = Math.floor(min / 60);
    const days = Math.floor(hr / 24);
    return `${days}T ${hr % 24}h ${min % 60}m ${sec % 60}s`;
}
