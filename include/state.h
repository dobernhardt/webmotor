#ifndef STATE_H
#define STATE_H

struct MotorState {
    int microsteps;  // Microstepping setting: {1, 2, 4, 8, 16}
    int frequency;   // Frequency in Hz (0-10000)
    bool direction;  // Direction of rotation: true for CW, false for CCW
    int mode;        // Mode: 0 for STOPPED, 1 for RUNNING, 2 for RELEASED
};

#endif // STATE_H