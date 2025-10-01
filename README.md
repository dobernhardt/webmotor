# WebMotor - ESP32 Stepper Motor Controller

A web-based stepper motor controller using ESP32 (ATOM S3 LITE) and TMC2209 driver. This project provides both a REST API and web interface for controlling stepper motors with precise microstepping capabilities.

## Features

- 🎯 TMC2209 stepper driver support
- 🌐 WiFi connectivity with automatic AP fallback
- 🔄 Real-time motor control via web interface
- 🛠 REST API for automation
- 🔍 mDNS discovery support
- 📱 Responsive web interface
- ⚙️ Configurable microstepping (1/1 to 1/16)
- 🔋 Motor power management (run/stop/release modes)

## Hardware Requirements

- ESP32 ATOM S3 LITE
- TMC2209 stepper driver
- Stepper motor
- Power supply suitable for your motor

## Pin Connections

| ESP32 Pin | TMC2209 Pin | Function |
|-----------|-------------|----------|
| GPIO4     | STEP        | Step signal |
| GPIO5     | DIR        | Direction |
| GPIO6     | EN         | Enable/disable |
| GPIO7     | MS1        | Microstep config 1 |
| GPIO8     | MS2        | Microstep config 2 |

## Building and Flashing

This project uses ESP-IDF framework. To build and flash:

```bash
# Clone the repository
git clone https://github.com/yourusername/WebMotor.git
cd WebMotor

# Build the project
idf.py build

# Flash to your device
idf.py -p (PORT) flash
```

## First-Time Setup

1. Power on the device
2. Connect to the "WebMotor-Config" WiFi network 
3. Open http://192.168.4.1 in your browser
4. Configure your WiFi network credentials
5. Device will reboot and connect to your network

## Using the Web Interface

Access the web interface by either:
- Opening http://webmotor.local (if mDNS is supported)
- Using the device's IP address (check your router or serial output)

### Features:
- Microstepping control (1/1 to 1/16 steps)
- Speed control (0-10,000 Hz)
- Direction control (CW/CCW)
- Operation modes (Start/Stop/Release)
- WiFi configuration

## REST API

### Endpoints

#### GET /api/motor/status
Returns current motor status:
```json
{
    "microsteps": 16,
    "frequency": 1000,
    "direction": true,
    "mode": 1
}
```

#### POST /api/motor/control
Control motor parameters:
```json
{
    "microsteps": 16,    // 1, 2, 4, 8, or 16
    "frequency": 1000,   // 0-10000 Hz
    "direction": true,   // true=CW, false=CCW
    "mode": 1           // 0=stopped, 1=running, 2=released
}
```

#### GET /api/wifi/config
Get current WiFi configuration:
```json
{
    "ssid": "current_network",
    "configured": true
}
```

#### POST /api/wifi/config
Configure WiFi settings:
```json
{
    "ssid": "your_network",
    "password": "your_password"
}
```

## Technical Details

### Implementation Features

- Hardware-precise stepping using RMT peripheral
- 1μs pulse width for reliable driver operation
- Up to 10kHz stepping frequency
- Non-blocking web server operation
- Persistent WiFi configuration in NVS
- Automatic fallback to AP mode if WiFi connection fails

### Components

1. **TMC2209 Driver (components/tmc2209)**
   - Hardware control interface
   - Microstepping configuration
   - RMT-based pulse generation

2. **WiFi Manager (components/wifi_manager)**
   - Connection management
   - AP/STA mode switching
   - Configuration persistence

3. **Web Server (components/web_server)**
   - REST API implementation
   - Static file serving
   - mDNS service

## Development

### Project Structure
```
WebMotor/
├── components/
│   ├── tmc2209/         # Motor driver component
│   ├── wifi_manager/    # WiFi management
│   └── web_server/      # Web interface and API
├── main/
│   └── main.c          # Application entry point
└── html/               # Web interface files
```

## License

[MIT License](LICENSE) - feel free to use in your own projects.

## Contributing

Contributions are welcome! Please feel free to submit a Pull Request.