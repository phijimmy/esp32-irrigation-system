document.addEventListener('DOMContentLoaded', function() {
    fetch('/api/status')
        .then(response => response.json())
        .then(data => {
            const config = data.config || {};
            // INT/SQW GPIO
            if (config.int_sqw_gpio !== undefined) {
                document.getElementById('int-sqw-gpio').value = config.int_sqw_gpio;
            }
            // Schedule settings
            if (config.use_weekly_schedule !== undefined) {
                document.getElementById('use-weekly-schedule').checked = !!config.use_weekly_schedule;
            }
            if (config.auto_update_daily !== undefined) {
                document.getElementById('auto-update-daily').checked = !!config.auto_update_daily;
            }
            // Weekly schedule table with editable dropdowns
            const schedule = config.weekly_schedule || {};
            const days = ['sunday','monday','tuesday','wednesday','thursday','friday','saturday'];
            let html = '<table class="schedule-table"><thead><tr><th>Day</th><th>Alarm 1</th><th>Alarm 2</th></tr></thead><tbody>';
            function dropdown(name, value, max) {
                let opts = '';
                for (let i = 0; i <= max; i++) {
                    opts += `<option value="${i}"${i==value?' selected':''}>${i.toString().padStart(2,'0')}</option>`;
                }
                return `<select name="${name}">${opts}</select>`;
            }
            function enabledBox(name, checked) {
                return `<input type="checkbox" name="${name}"${checked?' checked':''}>`;
            }
            days.forEach(day => {
                const d = schedule[day] || {};
                const a1 = d.alarm1 || {};
                const a2 = d.alarm2 || {};
                html += `<tr><td>${day.charAt(0).toUpperCase() + day.slice(1)}</td>` +
                    `<td>${enabledBox(day+'_a1_enabled', a1.enabled)} ` +
                        dropdown(day+'_a1_hour', a1.hour ?? 18, 23) + ':' +
                        dropdown(day+'_a1_minute', a1.minute ?? 0, 59) + ':' +
                        dropdown(day+'_a1_second', a1.second ?? 0, 59) +
                    `</td>` +
                    `<td>${enabledBox(day+'_a2_enabled', a2.enabled)} ` +
                        dropdown(day+'_a2_hour', a2.hour ?? 22, 23) + ':' +
                        dropdown(day+'_a2_minute', a2.minute ?? 0, 59) +
                    `</td></tr>`;
            });
            html += '</tbody></table>';
            document.getElementById('weekly-schedule-form').insertAdjacentHTML('afterbegin', html);

            // Save handler
            document.getElementById('save-schedule-btn').onclick = function() {
                const form = document.getElementById('weekly-schedule-form');
                const formData = new FormData(form);
                let newSchedule = {};
                days.forEach(day => {
                    newSchedule[day] = {
                        alarm1: {
                            enabled: formData.get(day+'_a1_enabled') === 'on',
                            hour: parseInt(formData.get(day+'_a1_hour')),
                            minute: parseInt(formData.get(day+'_a1_minute')),
                            second: parseInt(formData.get(day+'_a1_second'))
                        },
                        alarm2: {
                            enabled: formData.get(day+'_a2_enabled') === 'on',
                            hour: parseInt(formData.get(day+'_a2_hour')),
                            minute: parseInt(formData.get(day+'_a2_minute'))
                        }
                    };
                });
                fetch('/api/config', {
                    method: 'POST',
                    headers: {'Content-Type': 'application/json'},
                    body: JSON.stringify({weekly_schedule: newSchedule})
                }).then(r => r.json()).then(resp => {
                    alert('Schedule saved!');
                });
            };
        });
});
