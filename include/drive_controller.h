#ifndef DRIVE_CONTROLLER_H
#define DRIVE_CONTROLLER_H

#include <Arduino.h>
#include <Preferences.h>
#include <esp_timer.h>
#include "motor_controller_uln2003.h"
#include "state.h"

/**
 * @brief Vehicle-level controller: maps joystick input to two 28BYJ-48 steppers.
 *
 * - Drive motor (throttle): joystick y in [-1..1] sets step frequency and
 *   direction. Stepped by a periodic esp_timer; released when stopped.
 * - Steering motor: joystick x in [-1..1] sets a target angle within the
 *   configurable steering limit. A fixed-rate esp_timer moves the motor
 *   toward the target and holds torque there. Power-on position = center;
 *   centerSteering() re-defines the current position as straight.
 * - Failsafe: if no setTarget() arrives within DRIVE_FAILSAFE_TIMEOUT_MS
 *   while driving, the drive motor stops (steering keeps its angle).
 *
 * Config (steering limit, max speed) is persisted in NVS.
 */
class DriveController {
public:
    DriveController(MotorControllerULN2003& driveMotor, MotorControllerULN2003& steerMotor);

    void begin();

    /** Call frequently from loop(); enforces the failsafe. */
    void handle();

    /** Apply a joystick target. x = steering, y = throttle, both [-1..1]. */
    void setTarget(float x, float y);

    /** Emergency stop: halt drive and freeze steering at its current angle. */
    void stop();

    /** Define the current steering position as straight (center). */
    void centerSteering();

    /** Validate, apply and persist configuration. Returns false if invalid. */
    bool setConfig(float steerLimitDeg, uint32_t maxFrequency);

    float getSteerLimitDeg() const { return steerLimitDeg_; }
    uint32_t getMaxFrequency() const { return maxFrequency_; }
    DriveStatus getStatus() const;

private:
    static void driveTimerCallback(void* arg);
    static void steerTimerCallback(void* arg);
    void updateDriveTimer();
    void loadConfig();
    void saveConfig();
    int32_t steerLimitSteps() const;

    MotorControllerULN2003& driveMotor_;
    MotorControllerULN2003& steerMotor_;
    Preferences preferences_;
    esp_timer_handle_t driveTimer_;
    esp_timer_handle_t steerTimer_;
    bool driveTimerRunning_;

    // Shared with esp_timer callbacks (32-bit aligned -> atomic on ESP32)
    volatile bool driveForward_;
    volatile uint32_t driveFrequency_;  // 0 = stopped
    volatile int32_t steerTarget_;      // steps from center

    float targetX_;
    float targetY_;
    volatile uint32_t lastTargetMs_;
    volatile bool failsafe_;

    float steerLimitDeg_;
    uint32_t maxFrequency_;
};

#endif // DRIVE_CONTROLLER_H
