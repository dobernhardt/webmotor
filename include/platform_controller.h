#ifndef PLATFORM_CONTROLLER_H
#define PLATFORM_CONTROLLER_H

#include <Arduino.h>
#include <esp_timer.h>
#include "motor_controller_uln2003.h"
#include "state.h"

/**
 * @brief Platform controller: maps joystick input to two position-controlled
 *        axes driven by 28BYJ-48 steppers.
 *
 * - Rotation axis (around z): joystick x in [-1..1] sets a target angle;
 *   full deflection corresponds to the configured rotation limit.
 * - Tilt axis (around x): joystick y in [-1..1] sets a target angle within
 *   the configured tilt limit.
 * - Both motors are moved by fixed-rate esp_timers (AXIS_STEP_FREQUENCY)
 *   toward their target and keep holding torque at all times - the tilt
 *   axis carries load and must not slip, or the step counting would no
 *   longer match the physical position.
 * - Power-on position = center (0 deg) on both axes; centerAxes()
 *   re-defines the current position as center.
 * - The angle limits are owned by the WebUI: they are NOT persisted on the
 *   device and get re-applied via cloud sync after every boot (boot
 *   defaults from config.h).
 */
class PlatformController {
public:
    PlatformController(MotorControllerULN2003& rotationMotor, MotorControllerULN2003& tiltMotor);

    void begin();

    /** Apply a joystick target. x = rotation, y = tilt, both [-1..1]. */
    void setTarget(float x, float y);

    /** Emergency stop: freeze both axes at their current position. */
    void stop();

    /** Define the current position of both axes as center (0 deg). */
    void centerAxes();

    /** Validate and apply the angle limits. Returns false if invalid. */
    bool setLimits(float rotationLimitDeg, float tiltLimitDeg);

    float getRotationLimitDeg() const { return rotationLimitDeg_; }
    float getTiltLimitDeg() const { return tiltLimitDeg_; }
    PlatformStatus getStatus() const;

private:
    static void rotationTimerCallback(void* arg);
    static void tiltTimerCallback(void* arg);
    int32_t limitSteps(float limitDeg) const;

    MotorControllerULN2003& rotationMotor_;
    MotorControllerULN2003& tiltMotor_;
    esp_timer_handle_t rotationTimer_;
    esp_timer_handle_t tiltTimer_;

    // Shared with esp_timer callbacks (32-bit aligned -> atomic on ESP32)
    volatile int32_t rotationTarget_;  // steps from center
    volatile int32_t tiltTarget_;      // steps from center

    float targetX_;
    float targetY_;
    float rotationLimitDeg_;
    float tiltLimitDeg_;
};

#endif // PLATFORM_CONTROLLER_H
