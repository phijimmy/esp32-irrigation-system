document.addEventListener('DOMContentLoaded', function() {
    fetch('/api/status')
        .then(response => response.json())
        .then(data => {
            updateLedStatus(data.led);
        })
        .catch(err => {
            document.getElementById('led-status').textContent = 'Failed to load LED data';
        });
});

function updateLedStatus(led) {
    const statusDiv = document.getElementById('led-status');
    const metaDiv = document.getElementById('led-meta');
    if (!led) {
        statusDiv.textContent = 'No LED data available';
        metaDiv.textContent = '';
        return;
    }
    statusDiv.textContent = `GPIO: ${led.gpio} | State: ${led.state} | Mode: ${led.mode} | Blink Rate: ${led.blink_rate} ms`;
    metaDiv.textContent = led.last_updated ? `Last updated: ${led.last_updated}` : '';
}
