// --- Restart Button Logic ---
document.addEventListener('DOMContentLoaded', function() {
    const restartBtn = document.getElementById('restart-btn');
    const clearStatus = document.getElementById('clear-config-status');
    if (restartBtn) {
        restartBtn.addEventListener('click', function() {
            if (!confirm('Are you sure you want to restart the device?')) return;
            restartBtn.disabled = true;
            clearStatus.textContent = 'Restarting...';
            fetch('/api/restart', { method: 'POST' })
                .then(r => r.json())
                .then(data => {
                    if (data && data.result === 'ok') {
                        clearStatus.textContent = 'Restarting device...';
                    } else {
                        clearStatus.textContent = 'Failed to restart device.';
                    }
                })
                .catch(() => {
                    clearStatus.textContent = 'Failed to restart device.';
                })
                .finally(() => {
                    restartBtn.disabled = false;
                });
        });
    }
});
// --- Clear Config Button Logic ---
document.addEventListener('DOMContentLoaded', function() {
    const clearBtn = document.getElementById('clear-config-btn');
    const clearStatus = document.getElementById('clear-config-status');
    if (clearBtn) {
        clearBtn.addEventListener('click', function() {
            if (!confirm('Are you sure you want to clear the config? This will delete config.json and restore defaults after reboot.')) return;
            clearBtn.disabled = true;
            clearStatus.textContent = 'Clearing config...';
            fetch('/api/clearconfig', { method: 'POST' })
                .then(r => r.json())
                .then(data => {
                    if (data && data.result === 'ok') {
                        clearStatus.textContent = 'Config cleared. Please reboot the device.';
                    } else {
                        clearStatus.textContent = data && data.error ? data.error : 'Failed to clear config.';
                    }
                })
                .catch(() => {
                    clearStatus.textContent = 'Failed to clear config.';
                })
                .finally(() => {
                    clearBtn.disabled = false;
                });
        });
    }
});
document.addEventListener('DOMContentLoaded', function() {

    // --- Device Time Dropdowns Logic ---
    // Helper to populate dropdowns
    function populateDropdown(id, min, max, pad, selected) {
        const sel = document.getElementById(id);
        if (!sel) return;
        sel.innerHTML = '';
        for (let v = min; v <= max; v++) {
            let opt = document.createElement('option');
            opt.value = v;
            opt.text = pad ? v.toString().padStart(2, '0') : v;
            if (v === selected) opt.selected = true;
            sel.appendChild(opt);
        }
    }
    // Set dropdowns to a JS Date object
    function setDropdownsToDate(dt) {
        populateDropdown('manual-year', 2000, 2099, false, dt.getFullYear());
        populateDropdown('manual-month', 1, 12, true, dt.getMonth() + 1);
        // Days in month
        let daysInMonth = new Date(dt.getFullYear(), dt.getMonth() + 1, 0).getDate();
        populateDropdown('manual-day', 1, daysInMonth, true, dt.getDate());
        populateDropdown('manual-hour', 0, 23, true, dt.getHours());
        populateDropdown('manual-minute', 0, 59, true, dt.getMinutes());
        populateDropdown('manual-second', 0, 59, true, dt.getSeconds());
    }
    // Update days if year/month changes
    function addDayDropdownUpdater() {
        const yearSel = document.getElementById('manual-year');
        const monthSel = document.getElementById('manual-month');
        const daySel = document.getElementById('manual-day');
        if (yearSel && monthSel && daySel) {
            function updateDays() {
                let y = parseInt(yearSel.value);
                let m = parseInt(monthSel.value);
                let d = parseInt(daySel.value);
                let daysInMonth = new Date(y, m, 0).getDate();
                let prev = d;
                populateDropdown('manual-day', 1, daysInMonth, true, Math.min(prev, daysInMonth));
            }
            yearSel.addEventListener('change', updateDays);
            monthSel.addEventListener('change', updateDays);
        }
    }
    // On load, set dropdowns to device time
    fetch('/api/status')
        .then(r => r.json())
        .then(data => {
            let dt = new Date();
            // Try to parse device time from API
            let tstr = null;
            if (data && data.status && data.status.time) tstr = data.status.time;
            else if (data && data.time) tstr = data.time;
            if (tstr) {
                // Accepts ISO or 'YYYY-MM-DD HH:MM:SS'
                let m = tstr.match(/(\d{4})-(\d{2})-(\d{2})[ T](\d{2}):(\d{2}):(\d{2})/);
                if (m) {
                    dt = new Date(
                        parseInt(m[1]),
                        parseInt(m[2]) - 1,
                        parseInt(m[3]),
                        parseInt(m[4]),
                        parseInt(m[5]),
                        parseInt(m[6])
                    );
                }
            }
            setDropdownsToDate(dt);
            addDayDropdownUpdater();
        });

    // Sync to Browser button
    const syncBtn = document.getElementById('sync-browser-time-btn');
    if (syncBtn) {
        syncBtn.addEventListener('click', function() {
            setDropdownsToDate(new Date());
        });
    }

    // Set Device Time button
    const setTimeBtn = document.getElementById('set-device-time-btn');
    if (setTimeBtn) {
        setTimeBtn.addEventListener('click', function() {
            const year = parseInt(document.getElementById('manual-year').value);
            const month = parseInt(document.getElementById('manual-month').value);
            const day = parseInt(document.getElementById('manual-day').value);
            const hour = parseInt(document.getElementById('manual-hour').value);
            const minute = parseInt(document.getElementById('manual-minute').value);
            const second = parseInt(document.getElementById('manual-second').value);
            const statusDiv = document.getElementById('set-time-status');
            statusDiv.textContent = '';
            if (!(year && month && day && hour >= 0 && minute >= 0 && second >= 0)) {
                statusDiv.textContent = 'Please fill all date/time fields.';
                return;
            }
            setTimeBtn.disabled = true;
            fetch('/api/settime', {
                method: 'POST',
                headers: {'Content-Type': 'application/json'},
                body: JSON.stringify({ year, month, day, hour, minute, second })
            })
            .then(r => r.json())
            .then(resp => {
                if (resp && resp.result === 'ok') {
                    statusDiv.textContent = 'Device time set!';
                } else {
                    statusDiv.textContent = resp && resp.error ? resp.error : 'Failed to set time.';
                }
            })
            .catch(() => {
                statusDiv.textContent = 'Failed to set time (network error).';
            })
            .finally(() => {
                setTimeBtn.disabled = false;
            });
        });
    }
    // --- End Device Time Dropdowns Logic ---

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
                if (config.soil_moisture.stabilisation_time !== undefined) {
                    document.getElementById('soil-stabilisation-time').value = config.soil_moisture.stabilisation_time;
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
                    newConfig.brownout_threshold = document.getElementById('brownout-threshold').value.toString();
                    // NTP/Time
                    newConfig.ntp_server_1 = document.getElementById('ntp-server-1').value;
                    newConfig.ntp_server_2 = document.getElementById('ntp-server-2').value;
                    newConfig.ntp_enabled = document.getElementById('ntp-enabled').checked;
                    newConfig.ntp_sync_interval = parseInt(document.getElementById('ntp-sync-interval').value);
                    newConfig.ntp_timeout = parseInt(document.getElementById('ntp-timeout').value);
                    // Soil Moisture
                    newConfig.soil_moisture = {
                        wet: parseInt(document.getElementById('soil-moisture-wet').value),
                        dry: parseInt(document.getElementById('soil-moisture-dry').value),
                        stabilisation_time: parseInt(document.getElementById('soil-stabilisation-time').value)
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


                    fetch('/api/config', {
                        method: 'POST',
                        headers: {'Content-Type': 'application/json'},
                        body: JSON.stringify(newConfig)
                    }).then(r => r.json()).then(resp => {
                        alert('Config saved!');
                    });
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
