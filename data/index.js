// index.js - JS for index.html

async function loadSystemInfo() {
    try {
        const response = await fetch('/api/status');
        const data = await response.json();
        updateDeviceAndSystemInfo(data);
        updateSystemTime(data);
        updateNetworkInfo(data);
        updateFilesystemInfo(data);
        updateHealthInfo(data);
        updateI2CInfo(data);
    } catch (error) {
        console.error('Error fetching system info:', error);
        showError('Failed to load system information');
    }
}

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
        <div><strong>ISO Format:</strong> <span class="value">${data.datetime_iso || 'N/A'}</span></div>
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
    if (i2c.sda_pin !== undefined && i2c.scl_pin !== undefined) {
        i2cHtml += `<div><strong>SDA Pin:</strong> <span class="value">${i2c.sda_pin}</span></div>`;
        i2cHtml += `<div><strong>SCL Pin:</strong> <span class="value">${i2c.scl_pin}</span></div>`;
    }
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
    if (bytes === 0) return '0 Bytes';
    const k = 1024;
    const sizes = ['Bytes', 'KB', 'MB', 'GB'];
    const i = Math.floor(Math.log(bytes) / Math.log(k));
    return parseFloat((bytes / Math.pow(k, i)).toFixed(2)) + ' ' + sizes[i];
}

function formatUptime(ms) {
    const seconds = Math.floor(ms / 1000);
    const minutes = Math.floor(seconds / 60);
    const hours = Math.floor(minutes / 60);
    const days = Math.floor(hours / 24);
    if (days > 0) return `${days}d ${hours % 24}h ${minutes % 60}m`;
    if (hours > 0) return `${hours}h ${minutes % 60}m`;
    if (minutes > 0) return `${minutes}m ${seconds % 60}s`;
    return `${seconds}s`;
}

function showError(message) {
    document.getElementById('device-info').innerHTML = `<div class="error">${message}</div>`;
    document.getElementById('system-time').innerHTML = `<div class="error">${message}</div>`;
    const netEl = document.getElementById('network-info');
    if (netEl) netEl.innerHTML = `<div class="error">${message}</div>`;
    const fsEl = document.getElementById('filesystem-info');
    if (fsEl) fsEl.innerHTML = `<div class="error">${message}</div>`;
    const healthEl = document.getElementById('health-info');
    if (healthEl) healthEl.innerHTML = `<div class="error">${message}</div>`;
    const i2cEl = document.getElementById('i2c-info');
    if (i2cEl) i2cEl.innerHTML = `<div class="error">${message}</div>`;
}

window.addEventListener('DOMContentLoaded', loadSystemInfo);
