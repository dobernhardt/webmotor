// cloud-frontend/app.js
// WebMotor Cloud Control Frontend

document.addEventListener('DOMContentLoaded', function() {
    // Cloud configuration
    const apiEndpoint = '/api'; // Hardcoded since frontend and API are deployed together
    let apiKey = '';
    let isConfigured = false;
    let isConnected = false;
    let currentMotorState = null;
    let frequencyTimeout = null;
    let statePollingInterval = null;
    
    // Load saved configuration from localStorage
    loadConfiguration();
    
    // Configuration handlers
    document.getElementById('save-config').addEventListener('click', saveConfiguration);
    document.getElementById('test-connection').addEventListener('click', testConnection);
    
    // Frequency slider handling with debouncing
    const frequencySlider = document.getElementById('frequency');
    const frequencyValue = document.getElementById('frequency-value');
    
    frequencySlider.addEventListener('input', function() {
        frequencyValue.textContent = this.value;
        
        if (frequencyTimeout) {
            clearTimeout(frequencyTimeout);
        }
        
        frequencyTimeout = setTimeout(() => {
            sendMotorCommand('frequency', parseInt(this.value));
        }, 500);
    });
    
    frequencySlider.addEventListener('mouseup', function() {
        if (frequencyTimeout) {
            clearTimeout(frequencyTimeout);
            frequencyTimeout = null;
        }
        sendMotorCommand('frequency', parseInt(this.value));
    });
    
    frequencySlider.addEventListener('touchend', function() {
        if (frequencyTimeout) {
            clearTimeout(frequencyTimeout);
            frequencyTimeout = null;
        }
        sendMotorCommand('frequency', parseInt(this.value));
    });
    
    // Direction toggle handling
    const directionToggle = document.getElementById('direction');
    const directionText = document.getElementById('direction-text');
    
    directionToggle.addEventListener('change', function() {
        const direction = this.checked;
        directionText.textContent = direction ? 'CW' : 'CCW';
        sendMotorCommand('direction', direction);
    });
    
    // Microstepping handling
    document.getElementById('microsteps').addEventListener('change', function() {
        sendMotorCommand('microsteps', parseInt(this.value));
    });
    
    // Motor control button handlers
    document.getElementById('start').addEventListener('click', function() {
        sendMotorCommand('mode', 'RUNNING');
    });
    
    document.getElementById('stop').addEventListener('click', function() {
        sendMotorCommand('mode', 'STOPPED');
    });
    
    document.getElementById('release').addEventListener('click', function() {
        sendMotorCommand('mode', 'RELEASED');
    });
    
    // Load configuration from localStorage
    function loadConfiguration() {
        apiKey = localStorage.getItem('webmotor_api_key') || '';
        
        if (apiKey) {
            document.getElementById('api-key').value = apiKey;
            isConfigured = true;
            updateStatus('Configuration loaded from storage');
            startStatePolling();
        }
    }
    
    // Save configuration to localStorage
    function saveConfiguration() {
        const key = document.getElementById('api-key').value.trim();
        
        if (!key) {
            updateStatus('Error: Please enter API key');
            return;
        }
        
        apiKey = key;
        
        localStorage.setItem('webmotor_api_key', apiKey);
        
        isConfigured = true;
        updateStatus('Configuration saved successfully');
        
        // Start polling for state
        startStatePolling();
    }
    
    // Test connection to cloud backend
    async function testConnection() {
        if (!isConfigured) {
            updateStatus('Error: Please save configuration first');
            return;
        }
        
        updateStatus('Testing connection to Azure Function...');
        
        try {
            const response = await fetch(`${apiEndpoint}/health`);
            if (response.ok) {
                const data = await response.json();
                updateStatus(`✓ Connection successful! Backend is healthy.`);
                isConnected = true;
                updateConnectionStatus();
            } else {
                throw new Error(`HTTP ${response.status}`);
            }
        } catch (error) {
            updateStatus(`✗ Connection failed: ${error.message}`);
            isConnected = false;
            updateConnectionStatus();
        }
    }
    
    // Send motor command to cloud backend
    async function sendMotorCommand(parameter, value) {
        if (!isConfigured) {
            updateStatus('Error: Please configure cloud settings first');
            return;
        }
        
        const command = {
            action: 'control',
            parameters: {
                [parameter]: value
            }
        };
        
        try {
            const response = await fetch(`${apiEndpoint}/commands`, {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                    'X-API-Key': apiKey
                },
                body: JSON.stringify(command)
            });
            
            if (!response.ok) {
                throw new Error(`HTTP ${response.status}: ${response.statusText}`);
            }
            
            const data = await response.json();
            updateStatus(`✓ Command sent: ${parameter} = ${value}`);
            
        } catch (error) {
            updateStatus(`✗ Error sending command: ${error.message}`);
            isConnected = false;
            updateConnectionStatus();
        }
    }
    
    // Poll for motor state from cloud backend
    async function pollMotorState() {
        if (!isConfigured) {
            return;
        }
        
        try {
            const response = await fetch(`${apiEndpoint}/state`, {
                headers: {
                    'X-API-Key': apiKey
                }
            });
            
            if (!response.ok) {
                throw new Error(`HTTP ${response.status}`);
            }
            
            const data = await response.json();
            
            if (data.state) {
                // Update connection status
                if (!isConnected) {
                    isConnected = true;
                    updateStatus('✓ Connected to cloud backend');
                }
                
                // Update motor state
                updateMotorState(data.state);
            } else {
                // No state available yet
                if (isConnected) {
                    updateStatus('Waiting for controller to report state...');
                }
            }
            
            updateConnectionStatus();
            
        } catch (error) {
            if (isConnected) {
                isConnected = false;
                updateStatus(`✗ Connection lost: ${error.message}`);
                updateConnectionStatus();
            }
        }
    }
    
    // Update motor state display
    function updateMotorState(state) {
        currentMotorState = state;
        
        // Update motor status
        const motorStatus = document.getElementById('motor-status');
        if (state.mode) {
            motorStatus.textContent = state.mode;
            motorStatus.className = `motor-status ${state.mode.toLowerCase()}`;
        }
        
        // Update position if available
        const positionValue = document.getElementById('position-value');
        if (state.position !== undefined) {
            positionValue.textContent = state.position;
        }
        
        // Update UI controls from state (if not currently being changed by user)
        if (state.frequency !== undefined && !frequencyTimeout) {
            frequencySlider.value = state.frequency;
            frequencyValue.textContent = state.frequency;
        }
        
        if (state.direction !== undefined) {
            directionToggle.checked = state.direction;
            directionText.textContent = state.direction ? 'CW' : 'CCW';
        }
        
        if (state.microsteps !== undefined) {
            document.getElementById('microsteps').value = state.microsteps;
        }
        
        // Update last update timestamp
        const lastUpdate = document.getElementById('last-update');
        if (state.timestamp) {
            const time = new Date(state.timestamp);
            lastUpdate.textContent = time.toLocaleTimeString();
        } else {
            lastUpdate.textContent = new Date().toLocaleTimeString();
        }
        
        updateButtonStates();
    }
    
    // Update connection status display
    function updateConnectionStatus() {
        const statusIndicator = document.getElementById('connection-status');
        
        if (isConnected) {
            statusIndicator.textContent = 'Connected';
            statusIndicator.className = 'status-indicator connected';
        } else {
            statusIndicator.textContent = 'Disconnected';
            statusIndicator.className = 'status-indicator disconnected';
        }
    }
    
    // Update button visual states based on motor mode
    function updateButtonStates() {
        const startBtn = document.getElementById('start');
        const stopBtn = document.getElementById('stop');
        const releaseBtn = document.getElementById('release');
        
        startBtn.classList.remove('active');
        stopBtn.classList.remove('active');
        releaseBtn.classList.remove('active');
        
        if (currentMotorState && currentMotorState.mode) {
            switch(currentMotorState.mode) {
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
        }
    }
    
    // Start polling for state updates
    function startStatePolling() {
        // Clear any existing interval
        if (statePollingInterval) {
            clearInterval(statePollingInterval);
        }
        
        // Poll immediately
        pollMotorState();
        
        // Then poll every 2 seconds
        statePollingInterval = setInterval(pollMotorState, 2000);
    }
    
    // Status update function with log limit
    function updateStatus(message) {
        const statusOutput = document.getElementById('status-output');
        const timestamp = new Date().toLocaleTimeString();
        const newLine = statusOutput.textContent.trim() && 
                        !statusOutput.textContent.includes('Configure cloud settings') ? '\n' : '';
        
        if (statusOutput.textContent.includes('Configure cloud settings')) {
            statusOutput.textContent = '';
        }
        
        statusOutput.textContent += `${newLine}[${timestamp}] ${message}`;
        
        // Limit log entries to prevent infinite growth
        const lines = statusOutput.textContent.split('\n');
        if (lines.length > 50) {
            statusOutput.textContent = lines.slice(-50).join('\n');
        }
        
        statusOutput.scrollTop = statusOutput.scrollHeight;
    }
});
