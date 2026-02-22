#ifndef MOTOR_CONTROLLER_TMC2209_H
#define MOTOR_CONTROLLER_TMC2209_H

#include "abstract_motor_controller.h"
#include "config.h"
#include "state.h"

/**
 * @brief Concrete motor controller implementation for TMC2209 stepper driver
 * 
 * This class provides control for stepper motors using the TMC2209 driver chip.
 * It supports microstepping modes, direction control, and frequency adjustment.
 * Designed for educational use on ESP32-S3 (ATOM S3 LITE).
 */
class MotorControllerTMC2209 : public AbstractMotorController {
public:
    MotorControllerTMC2209();
    virtual ~MotorControllerTMC2209() = default;
    
    // AbstractMotorController interface implementation
    void begin() override;
    void start() override;
    void stop() override;
    void release() override;
    void setFrequency(uint32_t frequencyHz) override;
    uint32_t getFrequency() const override;
    void setMicrostepMode(uint16_t microsteps) override;
    uint16_t getMicrostepMode() const override;
    void setDirection(bool clockwise) override;
    bool getDirection() const override;
    bool runForDegrees(float degrees, uint16_t stepsPerRevolution) override;
    MotorState getMotorState() const override;
    bool isRunning() const override;
    
    // Legacy compatibility methods (for backward compatibility)
    void setMode(MotorMode mode);
    MotorMode getMode() const;
    void setMicrosteps(uint16_t microsteps);  // Alias for setMicrostepMode
    uint16_t getMicrosteps() const;           // Alias for getMicrostepMode
    
private:
    MotorState currentState;
    TaskHandle_t stepTaskHandle;
    volatile bool taskRunning;
    
    // Degree rotation tracking
    volatile bool degreeRotationActive;
    volatile uint32_t targetSteps;
    volatile uint32_t currentSteps;
    
    // Internal methods
    void startStepTask();
    void stopStepTask();
    static void stepTask(void* parameter);
    void setMicrostepPins(uint16_t microsteps);
    void setPinStates();
    uint16_t validateMicrosteps(uint16_t requested);
    uint32_t validateFrequency(uint32_t requested);
};

#endif // MOTOR_CONTROLLER_TMC2209_H
