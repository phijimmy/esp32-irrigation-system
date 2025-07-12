document.addEventListener('DOMContentLoaded', function() {
    function refreshRelayTable() {
        fetch('/api/status')
            .then(response => response.json())
            .then(data => {
                populateRelayTable(data.relays);
            })
            .catch(err => {
                document.querySelector('#relay-table tbody').innerHTML = '<tr><td colspan="4">Failed to load relay data</td></tr>';
            });
    }
    refreshRelayTable();

    function sendRelayCommand(relay, command) {
        fetch('/api/relay', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ relay, command })
        })
        .then(r => r.json())
        .then(() => refreshRelayTable());
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

function populateRelayTable(relays) {
    const tbody = document.querySelector('#relay-table tbody');
    tbody.innerHTML = '';
    if (!relays || relays.length === 0) {
        tbody.innerHTML = '<tr><td colspan="4">No relay data available</td></tr>';
        return;
    }
    relays.forEach(relay => {
        const tr = document.createElement('tr');
        tr.innerHTML = `
            <td>${relay.index}</td>
            <td>${relay.gpio}</td>
            <td>${relay.state}</td>
            <td>${relay.mode !== undefined ? relay.mode : ''}</td>
        `;
        tbody.appendChild(tr);
    });
}
