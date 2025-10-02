// app.js
document.addEventListener('DOMContentLoaded', function() {
    const frequencyInput = document.getElementById('frequency');
    const microstepsInput = document.getElementById('microsteps');
    const directionToggle = document.getElementById('direction');
    const modeButtons = document.querySelectorAll('input[name="mode"]');
    const statusDisplay = document.getElementById('status');

    function updateStatus() {
        fetch('/api/motor/status')
            .then(response => response.json())
            .then(data => {
                statusDisplay.innerText = `Microsteps: ${data.microsteps}, Frequency: ${data.frequency} Hz, Direction: ${data.direction ? 'CW' : 'CCW'}, Mode: ${data.mode}`;
            })
            .catch(error => console.error('Error fetching motor status:', error));
    }

    function sendControlUpdate() {
        const controlData = {
            frequency: parseInt(frequencyInput.value),
            microsteps: parseInt(microstepsInput.value),
            direction: directionToggle.checked,
            mode: Array.from(modeButtons).find(button => button.checked).value
        };

        fetch('/api/motor/control', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify(controlData)
        })
        .then(response => {
            if (!response.ok) {
                return response.json().then(err => {
                    throw new Error(err.error);
                });
            }
            updateStatus();
        })
        .catch(error => console.error('Error updating motor control:', error));
    }

    frequencyInput.addEventListener('change', sendControlUpdate);
    microstepsInput.addEventListener('change', sendControlUpdate);
    directionToggle.addEventListener('change', sendControlUpdate);
    modeButtons.forEach(button => button.addEventListener('change', sendControlUpdate));

    setInterval(updateStatus, 1000);
});