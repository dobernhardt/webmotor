#ifndef STATE_H
#define STATE_H

#include <Arduino.h>

struct PlatformStatus {
    float x = 0.0f;                 // last joystick x [-1..1] (rotation target)
    float y = 0.0f;                 // last joystick y [-1..1] (tilt target)
    float rotationDeg = 0.0f;       // current rotation around z, relative to center
    float tiltDeg = 0.0f;           // current tilt around x, relative to center
    float rotationLimitDeg = 0.0f;  // configured rotation limit
    float tiltLimitDeg = 0.0f;      // configured tilt limit
    bool moving = false;            // at least one axis still traveling to its target
};

#endif // STATE_H
