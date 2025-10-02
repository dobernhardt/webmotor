#ifndef MOTOR_CONTROLLER_H
#define MOTOR_CONTROLLER_H

#include "config.h"
#include "state.h"
#include <driver/rmt.h>

class MotorController {
public:
    MotorController();
    void begin();
    void reset();
    void setMicrosteps(uint16_t microsteps);
    uint16_t getMicrosteps() const;
    void setFrequency(uint32_t frequency);
    uint32_t getFrequency() const;
    void setDirection(bool clockwise);
    bool getDirection() const;
    void setMode(MotorMode mode);
    MotorMode getMode() const;
    void updateMotorState();
    MotorState getMotorState() const;
private:
    MotorState currentState;
    rmt_channel_t rmtChannel;
    void configureRMT();
    void updateRMT();
};

#endif // MOTOR_CONTROLLER_H