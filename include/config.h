#ifndef CONFIG_H
#define CONFIG_H

// Wi-Fi Credentials
#ifndef WIFI_SSID
#define WIFI_SSID "your_ssid"
#endif

#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD "your_password"
#endif

// Pin Assignments - ULN2003 drive motor (Antrieb, 28BYJ-48)
// ESP32-PICO-KIT V4: avoid GPIO 6-11 and 16/17 (internal flash)
#define DRIVE_IN1_PIN 25
#define DRIVE_IN2_PIN 26
#define DRIVE_IN3_PIN 32
#define DRIVE_IN4_PIN 33

// Pin Assignments - ULN2003 steering motor (Lenkung, 28BYJ-48)
#define STEER_IN1_PIN 18
#define STEER_IN2_PIN 19
#define STEER_IN3_PIN 21
#define STEER_IN4_PIN 22

// Drive Control Constants
// 28BYJ-48 with half-step sequence: 4096 steps per output shaft revolution
#define STEPS_PER_REVOLUTION 4096
// Hard upper bound for the drive step frequency (28BYJ-48 stalls above ~1000 pps)
#define ABS_MAX_FREQUENCY 1000
// Below this frequency the drive is treated as stopped
#define MIN_DRIVE_FREQUENCY 20
// Fixed step frequency used to move the steering motor to its target
#define STEER_FREQUENCY 500
// Drive stops automatically when no joystick update arrives within this window
#define DRIVE_FAILSAFE_TIMEOUT_MS 2000

// Defaults (overridden by values persisted in NVS)
#define DEFAULT_STEER_LIMIT_DEG 45.0f
#define DEFAULT_MAX_FREQUENCY 600

// Other Configuration Constants
#define AP_SSID "WebMotor-Config"
#define AP_IP "192.168.4.1"

#endif // CONFIG_H
