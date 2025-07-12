// touch.js - JS for touch.html

function updateTouchState() {
    fetch('/api/status')
        .then(response => response.json())
        .then(data => {
            let state = 'Unknown';
            let longPress = 'Unknown';
            let indicatorColor = '#b0b0b0';
            if (data.touch && typeof data.touch.state === 'string') {
                if (data.touch.state.toLowerCase() === 'touched') {
                    state = 'Touched';
                    indicatorColor = '#4caf50'; // green
                } else if (data.touch.state.toLowerCase() === 'released') {
                    state = 'Not Touched';
                    indicatorColor = '#b0b0b0'; // gray
                } else {
                    state = data.touch.state;
                }
                if (typeof data.touch.long_press === 'string') {
                    if (data.touch.long_press.toLowerCase() === 'true') {
                        longPress = 'Long Press: Active';
                        indicatorColor = '#e53935'; // red
                    } else if (data.touch.long_press.toLowerCase() === 'false') {
                        longPress = 'Long Press: Inactive';
                    } else {
                        longPress = 'Long Press: ' + data.touch.long_press;
                    }
                } else {
                    longPress = 'Long Press: Unknown';
                }
            } else {
                state = 'No touch state';
                longPress = 'Long Press: Unknown';
            }
            const stateEl = document.getElementById('touch-state');
            const longPressEl = document.getElementById('touch-long-press');
            const indicator = document.getElementById('touch-indicator');
            if (stateEl) stateEl.textContent = state;
            if (longPressEl) longPressEl.textContent = longPress;
            if (indicator) {
                indicator.style.background = indicatorColor;
                indicator.title = state + (longPress.includes('Active') ? ' (Long Press)' : '');
            }
        })
        .catch(() => {
            const stateEl = document.getElementById('touch-state');
            const longPressEl = document.getElementById('touch-long-press');
            const indicator = document.getElementById('touch-indicator');
            if (stateEl) stateEl.textContent = 'Error fetching state';
            if (longPressEl) longPressEl.textContent = '';
            if (indicator) {
                indicator.style.background = '#ccc';
                indicator.title = 'Error';
            }
        });
}

document.addEventListener('DOMContentLoaded', () => {
    updateTouchState();
    setInterval(updateTouchState, 2000);
});
