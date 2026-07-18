// cloud-frontend/app.js
// WebMotor Cloud Control Frontend

document.addEventListener('DOMContentLoaded', function() {
    // Cloud configuration
    const apiEndpoint = '/api'; // Hardcoded since frontend and API are deployed together
    const SEND_INTERVAL_MS = 100; // ~10 Hz while the joystick is engaged

    let apiKey = '';
    let isConfigured = false;
    let isConnected = false;
    let statePollingInterval = null;
    let deviceStatusPollingInterval = null;

    // Axis limits (degrees). The WebUI owns these values; they are loaded
    // from the cloud config and pushed to the ESP via /drive/config.
    let rotationLimitDeg = 45;
    let tiltLimitDeg = 30;

    // Load saved configuration from localStorage
    loadConfiguration();

    // Load version information
    loadVersionInfo();

    // Configuration handlers
    document.getElementById('save-config').addEventListener('click', saveConfiguration);
    document.getElementById('test-connection').addEventListener('click', testConnection);

    // Collapsible axis limits card - remember the user's choice
    const limitsCard = document.getElementById('limits-card');
    limitsCard.open = localStorage.getItem('webmotor_limits_open') !== 'false';
    limitsCard.addEventListener('toggle', function() {
        localStorage.setItem('webmotor_limits_open', limitsCard.open);
    });

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
        // reliably reaches the backend
        releaseSendsLeft = 3;
    }

    function formatDeg(deg) {
        return `${deg >= 0 ? '+' : ''}${deg.toFixed(1)}°`;
    }

    function updateJoystickDisplay() {
        joyXDisplay.textContent = formatDeg(joyX * rotationLimitDeg);
        joyYDisplay.textContent = formatDeg(joyY * tiltLimitDeg);
    }

    // While idle, mirror the actual controller angles on the knob
    function syncJoystickFromState(state) {
        if (engaged || releaseSendsLeft > 0) return;
        if (state.rotationDeg === undefined || state.tiltDeg === undefined) return;

        const rotLimit = Number(state.rotationLimitDeg) > 0
            ? Number(state.rotationLimitDeg) : rotationLimitDeg;
        const tiltLimit = Number(state.tiltLimitDeg) > 0
            ? Number(state.tiltLimitDeg) : tiltLimitDeg;

        joyX = Math.max(-1, Math.min(1, rotLimit > 0 ? Number(state.rotationDeg) / rotLimit : 0));
        joyY = Math.max(-1, Math.min(1, tiltLimit > 0 ? Number(state.tiltDeg) / tiltLimit : 0));
        setKnob(joyX, joyY);
        updateJoystickDisplay();
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
    // the held target a few times so it reliably arrives. The backend stores
    // only the latest value, the ESP ignores targets older than 2 s.
    setInterval(() => {
        if (!isConfigured) return;
        if (engaged) {
            sendTarget(joyX, joyY);
        } else if (releaseSendsLeft > 0) {
            releaseSendsLeft--;
            sendTarget(joyX, joyY);
        }
    }, SEND_INTERVAL_MS);

    async function sendTarget(x, y) {
        try {
            await fetch(`${apiEndpoint}/drive`, {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                    'X-API-Key': apiKey
                },
                body: JSON.stringify({ x, y })
            });
        } catch (error) {
            // Joystick posts are fire-and-forget; the state poll reports errors
        }
    }

    // =========================
    // Emergency stop
    // =========================
    document.getElementById('stop').addEventListener('click', async function() {
        // Freeze: stop sending targets, the ESP freezes both axes in place.
        // The knob re-syncs to the frozen position via the state poll.
        engaged = false;
        releaseSendsLeft = 0;
        try {
            await apiRequest('/commands', 'POST', { action: 'stop' });
            updateStatus('🛑 NOT-AUS gesendet (Achsen eingefroren)');
        } catch (error) {
            updateStatus(`✗ NOT-AUS fehlgeschlagen: ${error.message}`);
        }
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

    async function loadLimits() {
        if (!isConfigured) return;
        try {
            const data = await apiRequest('/drive/config');
            if (data.config) {
                if (data.config.rotationLimitDeg !== undefined && data.config.rotationLimitDeg !== null) {
                    rotationLimitDeg = Number(data.config.rotationLimitDeg);
                    rotationLimitSlider.value = Math.round(rotationLimitDeg);
                    rotationLimitValue.textContent = Math.round(rotationLimitDeg);
                }
                if (data.config.tiltLimitDeg !== undefined && data.config.tiltLimitDeg !== null) {
                    tiltLimitDeg = Number(data.config.tiltLimitDeg);
                    tiltLimitSlider.value = Math.round(tiltLimitDeg);
                    tiltLimitValue.textContent = Math.round(tiltLimitDeg);
                }
                updateJoystickDisplay();
            }
        } catch (error) {
            // Limits not set yet - keep defaults
        }
    }

    document.getElementById('save-limits').addEventListener('click', async function() {
        try {
            await apiRequest('/drive/config', 'POST', {
                rotationLimitDeg: parseFloat(rotationLimitSlider.value),
                tiltLimitDeg: parseFloat(tiltLimitSlider.value)
            });
            rotationLimitDeg = parseFloat(rotationLimitSlider.value);
            tiltLimitDeg = parseFloat(tiltLimitSlider.value);
            updateJoystickDisplay();
            updateStatus(`✓ Limits gespeichert: Drehung ±${rotationLimitSlider.value}°, Neigung ±${tiltLimitSlider.value}°`);
        } catch (error) {
            updateStatus(`✗ Fehler beim Speichern: ${error.message}`);
        }
    });

    document.getElementById('center-axes').addEventListener('click', async function() {
        try {
            await apiRequest('/commands', 'POST', { action: 'center' });
            // Optimistically re-center the knob; the state poll confirms
            joyX = 0;
            joyY = 0;
            setKnob(0, 0);
            updateJoystickDisplay();
            updateStatus('✓ Achsen zentriert (aktuelle Stellung = 0°)');
        } catch (error) {
            updateStatus(`✗ Fehler: ${error.message}`);
        }
    });

    // =========================
    // Configuration handling
    // =========================
    function loadConfiguration() {
        apiKey = localStorage.getItem('webmotor_api_key') || '';

        if (apiKey) {
            document.getElementById('api-key').value = apiKey;
            isConfigured = true;
            updateStatus('Configuration loaded from storage');
            // Collapse the cloud config card - it is already set up
            document.getElementById('cloud-config-card').open = false;
            startStatePolling();
            startDeviceStatusPolling();
            loadLimits();
        }
    }

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
        document.getElementById('cloud-config-card').open = false;

        startStatePolling();
        startDeviceStatusPolling();
        loadLimits();
    }

    async function testConnection() {
        if (!isConfigured) {
            updateStatus('Error: Please save configuration first');
            return;
        }

        updateStatus('Testing connection to Azure Function...');

        try {
            const response = await fetch(`${apiEndpoint}/health`);
            if (response.ok) {
                updateStatus('✓ Connection successful! Backend is healthy.');
                isConnected = true;
            } else {
                throw new Error(`HTTP ${response.status}`);
            }
        } catch (error) {
            updateStatus(`✗ Connection failed: ${error.message}`);
            isConnected = false;
        }
        updateConnectionStatus();
    }

    // =========================
    // State polling
    // =========================
    async function pollRobotState() {
        if (!isConfigured) return;

        try {
            const data = await apiRequest('/state');

            if (!isConnected) {
                isConnected = true;
                updateStatus('✓ Connected to cloud backend');
            }

            if (data.state) {
                updateRobotState(data.state);
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

    function updateRobotState(state) {
        const motionStatus = document.getElementById('motion-status');
        if (state.moving !== undefined) {
            motionStatus.textContent = state.moving ? 'Bewegt' : 'Steht';
            motionStatus.className = state.moving ? 'motor-status running' : 'motor-status unknown';
        }

        const rotationValue = document.getElementById('rotation-value');
        if (state.rotationDeg !== undefined && state.rotationLimitDeg !== undefined) {
            rotationValue.textContent =
                `${formatDeg(Number(state.rotationDeg))} / ±${Number(state.rotationLimitDeg).toFixed(0)}°`;
        }

        const tiltValue = document.getElementById('tilt-value');
        if (state.tiltDeg !== undefined && state.tiltLimitDeg !== undefined) {
            tiltValue.textContent =
                `${formatDeg(Number(state.tiltDeg))} / ±${Number(state.tiltLimitDeg).toFixed(0)}°`;
        }

        const lastUpdate = document.getElementById('last-update');
        if (state.timestamp) {
            const time = new Date(state.timestamp);
            lastUpdate.textContent = time.toLocaleTimeString();
        } else {
            lastUpdate.textContent = new Date().toLocaleTimeString();
        }

        syncJoystickFromState(state);
    }

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

    function startStatePolling() {
        if (statePollingInterval) {
            clearInterval(statePollingInterval);
        }
        pollRobotState();
        statePollingInterval = setInterval(pollRobotState, 2000);
    }

    // =========================
    // Device status polling
    // =========================
    function startDeviceStatusPolling() {
        if (deviceStatusPollingInterval) {
            clearInterval(deviceStatusPollingInterval);
        }
        pollDeviceStatus();
        deviceStatusPollingInterval = setInterval(pollDeviceStatus, 5000);
    }

    async function pollDeviceStatus() {
        if (!isConfigured) return;

        try {
            const data = await apiRequest('/device/status');
            updateDeviceStatus(data);
        } catch (error) {
            updateDeviceStatus({ online: false, last_seen: null, seconds_ago: null });
        }
    }

    function updateDeviceStatus(status) {
        const deviceStatus = document.getElementById('device-status');

        if (status.online) {
            deviceStatus.textContent = 'Online';
            deviceStatus.className = 'status-indicator connected';
        } else {
            if (status.seconds_ago !== null && status.seconds_ago !== undefined) {
                const minutes = Math.floor(status.seconds_ago / 60);
                const seconds = status.seconds_ago % 60;

                let timeText = '';
                if (minutes > 0) {
                    timeText = `${minutes}m ${seconds}s ago`;
                } else {
                    timeText = `${seconds}s ago`;
                }

                deviceStatus.textContent = `Offline (${timeText})`;
            } else {
                deviceStatus.textContent = 'Never Connected';
            }
            deviceStatus.className = 'status-indicator disconnected';
        }
    }

    // =========================
    // Helpers
    // =========================
    async function apiRequest(path, method = 'GET', data = null) {
        const options = {
            method,
            headers: {
                'Content-Type': 'application/json',
                'X-API-Key': apiKey
            }
        };

        if (data) options.body = JSON.stringify(data);

        const response = await fetch(apiEndpoint + path, options);
        if (!response.ok) {
            throw new Error(`HTTP ${response.status}`);
        }
        return response.json();
    }

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

    // Load and display version information
    async function loadVersionInfo() {
        try {
            const response = await fetch(`${apiEndpoint}/health`);
            if (response.ok) {
                const data = await response.json();
                const versionDiv = document.getElementById('version-info');

                if (data.version && !data.version.error) {
                    const v = data.version;
                    const githubUrl = `https://github.com/dobernhardt/webmotor/commit/${v.sha || ''}`;
                    versionDiv.innerHTML = `
                        <small>
                            Version: ${v.semVer || v.version || 'unknown'} |
                            Build: ${v.build_timestamp || 'unknown'} |
                            Commit: <a href="${githubUrl}"
                                      target="_blank" rel="noopener">${v.shortSha || 'unknown'}</a> |
                            Branch: ${v.branchName || 'unknown'}
                        </small>
                    `;
                } else {
                    versionDiv.innerHTML = '<small>Version info not available</small>';
                }
            }
        } catch (error) {
            console.error('Failed to load version info:', error);
            document.getElementById('version-info').innerHTML = '<small>Version info unavailable</small>';
        }
    }
});
