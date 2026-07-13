// app.js - local test interface
document.addEventListener('DOMContentLoaded', function() {
    const API_BASE = '';
    const SEND_INTERVAL_MS = 100; // ~10 Hz while the joystick is engaged

    let isConnected = false;

    // =========================
    // Joystick
    // =========================
    const joystick = document.getElementById('joystick');
    const knob = document.getElementById('joystick-knob');
    const joyXDisplay = document.getElementById('joy-x');
    const joyYDisplay = document.getElementById('joy-y');

    let joyX = 0;
    let joyY = 0;
    let engaged = false;
    let releaseSendsLeft = 0; // send {0,0} a few times after release

    function setKnob(x, y) {
        const radius = joystick.clientWidth / 2;
        const knobRadius = knob.clientWidth / 2;
        const maxDist = radius - knobRadius;
        knob.style.transform =
            `translate(calc(-50% + ${x * maxDist}px), calc(-50% + ${-y * maxDist}px))`;
    }

    function updateFromPointer(event) {
        const rect = joystick.getBoundingClientRect();
        const cx = rect.left + rect.width / 2;
        const cy = rect.top + rect.height / 2;
        const maxDist = rect.width / 2 - knob.clientWidth / 2;

        let dx = (event.clientX - cx) / maxDist;
        let dy = (event.clientY - cy) / maxDist;

        // Clamp to the unit circle
        const dist = Math.hypot(dx, dy);
        if (dist > 1) {
            dx /= dist;
            dy /= dist;
        }

        joyX = dx;
        joyY = -dy; // screen y grows downwards, throttle grows upwards
        setKnob(joyX, joyY);
        updateJoystickDisplay();
    }

    function releaseJoystick() {
        engaged = false;
        joyX = 0;
        joyY = 0;
        releaseSendsLeft = 3;
        setKnob(0, 0);
        updateJoystickDisplay();
    }

    function updateJoystickDisplay() {
        joyXDisplay.textContent = joyX.toFixed(2);
        joyYDisplay.textContent = joyY.toFixed(2);
    }

    joystick.addEventListener('pointerdown', function(event) {
        joystick.setPointerCapture(event.pointerId);
        engaged = true;
        updateFromPointer(event);
        event.preventDefault();
    });

    joystick.addEventListener('pointermove', function(event) {
        if (!engaged) return;
        updateFromPointer(event);
        event.preventDefault();
    });

    joystick.addEventListener('pointerup', releaseJoystick);
    joystick.addEventListener('pointercancel', releaseJoystick);

    // Send loop: while engaged post the current target; after release post
    // {0,0} a few times so the ESP reliably stops even if one packet drops
    setInterval(() => {
        if (engaged) {
            sendDriveTarget(joyX, joyY);
        } else if (releaseSendsLeft > 0) {
            releaseSendsLeft--;
            sendDriveTarget(0, 0);
        }
    }, SEND_INTERVAL_MS);

    function sendDriveTarget(x, y) {
        fetch(API_BASE + '/api/drive', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ x, y })
        }).catch(() => {});
    }

    // =========================
    // Emergency stop
    // =========================
    document.getElementById('stop').addEventListener('click', function() {
        releaseJoystick();
        sendRequest('/api/drive/stop', 'POST', {})
            .then(() => updateStatus('NOT-AUS ausgeführt'))
            .catch(e => updateStatus(`Fehler: ${e.message}`));
    });

    // =========================
    // Drive configuration
    // =========================
    const steerLimitSlider = document.getElementById('steer-limit');
    const steerLimitValue = document.getElementById('steer-limit-value');
    const maxSpeedSlider = document.getElementById('max-speed');
    const maxSpeedValue = document.getElementById('max-speed-value');

    steerLimitSlider.addEventListener('input', function() {
        steerLimitValue.textContent = this.value;
    });

    maxSpeedSlider.addEventListener('input', function() {
        maxSpeedValue.textContent = this.value;
    });

    function loadDriveConfig() {
        sendRequest('/api/drive/config')
            .then(cfg => {
                if (cfg.steerLimitDeg !== undefined) {
                    steerLimitSlider.value = Math.round(cfg.steerLimitDeg);
                    steerLimitValue.textContent = Math.round(cfg.steerLimitDeg);
                }
                if (cfg.maxFrequency !== undefined) {
                    maxSpeedSlider.value = cfg.maxFrequency;
                    maxSpeedValue.textContent = cfg.maxFrequency;
                }
            })
            .catch(() => {});
    }

    document.getElementById('save-drive-config').addEventListener('click', function() {
        sendRequest('/api/drive/config', 'POST', {
            steerLimitDeg: parseFloat(steerLimitSlider.value),
            maxFrequency: parseInt(maxSpeedSlider.value)
        })
        .then(() => updateStatus(`Fahrwerk gespeichert: ±${steerLimitSlider.value}°, max ${maxSpeedSlider.value} Schritte/s`))
        .catch(e => updateStatus(`Fehler: ${e.message}`));
    });

    document.getElementById('center-steering').addEventListener('click', function() {
        sendRequest('/api/drive/center', 'POST', {})
            .then(() => updateStatus('Lenkung zentriert (aktuelle Stellung = geradeaus)'))
            .catch(e => updateStatus(`Fehler: ${e.message}`));
    });

    // =========================
    // WiFi configuration
    // =========================
    document.getElementById('save-wifi').addEventListener('click', function() {
        const ssid = document.getElementById('ssid').value;
        const password = document.getElementById('password').value;

        if (!ssid || !password) {
            updateStatus('Fehler: WLAN-Daten fehlen');
            return;
        }

        sendRequest('/api/wifi/config', 'POST', { ssid, password })
            .then(() => {
                updateStatus(`WLAN gespeichert: ${ssid}`);
                document.getElementById('password').value = '';
            })
            .catch(e => updateStatus(`Fehler: ${e.message}`));
    });

    // =========================
    // Cloud configuration
    // =========================
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
            updateStatus('Fehler: Endpoint fehlt');
            return;
        }

        sendRequest('/api/cloud/config', 'POST', {
            apiEndpoint: endpoint,
            apiKey: apiKey,
            enabled
        })
        .then(() => {
            updateStatus('Cloud-Konfiguration gespeichert');
            if (apiKey) document.getElementById('cloud-api-key').value = '';
        })
        .catch(e => updateStatus(`Fehler: ${e.message}`));
    });

    document.getElementById('test-cloud').addEventListener('click', function() {
        updateStatus('Teste Cloud-Verbindung...');
        sendRequest('/api/cloud/test', 'GET')
            .then(r => updateStatus(r.success ? '✓ Cloud OK' : '✗ Cloud FAIL'))
            .catch(e => updateStatus(`Fehler: ${e.message}`));
    });

    // =========================
    // Status polling
    // =========================
    function pollDriveStatus() {
        sendRequest('/api/drive/status')
            .then(status => {
                setConnected(true);

                const driveStatus = document.getElementById('drive-status');
                if (status.failsafe) {
                    driveStatus.textContent = 'Failsafe';
                    driveStatus.className = 'motor-status stopped';
                } else if (status.driving) {
                    driveStatus.textContent = 'Fährt';
                    driveStatus.className = 'motor-status running';
                } else {
                    driveStatus.textContent = 'Steht';
                    driveStatus.className = 'motor-status unknown';
                }

                document.getElementById('steering-value').textContent =
                    `${status.steeringDeg.toFixed(1)}° / ±${status.steerLimitDeg.toFixed(0)}°`;
            })
            .catch(() => setConnected(false));
    }

    function pollWifiStatus() {
        sendRequest('/api/wifi/status')
            .then(info => {
                const text = info.isAccessPoint
                    ? `AP: ${info.ssid} (${info.ipAddress})`
                    : (info.isConnected ? `${info.ssid} (${info.ipAddress})` : 'nicht verbunden');
                document.getElementById('network-info').textContent = text;
            })
            .catch(() => {});
    }

    function setConnected(connected) {
        if (connected === isConnected) return;
        isConnected = connected;
        const el = document.getElementById('connection-status');
        el.textContent = connected ? 'Verbunden' : 'Getrennt';
        el.className = `status-indicator ${connected ? 'connected' : 'disconnected'}`;
    }

    setInterval(pollDriveStatus, 2000);
    setInterval(pollWifiStatus, 10000);
    pollDriveStatus();
    pollWifiStatus();
    loadDriveConfig();

    // =========================
    // Helpers
    // =========================
    function sendRequest(endpoint, method = 'GET', data = null) {
        const options = {
            method,
            headers: { 'Content-Type': 'application/json' }
        };

        if (data) options.body = JSON.stringify(data);

        return fetch(API_BASE + endpoint, options)
            .then(res => {
                if (!res.ok) throw new Error(res.statusText || `HTTP ${res.status}`);
                return res.json();
            });
    }

    function updateStatus(msg) {
        const el = document.getElementById('status-output');
        const time = new Date().toLocaleTimeString();
        el.textContent += `\n[${time}] ${msg}`;

        const lines = el.textContent.split('\n');
        if (lines.length > 50) {
            el.textContent = lines.slice(-50).join('\n');
        }

        el.scrollTop = el.scrollHeight;
    }
});
