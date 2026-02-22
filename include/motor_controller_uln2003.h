#ifndef MOTOR_CONTROLLER_ULN2003_H
#define MOTOR_CONTROLLER_ULN2003_H

#include "abstract_motor_controller.h"
#include "config.h"
#include "state.h"

/**
 * @brief Motor controller implementation for ULN2003 driver with 28BYJ-48 stepper
 * 
 * This class provides control for 28BYJ-48 stepper motors using the ULN2003 
 * Darlington transistor array driver. It supports wave drive, full step, and 
 * half step modes. Designed for educational use on ESP32-S3 (ATOM S3 LITE).
 * 
 * Step Mode Mapping:
 * - Microsteps = 1: Wave drive (4096 steps/rev for 28BYJ-48)
 * - Microsteps = 2: Full step (4096 steps/rev for 28BYJ-48)
 * - Microsteps = 4+: Half step (8192 steps/rev for 28BYJ-48)
 */
class MotorControllerULN2003 : public AbstractMotorController {
public:
    MotorControllerULN2003();
    virtual ~MotorControllerULN2003() = default;
    
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
    
    /**
     * @brief Step modes available for ULN2003
     */
    enum class StepMode : uint8_t {
        WAVE_DRIVE = 0,  // Single coil energized (4 steps per sequence)
        FULL_STEP = 1,   // Two coils energized (4 steps per sequence)
        HALF_STEP = 2    // Alternating single/double coil (8 steps per sequence)
    };
    
private:
    MotorState currentState;
    TaskHandle_t stepTaskHandle;
    volatile bool taskRunning;
    
    // Degree rotation tracking
    volatile bool degreeRotationActive;
    volatile uint32_t targetSteps;
    volatile uint32_t currentSteps;
    
    // Step sequencing
    StepMode stepMode;
    uint8_t currentStepIndex;
    
    // Internal methods
    void startStepTask();
    void stopStepTask();
    static void stepTask(void* parameter);
    void doStep();
    void setPinStates(uint8_t pattern);
    void releaseCoils();
    uint16_t validateMicrosteps(uint16_t requested);
    uint32_t validateFrequency(uint32_t requested);
    StepMode microstepsToStepMode(uint16_t microsteps) const;
    uint8_t getStepSequenceLength() const;
    uint8_t getStepPattern(uint8_t stepIndex, bool clockwise) const;
};

#endif // MOTOR_CONTROLLER_ULN2003_H
