// config.js - placeholder for config page logic
// Add your config page JavaScript here.

function createInput(name, value, label, type = 'text') {
    return `<label>${label}: <input name="${name}" value="${value}" type="${type}"></label><br>`;
}
function populateConfigForm(config) {
    let html = '';
    for (const [key, value] of Object.entries(config)) {
        if (typeof value === 'object' && value !== null) continue; // skip nested objects for now
        let label = key.replace(/_/g, ' ').replace(/\b\w/g, l => l.toUpperCase());
        let type = typeof value === 'number' ? 'number' : 'text';
        html += createInput(key, value, label, type);
    }
    document.getElementById('config-fields').innerHTML = html;
}
function loadConfig() {
    fetch('/api/status')
        .then(r => r.json())
        .then(data => {
            if (data.config) {
                populateConfigForm(data.config);
            } else {
                document.getElementById('config-fields').textContent = 'No config data.';
            }
        })
        .catch(() => {
            document.getElementById('config-fields').textContent = 'Error loading config.';
        });
}
document.addEventListener('DOMContentLoaded', function() {
    document.getElementById('config-form').addEventListener('submit', function(e) {
        e.preventDefault();
        const formData = new FormData(e.target);
        const config = {};
        for (const [key, value] of formData.entries()) {
            config[key] = value;
        }
        // TODO: Send config to backend (API endpoint for config update needed)
        document.getElementById('config-status').textContent = 'Config would be sent: ' + JSON.stringify(config, null, 2);
    });
    loadConfig();
});
