// Render relay table with left-aligned names and large, spaced buttons
document.addEventListener('DOMContentLoaded', function() {
    const container = document.getElementById('relay-table-container');
    if (!container) return;

    function renderRelayTable(data) {
        let relayCount = 4;
        let relayNames = ["Zone 1", "Zone 2", "Zone 3", "Zone 4"];
        if (data && data.config && Array.isArray(data.config.relay_names)) {
            relayNames = data.config.relay_names.map((n, i) => n || `Zone ${i+1}`);
        }
        if (data && data.config && typeof data.config.relay_count === 'number') {
            relayCount = data.config.relay_count;
        }
        let relayStates = [];
        if (data && data.relays && Array.isArray(data.relays)) {
            // Each relay is an object with .state ('on'|'off')
            relayStates = data.relays.map(r => r && r.state === 'on');
        } else {
            relayStates = Array(relayCount).fill(false);
        }

        let html = '<table id="relay-table">';
        for (let i = 0; i < relayCount; i++) {
            html += '<tr>';
            html += `<td class="name-col">${relayNames[i] || `Zone ${i+1}`}</td>`;
            html += `<td class="button-col">
                <div style="display:flex;align-items:center;justify-content:flex-end;gap:0.7em;">
                    <button class="relay-btn relay-on" data-relay="${i}"${relayStates[i] ? ' disabled' : ''}>An</button>
                    <button class="relay-btn relay-off" data-relay="${i}"${!relayStates[i] ? ' disabled' : ''}>Aus</button>
                    <span class="relay-indicator" id="relay-indicator-${i}" style="display:inline-block;width:1.2em;height:1.2em;border-radius:50%;background:${relayStates[i] ? '#27ae60' : '#888'};vertical-align:middle;"></span>
                </div>
            </td>`;
            html += '</tr>';
        }
        html += '</table>';
        container.innerHTML = html;

        // Add event listeners for ON/OFF buttons
        container.querySelectorAll('.relay-on').forEach(btn => {
            btn.addEventListener('click', function() {
                const idx = parseInt(this.getAttribute('data-relay'));
                fetch('/api/relay', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ relay: idx, command: 'on' })
                })
                .then(r => {
                    if (!r.ok) throw new Error('Relay ON failed');
                    return r.json();
                })
                .then(() => {
                    setTimeout(fetchAndRender, 200);
                })
                .catch(err => {
                    showError(`Relais ${idx+1} konnte nicht eingeschaltet werden: ${err.message}`);
                });
            });
        });
        container.querySelectorAll('.relay-off').forEach(btn => {
            btn.addEventListener('click', function() {
                const idx = parseInt(this.getAttribute('data-relay'));
                fetch('/api/relay', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ relay: idx, command: 'off' })
                })
                .then(r => {
                    if (!r.ok) throw new Error('Relay OFF failed');
                    return r.json();
                })
                .then(() => {
                    setTimeout(fetchAndRender, 200);
                })
                .catch(err => {
                    showError(`Relais ${idx+1} konnte nicht ausgeschaltet werden: ${err.message}`);
                });
            });
        });
    }

    // No need for updateRelayRow; table is always re-rendered from backend state

    function showError(msg) {
        container.innerHTML = `<div style='color:red;'>${msg}</div>`;
    }

    function fetchAndRender() {
        fetch('/api/status')
            .then(r => {
                if (!r.ok) throw new Error('Network response was not ok');
                return r.json();
            })
            .then(data => {
                if (!data || !data.config || (typeof data.config.relay_count !== 'number' && !Array.isArray(data.relays))) {
                    container.innerHTML = '<div style="color:red;">Keine Relaisdaten in der API-Antwort gefunden.</div>';
                    return;
                }
                renderRelayTable(data);
            })
            .catch(err => {
                container.innerHTML = `<div style='color:red;'>Relaisdaten konnten nicht geladen werden: ${err.message}</div>`;
            });
    }

    // Initial fetch and render
    fetchAndRender();
});
