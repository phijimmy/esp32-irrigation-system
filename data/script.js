async function loadSystemInfo() {
    try {
        const response = await fetch('/api/status');
        const data = await response.json();
        
        // Update device information
        updateDeviceInfo(data);
        
        // Update system time
        updateSystemTime(data);
        
    } catch (error) {
        console.error('Error fetching system info:', error);
        showError('Failed to load system information');
    }
}

function updateDeviceInfo(data) {
    const deviceInfoEl = document.getElementById('device-info');
    const deviceName = data.config?.device_name || 'Unknown Device';
    
    deviceInfoEl.innerHTML = `
        <div><strong>Device Name:</strong> <span class="value">${deviceName}</span></div>
        <div><strong>Chip ID:</strong> <span class="value">${data.system?.chip_id || 'N/A'}</span></div>
        <div><strong>Free Memory:</strong> <span class="value">${formatBytes(data.system?.free_heap || 0)}</span></div>
        <div><strong>Uptime:</strong> <span class="value">${formatUptime(data.system?.uptime_ms || 0)}</span></div>
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
}

// Load system info when page loads
window.addEventListener('DOMContentLoaded', loadSystemInfo);
