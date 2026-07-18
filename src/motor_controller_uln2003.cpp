#include "motor_controller_uln2003.h"

namespace {
    // Half-step sequence (alternating single/double coil) - 8 states
    constexpr uint8_t kHalfStepSequence[8] = {
        0b0001,  // IN1
        0b0011,  // IN1 + IN2
        0b0010,  // IN2
        0b0110,  // IN2 + IN3
        0b0100,  // IN3
        0b1100,  // IN3 + IN4
        0b1000,  // IN4
        0b1001   // IN4 + IN1
    };
}

MotorControllerULN2003::MotorControllerULN2003(uint8_t in1, uint8_t in2, uint8_t in3, uint8_t in4)
    : pins_{in1, in2, in3, in4},
      stepIndex_(0),
      position_(0) {
}

void MotorControllerULN2003::begin() {
    for (int i = 0; i < 4; i++) {
        pinMode(pins_[i], OUTPUT);
    }
    release();
}

void MotorControllerULN2003::stepOnce(bool clockwise) {
    if (clockwise) {
        stepIndex_ = (stepIndex_ + 1) % 8;
        position_ = position_ + 1;
    } else {
        stepIndex_ = (stepIndex_ == 0) ? 7 : stepIndex_ - 1;
        position_ = position_ - 1;
    }
    applyPattern(kHalfStepSequence[stepIndex_]);
}

void MotorControllerULN2003::holdCurrent() {
    applyPattern(kHalfStepSequence[stepIndex_]);
}

void MotorControllerULN2003::release() {
    applyPattern(0);
}

void MotorControllerULN2003::applyPattern(uint8_t pattern) {
    digitalWrite(pins_[0], (pattern & 0b0001) ? HIGH : LOW);
    digitalWrite(pins_[1], (pattern & 0b0010) ? HIGH : LOW);
    digitalWrite(pins_[2], (pattern & 0b0100) ? HIGH : LOW);
    digitalWrite(pins_[3], (pattern & 0b1000) ? HIGH : LOW);
}
