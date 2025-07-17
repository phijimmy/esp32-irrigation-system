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
        if (typeof showError === 'function') showError('Failed to load system information');
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
    let state = 'Unknown';
    let longPress = 'Unknown';
    let indicatorColor = '#b0b0b0';
    if (data && data.touch && typeof data.touch.state === 'string') {
        if (data.touch.state.toLowerCase() === 'touched') {
            state = 'Touched';
            indicatorColor = '#4caf50'; // green
        } else if (data.touch.state.toLowerCase() === 'released') {
            state = 'Not Touched';
            indicatorColor = '#b0b0b0'; // gray
        } else {
            state = data.touch.state;
        }
        if (typeof data.touch.long_press === 'string') {
            if (data.touch.long_press.toLowerCase() === 'true') {
                longPress = 'Long Press: Active';
                indicatorColor = '#e53935'; // red
            } else if (data.touch.long_press.toLowerCase() === 'false') {
                longPress = 'Long Press: Inactive';
            } else {
                longPress = 'Long Press: ' + data.touch.long_press;
            }
        } else {
            longPress = 'Long Press: Unknown';
        }
    } else if (data) {
        state = 'No touch state';
        longPress = 'Long Press: Unknown';
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
        if (stateEl) stateEl.textContent = 'Error fetching state';
        if (longPressEl) longPressEl.textContent = '';
        if (indicator) {
            indicator.style.background = '#ccc';
            indicator.title = 'Error';
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
        indicator.title = 'No LED data available';
        if (blinkRateDiv) blinkRateDiv.textContent = '';
        return;
    }
    // Set color and animation based on state/mode
    if (led.mode === 'blink') {
        indicator.classList.add('led-blink');
        indicator.style.background = '#ffd700'; // yellow for blinking
        indicator.title = 'Blinking';
        // Set animation duration to match configured blink rate (full cycle = 2x blink_rate)
        if (typeof led.blink_rate === 'number' && led.blink_rate > 0) {
            indicator.style.animationDuration = (led.blink_rate * 2) + 'ms';
        } else {
            indicator.style.animationDuration = '';
        }
    } else if (led.state === 'on') {
        indicator.style.background = '#4caf50'; // green for on
        indicator.title = 'On';
        indicator.style.animationDuration = '';
    } else {
        indicator.style.background = '#b0b0b0'; // gray for off
        indicator.title = 'Off';
        indicator.style.animationDuration = '';
    }
    // Show blink rate if available
    if (blinkRateDiv) {
        if (typeof led.blink_rate === 'number' && led.blink_rate > 0) {
            blinkRateDiv.textContent = `Blink Rate: ${led.blink_rate} ms`;
        } else {
            blinkRateDiv.textContent = '';
        }
    }
}
// index.js - JS for index.html

function updateDeviceAndSystemInfo(data) {
    const deviceInfoEl = document.getElementById('device-info');
    const deviceName = data.config?.device_name || 'Unknown Device';
    const sys = data.system || {};
    deviceInfoEl.innerHTML = `
        <div><strong>Device Name:</strong> <span class="value">${deviceName}</span></div>
        <div><strong>Chip ID:</strong> <span class="value">${sys.chip_id || 'N/A'}</span></div>
        <div><strong>Free Heap:</strong> <span class="value">${formatBytes(sys.free_heap || 0)}</span></div>
        <div><strong>Uptime:</strong> <span class="value">${formatUptime(sys.uptime_ms || 0)}</span></div>
        <div><strong>Chip Revision:</strong> <span class="value">${sys.chip_revision ?? 'N/A'}</span></div>
        <div><strong>CPU Freq:</strong> <span class="value">${sys.cpu_freq_mhz ? sys.cpu_freq_mhz + ' MHz' : 'N/A'}</span></div>
        <div><strong>Flash Size:</strong> <span class="value">${formatBytes(sys.flash_chip_size || 0)}</span></div>
        <div><strong>SDK Version:</strong> <span class="value">${sys.sdk_version || 'N/A'}</span></div>
        <div><strong>Sketch Size:</strong> <span class="value">${formatBytes(sys.sketch_size || 0)}</span></div>
        <div><strong>Sketch Free:</strong> <span class="value">${formatBytes(sys.sketch_free_space || 0)}</span></div>
    `;
}

function updateSystemTime(data) {
    const systemTimeEl = document.getElementById('system-time');
    const humanDate = data.date_human || 'Unknown';
    const humanTime = data.time_human || 'Unknown';
    systemTimeEl.innerHTML = `
        <div><strong>Date:</strong> <span class="value">${humanDate}</span></div>
        <div><strong>Time:</strong> <span class="value">${humanTime}</span></div>
    `;
}

function updateNetworkInfo(data) {
    const net = data.network || {};
    const el = document.getElementById('network-info');
    if (!el) return;
    el.innerHTML = `
        <div><strong>Mode:</strong> <span class="value">${net.mode || 'N/A'}</span></div>
        <div><strong>SSID:</strong> <span class="value">${net.ssid || 'N/A'}</span></div>
        <div><strong>IP:</strong> <span class="value">${net.ip || 'N/A'}</span></div>
        <div><strong>MAC:</strong> <span class="value">${net.mac || 'N/A'}</span></div>
        <div><strong>Connected:</strong> <span class="value">${net.connected ? 'Yes' : 'No'}</span></div>
        <div><strong>AP Active:</strong> <span class="value">${net.ap_active ? 'Yes' : 'No'}</span></div>
        <div><strong>AP Clients:</strong> <span class="value">${net.ap_clients ?? 'N/A'}</span></div>
    `;
}

function updateFilesystemInfo(data) {
    const fs = data.filesystem || {};
    const el = document.getElementById('filesystem-info');
    if (!el) return;
    el.innerHTML = `
        <div><strong>Total:</strong> <span class="value">${formatBytes(fs.total_bytes || 0)}</span></div>
        <div><strong>Used:</strong> <span class="value">${formatBytes(fs.used_bytes || 0)}</span></div>
        <div><strong>Free:</strong> <span class="value">${formatBytes(fs.free_bytes || 0)}</span></div>
    `;
}

function updateHealthInfo(data) {
    const health = data.health || {};
    const el = document.getElementById('health-info');
    if (!el) return;
    let healthHtml = '';
    if (Object.keys(health).length > 0) {
        healthHtml += `<div><strong>Config:</strong> <span class="value">${health.Config ? 'OK' : 'Fail'}</span></div>`;
        healthHtml += `<div><strong>FileSystem:</strong> <span class="value">${health.FileSystem ? 'OK' : 'Fail'}</span></div>`;
        healthHtml += `<div><strong>Overall:</strong> <span class="value">${health.overall ? 'OK' : 'Fail'}</span></div>`;
    } else {
        healthHtml = 'No health info';
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
            i2cHtml += `<div><strong>Device:</strong> <span class="value">${dev.name || 'Unknown'} (${dev.address || 'N/A'})</span></div>`;
        });
    } else {
        i2cHtml += '<div>No I2C devices found</div>';
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
    return `${days}d ${hr % 24}h ${min % 60}m ${sec % 60}s`;
}
