document.addEventListener('DOMContentLoaded', function() {
    function refreshRelayIndicators() {
        fetch('/api/status')
            .then(response => response.json())
            .then(data => {
                updateRelayIndicators(data.relays);
            })
            .catch(err => {
                for (let i = 0; i < 4; i++) {
                    const indicator = document.getElementById('relay-indicator-' + i);
                    if (indicator) {
                        indicator.style.background = '#ccc';
                        indicator.title = 'Failed to load';
                    }
                }
            });
    }
    refreshRelayIndicators();

    function sendRelayCommand(relay, command) {
        fetch('/api/relay', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ relay, command })
        })
        .then(r => r.json())
        .then(() => refreshRelayIndicators());
    }

    document.querySelectorAll('.relay-on').forEach(btn => {
        btn.addEventListener('click', function() {
            const relay = parseInt(this.getAttribute('data-relay'));
            sendRelayCommand(relay, 'on');
        });
    });
    document.querySelectorAll('.relay-off').forEach(btn => {
        btn.addEventListener('click', function() {
            const relay = parseInt(this.getAttribute('data-relay'));
            sendRelayCommand(relay, 'off');
        });
    });
    document.querySelectorAll('.relay-toggle').forEach(btn => {
        btn.addEventListener('click', function() {
            const relay = parseInt(this.getAttribute('data-relay'));
            sendRelayCommand(relay, 'toggle');
        });
    });
});

function updateRelayIndicators(relays) {
    for (let i = 0; i < 4; i++) {
        const indicator = document.getElementById('relay-indicator-' + i);
        if (!indicator) continue;
        indicator.className = 'relay-indicator';
        indicator.textContent = '';
        if (!relays || !relays[i]) {
            indicator.style.background = '#ccc';
            indicator.title = 'No data';
            continue;
        }
        if (relays[i].state === 'on') {
            indicator.style.background = '#4caf50'; // green
            indicator.title = 'On';
        } else {
            indicator.style.background = '#b0b0b0'; // gray
            indicator.title = 'Off';
        }
    }
}
