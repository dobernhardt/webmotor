// app.js - local test interface
document.addEventListener('DOMContentLoaded', function() {
    const API_BASE = '';
    const SEND_INTERVAL_MS = 100; // ~10 Hz while the joystick is engaged

    let isConnected = false;

    // Axis limits (degrees) - mirrored from the controller config
    let rotationLimitDeg = 45;
    let tiltLimitDeg = 30;

    // =========================
    // Joystick
    // =========================
    // Sticky: the knob does not spring back to center. While idle it mirrors
    // the actual axis angles reported by the controller; full deflection
    // corresponds to the configured axis limit.
    const joystick = document.getElementById('joystick');
    const knob = document.getElementById('joystick-knob');
    const joyXDisplay = document.getElementById('joy-x');
    const joyYDisplay = document.getElementById('joy-y');

    let joyX = 0; // normalized [-1..1], x = rotation around z
    let joyY = 0; // normalized [-1..1], y = tilt around x
    let engaged = false;
    let releaseSendsLeft = 0; // re-send the held target a few times after release

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
        joyY = -dy; // screen y grows downwards, tilt grows upwards
        setKnob(joyX, joyY);
        updateJoystickDisplay();
    }

    function releaseJoystick() {
        if (!engaged) return;
        engaged = false;
        // Sticky joystick: keep the value, just make sure the final target
        // reliably reaches the controller
        releaseSendsLeft = 3;
    }

    function formatDeg(deg) {
        return `${deg >= 0 ? '+' : ''}${deg.toFixed(1)}°`;
    }

    function updateJoystickDisplay() {
        joyXDisplay.textContent = formatDeg(joyX * rotationLimitDeg);
        joyYDisplay.textContent = formatDeg(joyY * tiltLimitDeg);
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

    // Send loop: while engaged post the current target; after release re-send
    // the held target a few times so it reliably arrives
    setInterval(() => {
        if (engaged) {
            sendTarget(joyX, joyY);
        } else if (releaseSendsLeft > 0) {
            releaseSendsLeft--;
            sendTarget(joyX, joyY);
        }
    }, SEND_INTERVAL_MS);

    function sendTarget(x, y) {
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
        // Freeze: stop sending targets, the controller freezes both axes
        engaged = false;
        releaseSendsLeft = 0;
        sendRequest('/api/drive/stop', 'POST', {})
            .then(() => updateStatus('NOT-AUS ausgeführt (Achsen eingefroren)'))
            .catch(e => updateStatus(`Fehler: ${e.message}`));
    });

    // =========================
    // Axis limits
    // =========================
    const rotationLimitSlider = document.getElementById('rotation-limit');
    const rotationLimitValue = document.getElementById('rotation-limit-value');
    const tiltLimitSlider = document.getElementById('tilt-limit');
    const tiltLimitValue = document.getElementById('tilt-limit-value');

    rotationLimitSlider.addEventListener('input', function() {
        rotationLimitValue.textContent = this.value;
    });

    tiltLimitSlider.addEventListener('input', function() {
        tiltLimitValue.textContent = this.value;
    });

    function loadLimits() {
        sendRequest('/api/drive/config')
            .then(cfg => {
                if (cfg.rotationLimitDeg !== undefined) {
                    rotationLimitDeg = Number(cfg.rotationLimitDeg);
                    rotationLimitSlider.value = Math.round(rotationLimitDeg);
                    rotationLimitValue.textContent = Math.round(rotationLimitDeg);
                }
                if (cfg.tiltLimitDeg !== undefined) {
                    tiltLimitDeg = Number(cfg.tiltLimitDeg);
                    tiltLimitSlider.value = Math.round(tiltLimitDeg);
                    tiltLimitValue.textContent = Math.round(tiltLimitDeg);
                }
                updateJoystickDisplay();
            })
            .catch(() => {});
    }

    document.getElementById('save-limits').addEventListener('click', function() {
        sendRequest('/api/drive/config', 'POST', {
            rotationLimitDeg: parseFloat(rotationLimitSlider.value),
            tiltLimitDeg: parseFloat(tiltLimitSlider.value)
        })
        .then(() => {
            rotationLimitDeg = parseFloat(rotationLimitSlider.value);
            tiltLimitDeg = parseFloat(tiltLimitSlider.value);
            updateJoystickDisplay();
            updateStatus(`Limits gespeichert: Drehung ±${rotationLimitSlider.value}°, Neigung ±${tiltLimitSlider.value}°`);
        })
        .catch(e => updateStatus(`Fehler: ${e.message}`));
    });

    document.getElementById('center-axes').addEventListener('click', function() {
        sendRequest('/api/drive/center', 'POST', {})
            .then(() => {
                joyX = 0;
                joyY = 0;
                setKnob(0, 0);
                updateJoystickDisplay();
                updateStatus('Achsen zentriert (aktuelle Stellung = 0°)');
            })
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
    function pollPlatformStatus() {
        sendRequest('/api/drive/status')
            .then(status => {
                setConnected(true);

                const motionStatus = document.getElementById('motion-status');
                if (status.moving) {
                    motionStatus.textContent = 'Bewegt';
                    motionStatus.className = 'motor-status running';
                } else {
                    motionStatus.textContent = 'Steht';
                    motionStatus.className = 'motor-status unknown';
                }

                document.getElementById('rotation-value').textContent =
                    `${formatDeg(status.rotationDeg)} / ±${status.rotationLimitDeg.toFixed(0)}°`;
                document.getElementById('tilt-value').textContent =
                    `${formatDeg(status.tiltDeg)} / ±${status.tiltLimitDeg.toFixed(0)}°`;

                // While idle, mirror the actual controller angles on the knob
                if (!engaged && releaseSendsLeft === 0) {
                    const rotLimit = status.rotationLimitDeg > 0 ? status.rotationLimitDeg : rotationLimitDeg;
                    const tiltLimit = status.tiltLimitDeg > 0 ? status.tiltLimitDeg : tiltLimitDeg;
                    joyX = Math.max(-1, Math.min(1, rotLimit > 0 ? status.rotationDeg / rotLimit : 0));
                    joyY = Math.max(-1, Math.min(1, tiltLimit > 0 ? status.tiltDeg / tiltLimit : 0));
                    setKnob(joyX, joyY);
                    updateJoystickDisplay();
                }
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

    setInterval(pollPlatformStatus, 2000);
    setInterval(pollWifiStatus, 10000);
    pollPlatformStatus();
    pollWifiStatus();
    loadLimits();

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
