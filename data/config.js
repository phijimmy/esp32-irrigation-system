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

            // Populate Touch card fields
            if (config.touch_gpio !== undefined) {
                document.getElementById('touch-gpio').value = config.touch_gpio;
            }
            if (config.touch_long_press !== undefined) {
                document.getElementById('touch-long-press').value = config.touch_long_press;
            }
            if (config.touch_threshold !== undefined) {
                document.getElementById('touch-threshold').value = config.touch_threshold;
            }

            // Populate Soil Moisture card fields
            if (config.soil_moisture) {
                if (config.soil_moisture.wet !== undefined) {
                    document.getElementById('soil-moisture-wet').value = config.soil_moisture.wet;
                }
                if (config.soil_moisture.dry !== undefined) {
                    document.getElementById('soil-moisture-dry').value = config.soil_moisture.dry;
                }
            }
            if (config.soil_power_gpio !== undefined) {
                document.getElementById('soil-power-gpio').value = config.soil_power_gpio;
            }

            // Populate Irrigation card fields
            if (config.watering_threshold !== undefined) {
                document.getElementById('watering-threshold').value = config.watering_threshold;
            }
            if (config.watering_duration_sec !== undefined) {
                document.getElementById('watering-duration').value = config.watering_duration_sec;
            }

            // Populate I2C card fields
            if (config.i2c_sda !== undefined) {
                document.getElementById('i2c-sda').value = config.i2c_sda;
            }
            if (config.i2c_scl !== undefined) {
                document.getElementById('i2c-scl').value = config.i2c_scl;
            }

            // Populate System card fields
            if (config.device_name !== undefined) {
                document.getElementById('device-name').value = config.device_name;
            }
            if (config.cpu_speed !== undefined) {
                document.getElementById('cpu-speed').value = config.cpu_speed;
            }
            if (config.brownout_threshold !== undefined) {
                document.getElementById('brownout-threshold').value = config.brownout_threshold;
            }

            // Populate Time/NTP card fields
            if (config.ntp_server_1 !== undefined) {
                document.getElementById('ntp-server-1').value = config.ntp_server_1;
            }
            if (config.ntp_server_2 !== undefined) {
                document.getElementById('ntp-server-2').value = config.ntp_server_2;
            }
            if (config.ntp_enabled !== undefined) {
                document.getElementById('ntp-enabled').checked = !!config.ntp_enabled;
            }
            if (config.ntp_sync_interval !== undefined) {
                document.getElementById('ntp-sync-interval').value = config.ntp_sync_interval;
            }
            if (config.ntp_timeout !== undefined) {
                document.getElementById('ntp-timeout').value = config.ntp_timeout;
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
