#ifndef CONFIG_H
#define CONFIG_H

// Wi-Fi Credentials
#ifndef WIFI_SSID
#define WIFI_SSID "your_ssid"
#endif

#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD "your_password"
#endif

// Pin Assignments - ULN2003 tilt motor (Neigung um die X-Achse, 28BYJ-48)
// ESP32-PICO-KIT V4: avoid GPIO 6-11 and 16/17 (internal flash)
#define TILT_IN1_PIN 25
#define TILT_IN2_PIN 26
#define TILT_IN3_PIN 32
#define TILT_IN4_PIN 33

// Pin Assignments - ULN2003 rotation motor (Drehung um die Z-Achse, 28BYJ-48)
#define ROTATION_IN1_PIN 18
#define ROTATION_IN2_PIN 19
#define ROTATION_IN3_PIN 21
#define ROTATION_IN4_PIN 22

// Axis Control Constants
// 28BYJ-48 with half-step sequence: 4096 steps per output shaft revolution
#define STEPS_PER_REVOLUTION 4096

// Gear reduction between motor output shaft and platform, per axis
// (motor revolutions per platform revolution). 1.0 = direct drive.
#define ROTATION_GEAR_RATIO 1.0f
#define TILT_GEAR_RATIO 1.0f

// Steps per degree of PLATFORM angle. All angles in the WebUI, the status
// output and the cloud sync refer to the platform, not the motor shaft -
// the conversion to motor steps happens exclusively through these values.
// Replace the formula with a measured value if the ratio is not exact.
#define ROTATION_STEPS_PER_PLATFORM_DEG ((float)STEPS_PER_REVOLUTION / 360.0f * ROTATION_GEAR_RATIO)
#define TILT_STEPS_PER_PLATFORM_DEG ((float)STEPS_PER_REVOLUTION / 360.0f * TILT_GEAR_RATIO)
// Hard upper bound for the step frequency (28BYJ-48 stalls above ~1000 pps)
#define ABS_MAX_FREQUENCY 1000
// Fixed traversal speed for both axes: 75 % of the stall limit
#define AXIS_STEP_FREQUENCY 750

// Boot defaults for the angle limits. The WebUI is the source of truth:
// limits are not persisted on the device and get re-applied via cloud sync.
#define DEFAULT_ROTATION_LIMIT_DEG 45.0f
#define DEFAULT_TILT_LIMIT_DEG 30.0f
// Hard validation bounds for the limits
#define MAX_ROTATION_LIMIT_DEG 180.0f
#define MAX_TILT_LIMIT_DEG 90.0f

// Other Configuration Constants
#define AP_SSID "WebMotor-Config"
#define AP_IP "192.168.4.1"

// Cloud sync: default API endpoint, used until a different one is saved on
// the device. Same host as the cloud frontend (Azure Static Web Apps serves
// the API under /api); an emptied endpoint falls back to this default.
#define DEFAULT_CLOUD_API_ENDPOINT "https://calm-river-0a48e7503.6.azurestaticapps.net/api"

#endif // CONFIG_H
