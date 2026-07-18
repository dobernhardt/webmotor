#ifndef STATE_H
#define STATE_H

#include <Arduino.h>

struct DriveStatus {
    float x;                // last joystick x [-1..1] (steering)
    float y;                // last joystick y [-1..1] (throttle)
    float steeringDeg;      // current steering angle relative to center
    float steerLimitDeg;    // configured steering limit
    uint32_t maxFrequency;  // configured max drive step frequency (Hz)
    bool driving;           // drive motor currently stepping
    bool failsafe;          // drive stopped because joystick updates timed out
};

#endif // STATE_H
