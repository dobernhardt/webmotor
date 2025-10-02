// app.js
document.addEventListener('DOMContentLoaded', function() {
    // API base URL (will be the device's IP when served from ATOM S3)
    const API_BASE = '';
    
    // Connection state tracking
    let isConnected = false;
    let currentMotorMode = 'STOPPED';
    
    // Frequency slider handling
    const frequencySlider = document.getElementById('frequency');
    const frequencyValue = document.getElementById('frequency-value');
    
    frequencySlider.addEventListener('input', function() {
        frequencyValue.textContent = this.value;
        // Send frequency update to device
        sendMotorControl('frequency', parseInt(this.value));
    });
    
    // Direction toggle handling
    const directionToggle = document.getElementById('direction');
    const directionText = document.getElementById('direction-text');
    
    directionToggle.addEventListener('change', function() {
        const direction = this.checked;
        directionText.textContent = direction ? 'CW' : 'CCW';
        // Send direction update to device (true = CW, false = CCW)
        sendMotorControl('direction', direction);
    });
    
    // Microstepping handling
    document.getElementById('microsteps').addEventListener('change', function() {
        sendMotorControl('microsteps', parseInt(this.value));
    });
    
    // Motor control button handlers
    document.getElementById('start').addEventListener('click', function() {
        sendMotorControl('mode', 'RUNNING')
            .then(() => {
                currentMotorMode = 'RUNNING';
                updateButtonStates();
                updateStatus('Motor started');
            })
            .catch(error => updateStatus(`Error starting motor: ${error.message}`));
    });
    
    document.getElementById('stop').addEventListener('click', function() {
        sendMotorControl('mode', 'STOPPED')
            .then(() => {
                currentMotorMode = 'STOPPED';
                updateButtonStates();
                updateStatus('Motor stopped');
            })
            .catch(error => updateStatus(`Error stopping motor: ${error.message}`));
    });
    
    document.getElementById('release').addEventListener('click', function() {
        sendMotorControl('mode', 'RELEASED')
            .then(() => {
                currentMotorMode = 'RELEASED';
                updateButtonStates();
                updateStatus('Motor released');
            })
            .catch(error => updateStatus(`Error releasing motor: ${error.message}`));
    });
    
    // WiFi configuration
    document.getElementById('save-wifi').addEventListener('click', function() {
        const ssid = document.getElementById('ssid').value;
        const password = document.getElementById('password').value;
        
        if (ssid && password) {
            const wifiConfig = {
                ssid: ssid,
                password: password
            };
            
            sendRequest('/api/wifi/config', 'POST', wifiConfig)
                .then(() => {
                    updateStatus(`WiFi config saved: ${ssid}`);
                    // Clear password field for security
                    document.getElementById('password').value = '';
                })
                .catch(error => updateStatus(`Error saving WiFi config: ${error.message}`));
        } else {
            updateStatus('Error: Please enter SSID and Password');
        }
    });
    
    // Function to send motor control commands using the /api/motor/control endpoint
    function sendMotorControl(parameter, value) {
        const data = {};
        data[parameter] = value;
        
        return sendRequest('/api/motor/control', 'POST', data)
            .then(() => updateStatus(`${parameter} set to ${value}`))
            .catch(error => updateStatus(`Error setting ${parameter}: ${error.message}`));
    }
    
    // Generic HTTP request function
    function sendRequest(endpoint, method = 'GET', data = null) {
        const options = {
            method: method,
            headers: {
                'Content-Type': 'application/json',
            }
        };
        
        if (data && (method === 'POST' || method === 'PUT')) {
            options.body = JSON.stringify(data);
        }
        
        return fetch(API_BASE + endpoint, options)
            .then(response => {
                if (!response.ok) {
                    throw new Error(`HTTP ${response.status}: ${response.statusText}`);
                }
                return response.json();
            });
    }
    
    // Status update function with log limit
    function updateStatus(message) {
        const statusOutput = document.getElementById('status-output');
        const timestamp = new Date().toLocaleTimeString();
        const newLine = statusOutput.textContent.trim() ? '\n' : '';
        statusOutput.textContent += `${newLine}[${timestamp}] ${message}`;
        
        // Limit log entries to prevent infinite growth
        const lines = statusOutput.textContent.split('\n');
        if (lines.length > 50) {
            statusOutput.textContent = lines.slice(-50).join('\n');
        }
        
        statusOutput.scrollTop = statusOutput.scrollHeight;
    }
    
    // Update button visual states based on motor mode
    function updateButtonStates() {
        const startBtn = document.getElementById('start');
        const stopBtn = document.getElementById('stop');
        const releaseBtn = document.getElementById('release');
        
        // Reset all button states
        startBtn.classList.remove('active');
        stopBtn.classList.remove('active');
        releaseBtn.classList.remove('active');
        
        // Set active button based on current mode
        switch(currentMotorMode) {
            case 'RUNNING':
                startBtn.classList.add('active');
                break;
            case 'STOPPED':
                stopBtn.classList.add('active');
                break;
            case 'RELEASED':
                releaseBtn.classList.add('active');
                break;
        }
        
        // Update status indicator
        const statusIndicator = document.getElementById('connection-status');
        const motorStatus = document.getElementById('motor-status');
        
        if (isConnected) {
            statusIndicator.textContent = 'Connected';
            statusIndicator.className = 'status-indicator connected';
            motorStatus.textContent = currentMotorMode;
            motorStatus.className = `motor-status ${currentMotorMode.toLowerCase()}`;
        } else {
            statusIndicator.textContent = 'Disconnected';
            statusIndicator.className = 'status-indicator disconnected';
            motorStatus.textContent = 'Unknown';
            motorStatus.className = 'motor-status unknown';
        }
    }
    
    // Load current motor status from device
    function loadMotorStatus() {
        sendRequest('/api/motor/status')
            .then(response => {
                // Only log connection message when state changes
                if (!isConnected) {
                    isConnected = true;
                    updateStatus('Connected to WebMotor device');
                }
                
                // Update UI with current values from device
                if (response.frequency !== undefined) {
                    frequencySlider.value = response.frequency;
                    frequencyValue.textContent = response.frequency;
                }
                
                if (response.direction !== undefined) {
                    // direction is boolean: true = CW, false = CCW
                    directionToggle.checked = response.direction;
                    directionText.textContent = response.direction ? 'CW' : 'CCW';
                }
                
                if (response.microsteps !== undefined) {
                    document.getElementById('microsteps').value = response.microsteps;
                }
                
                if (response.mode !== undefined && response.mode !== currentMotorMode) {
                    currentMotorMode = response.mode;
                    updateStatus(`Motor mode: ${response.mode}`);
                }
                
                updateButtonStates();
            })
            .catch(error => {
                // Only log disconnect message when state changes
                if (isConnected) {
                    isConnected = false;
                    currentMotorMode = 'UNKNOWN';
                    updateButtonStates();
                    updateStatus(`Connection lost: ${error.message}`);
                }
                // Always log retry attempts when disconnected
                updateStatus(`Cannot connect to device: ${error.message}`);
            });
    }
    
    // Periodic status updates (every 5 seconds)
    setInterval(loadMotorStatus, 5000);
    
    // Initial status load
    loadMotorStatus();
});