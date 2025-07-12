document.addEventListener('DOMContentLoaded', function() {
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

function updateLedIndicator(led) {
    const indicator = document.getElementById('led-indicator');
    if (!indicator) return;
    indicator.textContent = '';
    indicator.className = 'led-indicator';
    if (!led) {
        indicator.style.background = '#ccc';
        indicator.title = 'No LED data available';
        return;
    }
    // Set color and animation based on state/mode
    if (led.mode === 'blink') {
        indicator.classList.add('led-blink');
        indicator.style.background = '#ffd700'; // yellow for blinking
        indicator.title = 'Blinking';
    } else if (led.state === 'on') {
        indicator.style.background = '#4caf50'; // green for on
        indicator.title = 'On';
    } else {
        indicator.style.background = '#b0b0b0'; // gray for off
        indicator.title = 'Off';
    }
}
