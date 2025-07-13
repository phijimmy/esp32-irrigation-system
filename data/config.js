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

            // Populate scheduled hour and minute dropdowns
            const hourSelect = document.getElementById('irrigation-scheduled-hour');
            const minuteSelect = document.getElementById('irrigation-scheduled-minute');
            if (hourSelect && hourSelect.options.length === 0) {
                for (let h = 0; h < 24; h++) {
                    let opt = document.createElement('option');
                    opt.value = h;
                    opt.text = h.toString().padStart(2, '0');
                    hourSelect.appendChild(opt);
                }
            }
            if (minuteSelect && minuteSelect.options.length === 0) {
                for (let m = 0; m < 60; m++) {
                    let opt = document.createElement('option');
                    opt.value = m;
                    opt.text = m.toString().padStart(2, '0');
                    minuteSelect.appendChild(opt);
                }
            }
            if (config.irrigation_scheduled_hour !== undefined) {
                hourSelect.value = config.irrigation_scheduled_hour;
            }
            if (config.irrigation_scheduled_minute !== undefined) {
                minuteSelect.value = config.irrigation_scheduled_minute;
            }

            // Populate I2C card fields
            if (config.i2c_sda !== undefined) {
                document.getElementById('i2c-sda').value = config.i2c_sda;
            }
            if (config.i2c_scl !== undefined) {
                document.getElementById('i2c-scl').value = config.i2c_scl;
            }

            // Save config handler
            const form = document.getElementById('config-form');
            if (form) {
                form.addEventListener('submit', function(e) {
                    e.preventDefault();
                    // Gather all config values from the form
                    const formData = new FormData(form);
                    // Build config object (add all fields you want to save)
                    let newConfig = {};
                    // Network
                    newConfig.wifi_mode = document.getElementById('network-mode').value;
                    newConfig.wifi_ssid = document.getElementById('wifi-ssid').value;
                    newConfig.wifi_pass = document.getElementById('wifi-pass').value;
                    newConfig.ap_ssid = document.getElementById('ap-ssid').value;
                    newConfig.ap_password = document.getElementById('ap-password').value;
                    newConfig.ap_timeout = parseInt(document.getElementById('ap-timeout').value);
                    // LED
                    newConfig.led_gpio = parseInt(document.getElementById('led-gpio').value);
                    newConfig.led_blink_rate = parseInt(document.getElementById('led-blink-rate').value);
                    // Touch
                    newConfig.touch_gpio = parseInt(document.getElementById('touch-gpio').value);
                    newConfig.touch_long_press = parseInt(document.getElementById('touch-long-press').value);
                    newConfig.touch_threshold = parseInt(document.getElementById('touch-threshold').value);
                    // System
                    newConfig.device_name = document.getElementById('device-name').value;
                    newConfig.cpu_speed = parseInt(document.getElementById('cpu-speed').value);
                    newConfig.brownout_threshold = parseFloat(document.getElementById('brownout-threshold').value);
                    // NTP/Time
                    newConfig.ntp_server_1 = document.getElementById('ntp-server-1').value;
                    newConfig.ntp_server_2 = document.getElementById('ntp-server-2').value;
                    newConfig.ntp_enabled = document.getElementById('ntp-enabled').checked;
                    newConfig.ntp_sync_interval = parseInt(document.getElementById('ntp-sync-interval').value);
                    newConfig.ntp_timeout = parseInt(document.getElementById('ntp-timeout').value);
                    // Soil Moisture
                    newConfig.soil_moisture = {
                        wet: parseInt(document.getElementById('soil-moisture-wet').value),
                        dry: parseInt(document.getElementById('soil-moisture-dry').value)
                    };
                    newConfig.soil_power_gpio = parseInt(document.getElementById('soil-power-gpio').value);
                    // Relays
                    let relayCount = (typeof config.relay_count === 'number') ? config.relay_count : 0;
                    newConfig.relay_count = relayCount;
                    for (let i = 0; i < relayCount; i++) {
                        newConfig['relay_gpio_' + i] = parseInt(document.getElementById('relay-gpio-' + i).value);
                        // Find checked radio for active high/low
                        let radios = document.getElementsByName('relay-active-high-' + i);
                        let activeHigh = true;
                        for (let r = 0; r < radios.length; r++) {
                            if (radios[r].checked && radios[r].value === 'low') activeHigh = false;
                        }
                        newConfig['relay_active_high_' + i] = activeHigh;
                    }
                    // Relay names
                    let relayNames = [];
                    for (let i = 0; i < relayCount; i++) {
                        relayNames.push(document.getElementById('relay-name-' + i).value);
                    }
                    newConfig.relay_names = relayNames;
                    // Relay 2 interrupt GPIO (if present)
                    if (relayCount > 2) {
                        const relay2GpioElem = document.getElementById('relay2-control-gpio');
                        if (relay2GpioElem) newConfig.relay2_control_gpio = parseInt(relay2GpioElem.value);
                    }
                    // Irrigation
                    newConfig.watering_threshold = parseFloat(document.getElementById('watering-threshold').value);
                    newConfig.watering_duration_sec = parseInt(document.getElementById('watering-duration').value);
                    newConfig.irrigation_scheduled_hour = parseInt(document.getElementById('irrigation-scheduled-hour').value);
                    newConfig.irrigation_scheduled_minute = parseInt(document.getElementById('irrigation-scheduled-minute').value);
                    // I2C
                    newConfig.i2c_sda = document.getElementById('i2c-sda').value;
                    newConfig.i2c_scl = document.getElementById('i2c-scl').value;

                    fetch('/api/config', {
                        method: 'POST',
                        headers: {'Content-Type': 'application/json'},
                        body: JSON.stringify(newConfig)
                    }).then(r => r.json()).then(resp => {
                        alert('Config saved!');
                    });
                });
            }

            // Reset to defaults handler
            const resetBtn = document.getElementById('reset-defaults-btn');
            if (resetBtn) {
                resetBtn.addEventListener('click', function() {
                    if (confirm('Are you sure you want to reset to defaults?')) {
                        fetch('/api/config/reset', { method: 'POST' })
                            .then(() => { alert('Config reset to defaults. Rebooting...'); location.reload(); });
                    }
                });
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
