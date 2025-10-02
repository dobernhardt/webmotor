# WebMotor Project

## Overview
WebMotor is an educational Arduino framework-based firmware designed for the M5Stack ATOM S3 Lite (ESP32-S3). It provides a simple and intuitive way to control a stepper motor using a TMC2209 driver. The project exposes a REST API and a web interface for configuration and live control of motor parameters such as speed, direction, microstepping, and mode.

## Features
- Continuous step pulse generation up to 10 kHz.
- Runtime adjustment of microstepping, direction, and run mode.
- Network-based control via REST API and a web interface over Wi-Fi, with an access point (AP) fallback.
- mDNS discoverability for easy access.
- Persistent storage of Wi-Fi credentials.

## Project Structure
```
WebMotor
├── data
│   ├── app.js          # JavaScript for web interface
│   ├── index.html      # Main HTML page for the web interface
│   └── styles.css      # CSS styles for the web interface
├── include
│   ├── config.h        # Configuration constants
│   ├── motor_controller.h # Motor control functions
│   ├── state.h         # Motor state definitions
│   ├── web_server.h     # HTTP server and API handling
│   └── wifi_manager.h   # Wi-Fi management functions
├── src
│   ├── main.cpp        # Entry point of the application
│   ├── motor_controller.cpp # Implementation of motor control
│   ├── web_server.cpp   # Implementation of web server
│   └── wifi_manager.cpp  # Implementation of Wi-Fi manager
├── test
│   └── test_motor_controller.cpp # Unit tests for motor controller
├── platformio.ini      # PlatformIO configuration file
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