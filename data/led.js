document.addEventListener('DOMContentLoaded', function() {
    function refreshLedStatus() {
        fetch('/api/status')
            .then(response => response.json())
            .then(data => {
                updateLedStatus(data.led);
            })
            .catch(err => {
                document.getElementById('led-status').textContent = 'Failed to load LED data';
            });
    }
    refreshLedStatus();

    function sendLedCommand(command) {
        fetch('/api/led', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ command })
        })
        .then(r => r.json())
        .then(() => refreshLedStatus());
    }

    document.getElementById('led-on').addEventListener('click', function() { sendLedCommand('on'); });
    document.getElementById('led-off').addEventListener('click', function() { sendLedCommand('off'); });
    document.getElementById('led-toggle').addEventListener('click', function() { sendLedCommand('toggle'); });
    document.getElementById('led-blink').addEventListener('click', function() { sendLedCommand('blink'); });
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
