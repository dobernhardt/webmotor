// app.js
document.addEventListener('DOMContentLoaded', function() {
    // Frequency slider handling
    const frequencySlider = document.getElementById('frequency');
    const frequencyValue = document.getElementById('frequency-value');
    
    frequencySlider.addEventListener('input', function() {
        frequencyValue.textContent = this.value;
    });
    
    // Direction toggle handling
    const directionToggle = document.getElementById('direction');
    const directionText = document.getElementById('direction-text');
    
    directionToggle.addEventListener('change', function() {
        directionText.textContent = this.checked ? 'CW' : 'CCW';
    });
    
    // Button event handlers
    document.getElementById('start').addEventListener('click', function() {
        updateStatus('Motor started');
    });
    
    document.getElementById('stop').addEventListener('click', function() {
        updateStatus('Motor stopped');
    });
    
    document.getElementById('release').addEventListener('click', function() {
        updateStatus('Motor released');
    });
    
    document.getElementById('save-wifi').addEventListener('click', function() {
        const ssid = document.getElementById('ssid').value;
        const password = document.getElementById('password').value;
        if (ssid && password) {
            updateStatus(`WiFi config saved: ${ssid}`);
        } else {
            updateStatus('Error: Please enter SSID and Password');
        }
    });
    
    function updateStatus(message) {
        const statusOutput = document.getElementById('status-output');
        const timestamp = new Date().toLocaleTimeString();
        statusOutput.textContent += `\n[${timestamp}] ${message}`;
        statusOutput.scrollTop = statusOutput.scrollHeight;
    }
});