document.addEventListener('DOMContentLoaded', function() {
    fetch('/api/status')
        .then(response => response.json())
        .then(data => {
            // DashboardManager JSON: relays is a top-level property
            populateRelayTable(data.relays);
        })
        .catch(err => {
            document.querySelector('#relay-table tbody').innerHTML = '<tr><td colspan="4">Failed to load relay data</td></tr>';
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
