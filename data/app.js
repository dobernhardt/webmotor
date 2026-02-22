// app.js
document.addEventListener('DOMContentLoaded', function() {
    // API base URL (will be the device's IP when served from ATOM S3)
    const API_BASE = '';
    
    // Connection state tracking
    let isConnected = false;
    let currentMotorMode = 'STOPPED';
    let frequencyTimeout = null;
    
    // Frequency slider handling with debouncing
    const frequencySlider = document.getElementById('frequency');
    const frequencyValue = document.getElementById('frequency-value');
    
    frequencySlider.addEventListener('input', function() {
        // Update display immediately for responsive UI
        frequencyValue.textContent = this.value;
        
        // Clear existing timeout
        if (frequencyTimeout) {
            clearTimeout(frequencyTimeout);
        }
        
        // Set new timeout for API call (500ms delay)
        frequencyTimeout = setTimeout(() => {
            sendMotorControl('frequency', parseInt(this.value));
        }, 500);
    });
    
    // Also send on mouseup for immediate response when user releases slider
    frequencySlider.addEventListener('mouseup', function() {
        if (frequencyTimeout) {
            clearTimeout(frequencyTimeout);
            frequencyTimeout = null;
        }
        sendMotorControl('frequency', parseInt(this.value));
    });
    
    // Handle touch events for mobile devices
    frequencySlider.addEventListener('touchend', function() {
        if (frequencyTimeout) {
            clearTimeout(frequencyTimeout);
            frequencyTimeout = null;
        }
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
    
    // Cloud configuration toggle
    const cloudEnabledToggle = document.getElementById('cloud-enabled');
    const cloudEnabledText = document.getElementById('cloud-enabled-text');
    
    cloudEnabledToggle.addEventListener('change', function() {
        cloudEnabledText.textContent = this.checked ? 'Enabled' : 'Disabled';
    });
    
    // Cloud configuration
    document.getElementById('save-cloud').addEventListener('click', function() {
        const endpoint = document.getElementById('cloud-endpoint').value;
        const apiKey = document.getElementById('cloud-api-key').value;
        const enabled = cloudEnabledToggle.checked;
        
        if (endpoint) {
            const cloudConfig = {
                apiEndpoint: endpoint,
                apiKey: apiKey,  // Empty string means "keep existing key"
                enabled: enabled
            };
            
            sendRequest('/api/cloud/config', 'POST', cloudConfig)
                .then(() => {
                    updateStatus(`Cloud config saved. Sync ${enabled ? 'enabled' : 'disabled'}`);
                    // Only clear API key field if it was provided
                    if (apiKey) {
                        document.getElementById('cloud-api-key').value = '';
                    }
                    // Reload cloud status to confirm the saved state
                    setTimeout(() => loadCloudStatus(), 500);
                })
                .catch(error => updateStatus(`Error saving cloud config: ${error.message}`));
        } else {
            updateStatus('Error: Please enter endpoint');
        }
    });
    
    // Test cloud connection
    document.getElementById('test-cloud').addEventListener('click', function() {
        updateStatus('Testing cloud connection...');
        
        sendRequest('/api/cloud/test', 'GET')
            .then(response => {
                if (response.success) {
                    updateStatus(`✓ ${response.message}`);
                } else {
                    updateStatus(`✗ ${response.message}`);
                }
            })
            .catch(error => updateStatus(`Error testing cloud: ${error.message}`));
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
        // Load motor status
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
            });
        
        // Load WiFi status
        loadWiFiStatus();
        
        // Load cloud status
        loadCloudStatus();
    }
    
    // Load WiFi status from device
    function loadWiFiStatus() {
        sendRequest('/api/wifi/status')
            .then(response => {
                updateWiFiStatus(response);
            })
            .catch(error => {
                // WiFi status is less critical, just log without updating connection state
                console.log('WiFi status request failed:', error);
            });
    }
    
    // Load cloud status from device
    function loadCloudStatus() {
        sendRequest('/api/cloud/status')
            .then(response => {
                // Update UI with cloud config
                if (response.apiEndpoint) {
                    document.getElementById('cloud-endpoint').value = response.apiEndpoint;
                }
                if (response.enabled !== undefined) {
                    cloudEnabledToggle.checked = response.enabled;
                    cloudEnabledText.textContent = response.enabled ? 'Enabled' : 'Disabled';
                }
            })
            .catch(error => {
                console.log('Cloud status request failed:', error);
            });
    }
    
    // Update WiFi status display
    function updateWiFiStatus(wifiInfo) {
        const wifiStatus = document.getElementById('wifi-status');
        const networkInfo = document.getElementById('network-info');
        
        if (wifiInfo.isAccessPoint) {
            wifiStatus.textContent = 'Access Point';
            wifiStatus.className = 'status-indicator ap-mode';
            networkInfo.textContent = `${wifiInfo.ssid} (${wifiInfo.ipAddress})`;
        } else if (wifiInfo.isConnected) {
            wifiStatus.textContent = 'Connected';
            wifiStatus.className = 'status-indicator connected';
            networkInfo.textContent = `${wifiInfo.ssid} (${wifiInfo.ipAddress})`;
        } else {
            wifiStatus.textContent = 'Disconnected';
            wifiStatus.className = 'status-indicator disconnected';
            networkInfo.textContent = '-';
        }
    }
    
    // Periodic status updates (every 5 seconds)
    setInterval(loadMotorStatus, 5000);
    
    // Initial status load
    loadMotorStatus();
});