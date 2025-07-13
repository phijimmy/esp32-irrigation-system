document.addEventListener('DOMContentLoaded', function() {
    fetch('/api/status')
        .then(response => response.json())
        .then(data => {
            const config = data.config || {};
            const select = document.getElementById('network-mode');
            const wifiClientFields = document.getElementById('wifi-client-fields');
            const wifiAPFields = document.getElementById('wifi-ap-fields');
            // Helper to show/hide fields
            function updateFields(mode) {
                if (mode === 'client') {
                    wifiClientFields.style.display = '';
                    wifiAPFields.style.display = 'none';
                } else if (mode === 'ap') {
                    wifiClientFields.style.display = 'none';
                    wifiAPFields.style.display = '';
                } else {
                    wifiClientFields.style.display = 'none';
                    wifiAPFields.style.display = 'none';
                }
            }
            // Set selector and fields
            let mode = 'client';
            if (config.wifi_mode) {
                mode = config.wifi_mode.toLowerCase();
                if (select) select.value = mode;
            }
            // Populate fields
            if (config.wifi_ssid) document.getElementById('wifi-ssid').value = config.wifi_ssid;
            if (config.wifi_pass) document.getElementById('wifi-pass').value = config.wifi_pass;
            if (config.ap_ssid) document.getElementById('ap-ssid').value = config.ap_ssid;
            if (config.ap_password) document.getElementById('ap-password').value = config.ap_password;
            if (config.ap_timeout) document.getElementById('ap-timeout').value = config.ap_timeout;
            updateFields(mode);
            // Change event for selector
            if (select) {
                select.addEventListener('change', function() {
                    updateFields(this.value);
                });
            }

            // Populate LED card fields (must be after config is loaded)
            if (config.led_gpio !== undefined) {
                document.getElementById('led-gpio').value = config.led_gpio;
            }
            if (config.led_blink_rate !== undefined) {
                document.getElementById('led-blink-rate').value = config.led_blink_rate;
            }
            // Render relay config fields
            const relaysContainer = document.getElementById('relays-config-fields');
            if (relaysContainer && typeof config.relay_count === 'number') {
                let html = '';
                const relayNames = Array.isArray(config.relay_names) ? config.relay_names : [];
                for (let i = 0; i < config.relay_count; i++) {
                    const name = relayNames[i] || `Zone ${i+1}`;
                    const gpio = config[`relay_gpio_${i}`] !== undefined ? config[`relay_gpio_${i}`] : '';
                    const activeHigh = config[`relay_active_high_${i}`] !== undefined ? config[`relay_active_high_${i}`] : true;
                    html += `<fieldset style="margin-bottom:1em;"><legend>Relay ${i} Settings</legend>`;
                    html += `<label for="relay-name-${i}">Name:</label> <input type="text" id="relay-name-${i}" value="${name}"><br>`;
                    html += `<label for="relay-gpio-${i}">GPIO:</label> <input type="number" id="relay-gpio-${i}" value="${gpio}"><br>`;
                    html += `<label>Active:</label> `;
                    html += `<label><input type="radio" name="relay-active-high-${i}" value="high" ${activeHigh ? 'checked' : ''}>High</label> `;
                    html += `<label><input type="radio" name="relay-active-high-${i}" value="low" ${!activeHigh ? 'checked' : ''}>Low</label><br>`;
                    if (i === 2) {
                        const relay2Gpio = config.relay2_control_gpio !== undefined ? config.relay2_control_gpio : '';
                        html += `<label for="relay2-control-gpio">Interrupt GPIO:</label> <input type="number" id="relay2-control-gpio" value="${relay2Gpio}"><br>`;
                    }
                    html += `</fieldset>`;
                }
                relaysContainer.innerHTML = html;
            }
        });
    // You can add event listeners here for saving changes later
});
