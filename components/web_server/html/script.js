// Motor control state
let motorState = {
    microsteps: 16,
    frequency: 0,
    direction: true,
    mode: 0
};

// DOM Elements
const elements = {
    microsteps: document.getElementById('microsteps'),
    frequency: document.getElementById('frequency'),
    direction: document.getElementById('direction'),
    btnStart: document.getElementById('btn-start'),
    btnStop: document.getElementById('btn-stop'),
    btnRelease: document.getElementById('btn-release'),
    wifiSsid: document.getElementById('wifi-ssid'),
    wifiPassword: document.getElementById('wifi-password'),
    btnWifiSave: document.getElementById('btn-wifi-save')
};

// API endpoints
const API = {
    MOTOR_STATUS: '/api/motor/status',
    MOTOR_CONTROL: '/api/motor/control',
    WIFI_CONFIG: '/api/wifi/config'
};

// Fetch motor status
async function fetchMotorStatus() {
    try {
        const response = await fetch(API.MOTOR_STATUS);
        const data = await response.json();
        updateUIFromStatus(data);
    } catch (error) {
        console.error('Error fetching motor status:', error);
    }
}

// Update UI elements from motor status
function updateUIFromStatus(status) {
    motorState = status;
    elements.microsteps.value = status.microsteps;
    elements.frequency.value = status.frequency;
    elements.direction.checked = status.direction;
    updateModeButtons(status.mode);
}

// Update mode button states
function updateModeButtons(mode) {
    elements.btnStart.classList.remove('active');
    elements.btnStop.classList.remove('active');
    elements.btnRelease.classList.remove('active');
    
    switch (mode) {
        case 1: // RUNNING
            elements.btnStart.classList.add('active');
            break;
        case 0: // STOPPED
            elements.btnStop.classList.add('active');
            break;
        case 2: // RELEASED
            elements.btnRelease.classList.add('active');
            break;
    }
}

// Send motor control update
async function updateMotorControl(updates) {
    try {
        const response = await fetch(API.MOTOR_CONTROL, {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({
                ...motorState,
                ...updates
            })
        });
        
        if (!response.ok) {
            throw new Error('Failed to update motor control');
        }
        
        await fetchMotorStatus();
    } catch (error) {
        console.error('Error updating motor control:', error);
    }
}

// Save WiFi configuration
async function saveWiFiConfig() {
    const ssid = elements.wifiSsid.value.trim();
    const password = elements.wifiPassword.value;
    
    if (!ssid) {
        alert('Please enter SSID');
        return;
    }
    
    try {
        const response = await fetch(API.WIFI_CONFIG, {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({
                ssid,
                password
            })
        });
        
        const data = await response.json();
        if (data.status === 'ok') {
            alert('WiFi configuration saved. Device will reboot.');
        } else {
            throw new Error(data.message || 'Failed to save WiFi configuration');
        }
    } catch (error) {
        console.error('Error saving WiFi configuration:', error);
        alert(error.message);
    }
}

// Event Listeners
elements.microsteps.addEventListener('change', () => {
    updateMotorControl({ microsteps: parseInt(elements.microsteps.value) });
});

elements.frequency.addEventListener('change', () => {
    updateMotorControl({ frequency: parseInt(elements.frequency.value) });
});

elements.direction.addEventListener('change', () => {
    updateMotorControl({ direction: elements.direction.checked });
});

elements.btnStart.addEventListener('click', () => {
    updateMotorControl({ mode: 1 });
});

elements.btnStop.addEventListener('click', () => {
    updateMotorControl({ mode: 0 });
});

elements.btnRelease.addEventListener('click', () => {
    updateMotorControl({ mode: 2 });
});

elements.btnWifiSave.addEventListener('click', saveWiFiConfig);

// Load initial state
document.addEventListener('DOMContentLoaded', () => {
    fetchMotorStatus();
    // Fetch motor status every second
    setInterval(fetchMotorStatus, 1000);
});