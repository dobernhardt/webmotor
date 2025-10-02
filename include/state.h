#ifndef STATE_H
#define STATE_H

#include <Arduino.h>

enum MotorMode : uint8_t {
    STOPPED = 0,
    RUNNING = 1,
    RELEASED = 2
};

struct MotorState {
    uint16_t microsteps;   // Microstepping setting: {1, 2, 4, 8, 16}
    uint32_t frequency;    // Frequency in Hz (0-10000)
    bool direction;        // Direction of rotation: true for CW, false for CCW
    MotorMode mode;        // Operational mode
};

#endif // STATE_H