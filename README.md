# WebMotor Project

## Overview
WebMotor is an educational Arduino framework-based firmware designed for the M5Stack ATOM S3 Lite (ESP32-S3). It provides a simple and intuitive way to control a stepper motor using a TMC2209 driver. The project exposes a REST API and a web interface for configuration and live control of motor parameters such as speed, direction, microstepping, and mode.

## Features
- Continuous step pulse generation up to 10 kHz.
- Runtime adjustment of microstepping, direction, and run mode.
- Network-based control via REST API and a web interface over Wi-Fi, with an access point (AP) fallback.
- mDNS discoverability for easy access.
- Persistent storage of Wi-Fi credentials.
- **Visual status indication** via the ATOM S3 Lite's built-in RGB LED.
- **☁️ Cloud Integration** for remote motor control from anywhere with internet access.

## Cloud Integration

WebMotor now supports remote control via Azure cloud services! Control your motor from anywhere with internet access using:

- **Azure Function Backend**: REST API with long-polling for efficient command delivery
- **Cloud Frontend**: Web-based interface for remote control
- **Real-time State Sync**: Motor state updates every 2 seconds
- **API Key Authentication**: Secure communication
- **Dual Control**: Local and cloud control work simultaneously
- **Persistent Configuration**: Cloud settings stored in NVS

### Quick Start

1. **Set up Azure resources** (Storage Account, Function App, Static Web App)
2. **Deploy the backend** from `azure-function/` directory
3. **Deploy the frontend** from `cloud-frontend/` directory
4. **Configure controller** via local web UI

📖 **See [CLOUD_SETUP.md](CLOUD_SETUP.md) for complete setup instructions**

### Architecture

```
Cloud Frontend → Azure Function → Queue Storage
                                      ↓
Controller ← Long Poll (30s) ─────────┘

Controller → Azure Function → Table Storage
                                   ↓
Cloud Frontend ← Polling (2s) ─────┘
```

## LED Status Indicators

The ATOM S3 Lite's built-in RGB LED provides real-time visual feedback about the system's operational status. This is particularly useful for educational demonstrations and debugging without needing serial output.

### Boot Sequence
| Pattern | Description |
|---------|-------------|
| Fast red blink | System booting up |
| 2 blue blinks | Wi-Fi Manager initializing |
| 3 green blinks | Motor Controller initializing |
| 4 yellow blinks | Web Server initializing |
| 3 white blinks | Initialization complete |

### Operational Status Patterns
| Pattern | Status | Description |
|---------|--------|-------------|
| **Slow green pulse** | Wi-Fi Connected | System ready, connected to Wi-Fi network |
| **Orange/Blue alternating** | Access Point Mode | Device in setup mode, connect to configure Wi-Fi |
| **Medium red blink** | Wi-Fi Disconnected | Lost connection to Wi-Fi network |
| **Very slow blue pulse** | Motor Idle | Motor idle, Wi-Fi connected |
| **Fast green pulse** | Motor Running Forward | Motor actively running in forward direction |
| **Fast purple pulse** | Motor Running Reverse | Motor actively running in reverse direction |
| **Solid yellow** | Motor Stopped | Motor stopped (powered but not moving) |
| **Slow cyan pulse** | Motor Released | Motor released (unpowered) |
| **Fast red/white alternating** | System Error | Critical system error detected |
| **Dim purple quick pulse** | Heartbeat | Brief pulse every 10 seconds (system alive indicator) |

### LED State Priority System
The LED follows a priority hierarchy to ensure the most critical information is always displayed:

1. **Heartbeat** (temporary override every 10s)
2. **Wi-Fi Disconnected** (critical connectivity issue)
3. **Access Point Mode** (setup required)
4. **Motor States** (when motor is active)
5. **Wi-Fi Connected** (normal operation)
6. **Motor Idle** (default state)

### Web API Visual Feedback
When commands are received via the web interface or REST API, the LED briefly flashes white to provide immediate confirmation that the command was received, before returning to the appropriate status pattern.

## Project Structure
```
WebMotor
├── data
│   ├── app.js          # JavaScript for local web interface
│   ├── index.html      # Main HTML page for local web interface
│   └── styles.css      # CSS styles for local web interface
├── include
│   ├── abstract_motor_controller.h # Abstract motor controller interface
│   ├── cloud_client.h   # Cloud client for Azure integration
│   ├── config.h        # Configuration constants
│   ├── motor_controller_tmc2209.h # TMC2209 motor control
│   ├── state.h         # Motor state definitions
│   ├── web_server.h    # HTTP server and API handling
│   └── wifi_manager.h  # Wi-Fi management functions
├── src
│   ├── main.cpp        # Entry point of the application
│   ├── cloud_client.cpp # Implementation of cloud client
│   ├── motor_controller_tmc2209.cpp # Implementation of motor control
│   ├── web_server.cpp  # Implementation of web server
│   └── wifi_manager.cpp # Implementation of Wi-Fi manager
├── azure-function      # Azure Function backend for cloud control
│   ├── function_app.py # Main Azure Function application
│   ├── requirements.txt # Python dependencies
│   ├── host.json       # Azure Functions configuration
│   └── README.md       # Backend documentation
├── cloud-frontend      # Cloud-based web interface
│   ├── index.html      # Cloud frontend HTML
│   ├── app.js          # Cloud frontend JavaScript
│   ├── styles.css      # Cloud frontend CSS
│   └── README.md       # Frontend documentation
├── test
│   └── test_motor_controller.cpp # Unit tests for motor controller
├── platformio.ini      # PlatformIO configuration file
├── CLOUD_SETUP.md      # Complete cloud setup guide
└── README.md           # Project documentation
```

## Getting Started
1. **Clone the Repository**: 
   ```
   git clone <repository-url>
   cd WebMotor
   ```

2. **Install PlatformIO**: Ensure you have PlatformIO installed in your development environment.

3. **Open the Project**: Open the project folder in your preferred IDE that supports PlatformIO.

4. **Configure Wi-Fi Credentials**: Update the `include/config.h` file with your Wi-Fi credentials.

5. **Build and Upload**: Use PlatformIO to build the project and upload it to the M5Stack ATOM S3 Lite.

6. **Access the Web Interface**: Once the device is connected to Wi-Fi, access the web interface via the provided mDNS hostname or IP address.

## Usage
- Use the web interface to control the motor's microstepping, frequency, direction, and mode.
- Monitor the motor status and make adjustments in real-time.

## Future Enhancements
- Implement acceleration profiles.
- Add WebSocket support for real-time updates.
- Introduce authentication for secure access.
- Expand to support multiple motors.

## License
This project is licensed under the MIT License. See the LICENSE file for more details.