#ifndef CONFIG_H
#define CONFIG_H

// Wi-Fi Credentials
#ifndef WIFI_SSID
#define WIFI_SSID "your_ssid"
#endif

#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD "your_password"
#endif

// Pin Assignments - TMC2209 Driver
#define STEP_PIN 7
#define DIR_PIN 8
#define EN_PIN 38
#define MS1_PIN 5
#define MS2_PIN 6

// Pin Assignments - ULN2003 Driver (for 28BYJ-48 stepper)
#define ULN2003_IN1_PIN 1
#define ULN2003_IN2_PIN 2
#define ULN2003_IN3_PIN 3
#define ULN2003_IN4_PIN 4

// Motor Control Constants
#define MAX_FREQUENCY 10000
#define MIN_FREQUENCY 0

// Microstepping Options
#define MICROSTEPS_1 1
#define MICROSTEPS_2 2
#define MICROSTEPS_4 4
#define MICROSTEPS_8 8
#define MICROSTEPS_16 16

// Other Configuration Constants
#define AP_SSID "WebMotor-Config"
#define AP_IP "192.168.4.1"

#endif // CONFIG_H