#ifndef MOTOR_CONTROLLER_H
#define MOTOR_CONTROLLER_H

#include "config.h"
#include "state.h"

class MotorController {
public:
    MotorController();
    void begin();
    void setMicrostepping(int microsteps);
    void setFrequency(int frequency);
    void setDirection(bool direction);
    void setMode(int mode);
    void updateMotorState();
    MotorState getMotorState();

private:
    MotorState currentState;
    void generatePulse();
    void configureRMT();
};

#endif // MOTOR_CONTROLLER_H