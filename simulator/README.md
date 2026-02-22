# WebMotor Simulators

Python-based simulators for testing the WebMotor system without requiring the actual ATOM S3 LITE hardware.

## Simulators

### 1. Local REST API Simulator (`webmotor_simulator.py`)
Simulates the local web interface served directly from the ESP32.

### 2. Cloud Controller Simulator (`cloud_controller_simulator.py`)
Simulates an ESP32 controller connecting to the Azure cloud backend.

## Features

### Local Simulator
- **REST API Implementation**: Full implementation of the WebMotor REST API
- **Request Validation**: JSON schema validation for all API requests
- **Logging**: Comprehensive logging of all API calls and state changes
- **Web UI Serving**: Serves the actual web UI files from the data directory
- **CORS Support**: Enables cross-origin requests for development
- **Educational Focus**: Designed for learning and testing the WebMotor system

### Cloud Controller Simulator
- **Cloud Integration**: Connects to Azure cloud backend
- **Long Polling**: Polls for commands from cloud (30s timeout)
- **State Synchronization**: Pushes motor state every 2 seconds
- **Position Simulation**: Simulates realistic motor position changes
- **Command Processing**: Handles all motor control commands from cloud
- **Real-time Testing**: Test cloud frontend without ESP32 hardware

## Setup

1. Install Python dependencies:
```bash
pip install -r requirements.txt
```

2. Ensure the web UI files are in the `../data` directory:
   - `index.html`
   - `styles.css`  
   - `app.js`

## Usage

### Local Simulator - Quick Start
```bash
python run_simulator.py
```

### Local Simulator - Advanced Usage
```bash
python webmotor_simulator.py --host 0.0.0.0 --port 8080 --debug
```

### Cloud Controller Simulator
```bash
# Basic usage with Azure Static Web App
python cloud_controller_simulator.py --key YOUR_API_KEY

# Custom endpoint
python cloud_controller_simulator.py \
  --endpoint https://your-app.azurestaticapps.net/api \
  --key YOUR_API_KEY

# Verbose logging
python cloud_controller_simulator.py --key YOUR_API_KEY --verbose
```

### Command Line Options

**Local Simulator:**
- `--host`: Host to bind to (default: 127.0.0.1)
- `--port`: Port to bind to (default: 8080)
- `--debug`: Enable Flask debug mode

**Cloud Controller Simulator:**
- `--endpoint`: Azure Function API endpoint (default: https://calm-river-0a48e7503.6.azurestaticapps.net/api)
- `--key`: API key for authentication (required)
- `--verbose`, `-v`: Enable verbose debug logging

## API Endpoints

### Local Simulator
The local simulator implements all WebMotor REST API endpoints:

### Motor Control
- `GET /api/motor/status` - Get current motor status
- `POST /api/motor/control` - Control motor parameters

### WiFi Configuration  
- `POST /api/wifi/config` - Save WiFi credentials

### Web UI
- `GET /` - Serve the main web interface
- `GET /<filename>` - Serve static files (CSS, JS)

### Cloud Backend API
The cloud controller simulator connects to these endpoints:
- `GET /api/health` - Test connection to backend
- `GET /api/commands/poll` - Long poll for commands (30s)
- `POST /api/state` - Push motor state to cloud
- `GET /api/state` - Get current motor state

## Testing

### Local Simulator Testing
Access the web interface at: http://127.0.0.1:8080

The simulator logs all API calls and validates requests according to the OpenAPI specification. Use the web UI to test:

1. **Motor Control**: Adjust frequency, direction, microsteps
2. **Mode Control**: Start, stop, release motor
3. **WiFi Config**: Test credential saving
4. **Status Updates**: Verify periodic status polling

### Cloud Controller Testing

1. **Start the cloud controller simulator:**
   ```bash
   python cloud_controller_simulator.py --key e70652bed1a4062be80e0c7c0c6a11d517ab4ab227f79baa0994ac8f33725613
   ```

2. **Open the cloud frontend:**
   ```
   https://calm-river-0a48e7503.6.azurestaticapps.net
   ```

3. **Configure the cloud frontend:**
   - Enter API Key: `e70652bed1a4062be80e0c7c0c6a11d517ab4ab227f79baa0994ac8f33725613`
   - Click "Save Configuration"
   - Click "Test Connection" (should show "✓ Connected")

4. **Test motor control from the cloud:**
   - Adjust frequency slider → Simulator logs command received
   - Toggle direction → Position changes direction
   - Change microstepping → Simulator updates parameters
   - Click Start → Motor mode changes to RUNNING, position updates
   - Click Stop → Motor mode changes to STOPPED, position freezes

5. **Verify state synchronization:**
   - Cloud frontend updates motor status every 2 seconds
   - Position value changes when motor is running
   - Status indicators reflect current state

## Validation

The simulator validates all requests against JSON schemas derived from the OpenAPI specification:

- **Motor Control**: Validates frequency (0-10000), direction (boolean), microsteps (1,2,4,8,16), mode (RUNNING/STOPPED/RELEASED)
- **WiFi Config**: Validates SSID length (1-32) and password length (8-63)
- **Content Type**: Ensures JSON content type for POST requests

## Educational Value

This simulator helps understand:
- REST API design and implementation
- JSON schema validation
- HTTP status codes and error handling
- Client-server communication patterns
- Embedded device API constraints

## Hardware Simulation

The simulator maintains internal state that mimics the ATOM S3 LITE behavior:
- Motor parameters (frequency, direction, microsteps, mode)
- WiFi configuration state
- Connection status simulation
- Realistic response formats

## Example Output

```
2025-10-02 15:30:00,123 - INFO - Motor simulator initialized
2025-10-02 15:30:00,124 - INFO - WiFi simulator initialized
2025-10-02 15:30:00,125 - INFO - WebMotor simulator initialized
2025-10-02 15:30:00,126 - INFO - Starting WebMotor simulator on http://127.0.0.1:8080
2025-10-02 15:30:00,127 - INFO - Motor control endpoints:
2025-10-02 15:30:00,128 - INFO -   GET  /api/motor/status
2025-10-02 15:30:00,129 - INFO -   POST /api/motor/control
2025-10-02 15:30:00,130 - INFO -   POST /api/wifi/config
2025-10-02 15:30:00,131 - INFO - Web UI available at root URL
2025-10-02 15:30:15,456 - INFO - Motor status requested: {'frequency': 0, 'direction': False, 'microsteps': 1, 'mode': 'STOPPED', 'connected': True}
2025-10-02 15:30:25,789 - INFO - Motor control request: {'frequency': 1500}
2025-10-02 15:30:25,790 - INFO - Updating motor parameters: {'frequency': 1500}
2025-10-02 15:30:25,791 - INFO - Frequency set to 1500 Hz
```