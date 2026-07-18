#ifndef MOTOR_CONTROLLER_ULN2003_H
#define MOTOR_CONTROLLER_ULN2003_H

#include <Arduino.h>

/**
 * @brief Single 28BYJ-48 stepper on a ULN2003 driver board.
 *
 * Lean, task-free driver: it only knows how to advance the half-step
 * sequence and track its signed step position. Timing (when to step)
 * is owned by the caller (see DriveController).
 *
 * Half-step sequence -> 4096 steps per output shaft revolution.
 */
class MotorControllerULN2003 {
public:
    MotorControllerULN2003(uint8_t in1, uint8_t in2, uint8_t in3, uint8_t in4);

    void begin();

    /** Advance one half-step. Clockwise increments the position counter. */
    void stepOnce(bool clockwise);

    /** Re-energize the current step pattern (holding torque). */
    void holdCurrent();

    /** De-energize all coils (no holding torque, no heat). */
    void release();

    int32_t getPosition() const { return position_; }

    /** Define the current physical position as zero. */
    void resetPosition() { position_ = 0; }

private:
    void applyPattern(uint8_t pattern);

    uint8_t pins_[4];
    uint8_t stepIndex_;
    volatile int32_t position_;
};

#endif // MOTOR_CONTROLLER_ULN2003_H
