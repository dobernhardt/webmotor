// app.js
document.addEventListener('DOMContentLoaded', function() {
    const API_BASE = '';
    
    let isConnected = false;
    let currentMotorMode = 'STOPPED';
    let frequencyTimeout = null;
    
    const frequencySlider = document.getElementById('frequency');
    const frequencyValue = document.getElementById('frequency-value');
    
    frequencySlider.addEventListener('input', function() {
        frequencyValue.textContent = this.value;

        if (frequencyTimeout) clearTimeout(frequencyTimeout);

        frequencyTimeout = setTimeout(() => {
            sendMotorControl('frequency', parseInt(this.value));
        }, 500);
    });

    frequencySlider.addEventListener('mouseup', function() {
        if (frequencyTimeout) {
            clearTimeout(frequencyTimeout);
            frequencyTimeout = null;
        }
        sendMotorControl('frequency', parseInt(this.value));
    });

    frequencySlider.addEventListener('touchend', function() {
        if (frequencyTimeout) {
            clearTimeout(frequencyTimeout);
            frequencyTimeout = null;
        }
        sendMotorControl('frequency', parseInt(this.value));
    });

    const directionToggle = document.getElementById('direction');
    const directionText = document.getElementById('direction-text');

    directionToggle.addEventListener('change', function() {
        const direction = this.checked;
        directionText.textContent = direction ? 'CW' : 'CCW';
        sendMotorControl('direction', direction);
    });

    document.getElementById('microsteps').addEventListener('change', function() {
        sendMotorControl('microsteps', parseInt(this.value));
    });

    document.getElementById('start').addEventListener('click', function() {
        sendMotorControl('mode', 'RUNNING')
            .then(() => {
                currentMotorMode = 'RUNNING';
                updateButtonStates();
                updateStatus('Motor started');
            })
            .catch(e => updateStatus(`Error: ${e.message}`));
    });

    document.getElementById('stop').addEventListener('click', function() {
        sendMotorControl('mode', 'STOPPED')
            .then(() => {
                currentMotorMode = 'STOPPED';
                updateButtonStates();
                updateStatus('Motor stopped');
            })
            .catch(e => updateStatus(`Error: ${e.message}`));
    });

    document.getElementById('release').addEventListener('click', function() {
        sendMotorControl('mode', 'RELEASED')
            .then(() => {
                currentMotorMode = 'RELEASED';
                updateButtonStates();
                updateStatus('Motor released');
            })
            .catch(e => updateStatus(`Error: ${e.message}`));
    });

    document.getElementById('save-wifi').addEventListener('click', function() {
        const ssid = document.getElementById('ssid').value;
        const password = document.getElementById('password').value;

        if (!ssid || !password) {
            updateStatus('Error: missing WiFi data');
            return;
        }

        sendRequest('/api/wifi/config', 'POST', { ssid, password })
            .then(() => {
                updateStatus(`WiFi saved: ${ssid}`);
                document.getElementById('password').value = '';
            })
            .catch(e => updateStatus(`Error: ${e.message}`));
    });

    const cloudEnabledToggle = document.getElementById('cloud-enabled');
    const cloudEnabledText = document.getElementById('cloud-enabled-text');

    cloudEnabledToggle.addEventListener('change', function() {
        cloudEnabledText.textContent = this.checked ? 'Enabled' : 'Disabled';
    });

    document.getElementById('save-cloud').addEventListener('click', function() {
        const endpoint = document.getElementById('cloud-endpoint').value;
        const apiKey = document.getElementById('cloud-api-key').value;
        const enabled = cloudEnabledToggle.checked;

        if (!endpoint) {
            updateStatus('Error: missing endpoint');
            return;
        }

        sendRequest('/api/cloud/config', 'POST', {
            apiEndpoint: endpoint,
            apiKey: apiKey,
            enabled
        })
        .then(() => {
            updateStatus(`Cloud config saved`);
            if (apiKey) document.getElementById('cloud-api-key').value = '';
        })
        .catch(e => updateStatus(`Error: ${e.message}`));
    });

    document.getElementById('test-cloud').addEventListener('click', function() {
        updateStatus('Testing cloud...');
        sendRequest('/api/cloud/test', 'GET')
            .then(r => updateStatus(r.success ? '✓ OK' : '✗ FAIL'))
            .catch(e => updateStatus(`Error: ${e.message}`));
    });

    // ✅ FIX: angepasst an Backend "action + parameters"
    function sendMotorControl(parameter, value) {
        const command = {
            action: "control",
            parameters: {
                [parameter]: value
            }
        };

        return sendRequest('/api/motor/control', 'POST', command)
            .then(() => updateStatus(`${parameter} = ${value}`));
    }

    function sendRequest(endpoint, method = 'GET', data = null) {
        const options = {
            method,
            headers: { 'Content-Type': 'application/json' }
        };

        if (data) options.body = JSON.stringify(data);

        return fetch(API_BASE + endpoint, options)
            .then(res => {
                if (!res.ok) throw new Error(res.statusText);
                return res.json();
            });
    }

    function updateStatus(msg) {
        const el = document.getElementById('status-output');
        const time = new Date().toLocaleTimeString();
        el.textContent += `\n[${time}] ${msg}`;
        el.scrollTop = el.scrollHeight;
    }

    function updateButtonStates() {
        document.querySelectorAll('.btn').forEach(b => b.classList.remove('active'));
    }

    setInterval(() => {
        sendRequest('/api/motor/status')
            .then(r => {
                isConnected = true;
                currentMotorMode = r.mode || currentMotorMode;
                updateButtonStates();
            })
            .catch(() => isConnected = false);
    }, 5000);
});