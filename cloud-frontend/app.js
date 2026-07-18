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

    // Load saved configuration from localStorage
    loadConfiguration();

    // Load version information
    loadVersionInfo();

    // Configuration handlers
    document.getElementById('save-config').addEventListener('click', saveConfiguration);
    document.getElementById('test-connection').addEventListener('click', testConnection);

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
    // {0,0} a few times. The backend stores only the latest value, the ESP
    // ignores targets older than 2 s (failsafe).
    setInterval(() => {
        if (!isConfigured) return;
        if (engaged) {
            sendDriveTarget(joyX, joyY);
        } else if (releaseSendsLeft > 0) {
            releaseSendsLeft--;
            sendDriveTarget(0, 0);
        }
    }, SEND_INTERVAL_MS);

    async function sendDriveTarget(x, y) {
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
        releaseJoystick();
        // Belt and braces: zero the drive target AND queue a stop command
        sendDriveTarget(0, 0);
        try {
            await apiRequest('/commands', 'POST', { action: 'stop' });
            updateStatus('🛑 NOT-AUS gesendet');
        } catch (error) {
            updateStatus(`✗ NOT-AUS fehlgeschlagen: ${error.message}`);
        }
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

    async function loadDriveConfig() {
        if (!isConfigured) return;
        try {
            const data = await apiRequest('/drive/config');
            if (data.config) {
                if (data.config.steerLimitDeg !== undefined && data.config.steerLimitDeg !== null) {
                    steerLimitSlider.value = Math.round(data.config.steerLimitDeg);
                    steerLimitValue.textContent = Math.round(data.config.steerLimitDeg);
                }
                if (data.config.maxFrequency !== undefined && data.config.maxFrequency !== null) {
                    maxSpeedSlider.value = data.config.maxFrequency;
                    maxSpeedValue.textContent = data.config.maxFrequency;
                }
            }
        } catch (error) {
            // Config not set yet - keep defaults
        }
    }

    document.getElementById('save-drive-config').addEventListener('click', async function() {
        try {
            await apiRequest('/drive/config', 'POST', {
                steerLimitDeg: parseFloat(steerLimitSlider.value),
                maxFrequency: parseInt(maxSpeedSlider.value)
            });
            updateStatus(`✓ Limits gespeichert: X ±${steerLimitSlider.value}°, Y max ${maxSpeedSlider.value} Schritte/s`);
        } catch (error) {
            updateStatus(`✗ Fehler beim Speichern: ${error.message}`);
        }
    });

    document.getElementById('center-steering').addEventListener('click', async function() {
        try {
            await apiRequest('/commands', 'POST', { action: 'center' });
            updateStatus('✓ Zentrieren gesendet (aktuelle Stellung = Mitte)');
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
            loadDriveConfig();
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
        loadDriveConfig();
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
        const driveStatus = document.getElementById('drive-status');
        if (state.failsafe) {
            driveStatus.textContent = 'Failsafe';
            driveStatus.className = 'motor-status stopped';
        } else if (state.driving) {
            driveStatus.textContent = 'Läuft';
            driveStatus.className = 'motor-status running';
        } else if (state.driving !== undefined) {
            driveStatus.textContent = 'Steht';
            driveStatus.className = 'motor-status unknown';
        }

        const steeringValue = document.getElementById('steering-value');
        if (state.steeringDeg !== undefined && state.steerLimitDeg !== undefined) {
            steeringValue.textContent =
                `${Number(state.steeringDeg).toFixed(1)}° / ±${Number(state.steerLimitDeg).toFixed(0)}°`;
        }

        const lastUpdate = document.getElementById('last-update');
        if (state.timestamp) {
            const time = new Date(state.timestamp);
            lastUpdate.textContent = time.toLocaleTimeString();
        } else {
            lastUpdate.textContent = new Date().toLocaleTimeString();
        }
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
