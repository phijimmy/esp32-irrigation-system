document.addEventListener('DOMContentLoaded', function() {
    // Dynamically render relay controls using names from API
    function renderRelayControls(relays) {
        const container = document.getElementById('relay-controls-dynamic');
        if (!container) return;
        container.innerHTML = '';
        if (!relays) return;
        relays.forEach((relay, i) => {
            const div = document.createElement('div');
            div.innerHTML = `
                <span>${relay.name || 'Zone ' + (i+1)}: </span>
                <button class="relay-on" data-relay="${i}">On</button>
                <button class="relay-off" data-relay="${i}">Off</button>
                <span id="relay-indicator-${i}" class="relay-indicator"></span>
            `;
            container.appendChild(div);
        });
        // Re-attach event listeners for new buttons
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
    }
    function refreshRelayIndicators() {
        fetch('/api/status')
            .then(response => response.json())
            .then(data => {
                renderRelayControls(data.relays);
                updateRelayIndicators(data.relays);
            })
            .catch(err => {
                const container = document.getElementById('relay-controls-dynamic');
                if (container) container.innerHTML = '<div style="color:red">Failed to load relay data</div>';
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
