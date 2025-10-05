#ifndef MOTOR_CONTROLLER_H
#define MOTOR_CONTROLLER_H

#include "config.h"
#include "state.h"
#include <driver/rmt.h>

class MotorController {
public:
    MotorController();
    void begin();
    
    // Primary control methods
    void setMicrosteps(uint16_t microsteps);
    uint16_t getMicrosteps() const;
    void setFrequency(uint32_t frequency);
    uint32_t getFrequency() const;
    void setDirection(bool clockwise);
    bool getDirection() const;
    void setMode(MotorMode mode);
    MotorMode getMode() const;
    
    // State management
    MotorState getMotorState() const;
    bool isRunning() const;
    
private:
    MotorState currentState;
    rmt_channel_t rmtChannel;
    bool rmtConfigured;
    
    // Internal methods
    void configureRMT();
    void updateRMT();
    void setMicrostepPins(uint16_t microsteps);
    void setPinStates();
    uint16_t validateMicrosteps(uint16_t requested);
    uint32_t validateFrequency(uint32_t requested);
};

#endif // MOTOR_CONTROLLER_H