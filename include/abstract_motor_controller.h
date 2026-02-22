#ifndef ABSTRACT_MOTOR_CONTROLLER_H
#define ABSTRACT_MOTOR_CONTROLLER_H

#include <Arduino.h>
#include "state.h"

/**
 * @brief Abstract base class for motor controllers
 * 
 * Provides a common interface for different motor driver implementations.
 * Concrete implementations handle specific driver hardware (e.g., TMC2209, A4988, etc.)
 */
class AbstractMotorController {
public:
    virtual ~AbstractMotorController() = default;
    
    /**
     * @brief Initialize the motor controller hardware
     */
    virtual void begin() = 0;
    
    /**
     * @brief Start the motor (enable and begin stepping)
     */
    virtual void start() = 0;
    
    /**
     * @brief Stop the motor while maintaining holding torque
     */
    virtual void stop() = 0;
    
    /**
     * @brief Release the motor (disable driver, no holding torque)
     */
    virtual void release() = 0;
    
    /**
     * @brief Set the stepping frequency
     * @param frequencyHz Frequency in Hz (steps per second)
     */
    virtual void setFrequency(uint32_t frequencyHz) = 0;
    
    /**
     * @brief Get the current stepping frequency
     * @return Frequency in Hz
     */
    virtual uint32_t getFrequency() const = 0;
    
    /**
     * @brief Set the microstepping mode
     * @param microsteps Number of microsteps per full step (e.g., 2, 4, 8, 16)
     */
    virtual void setMicrostepMode(uint16_t microsteps) = 0;
    
    /**
     * @brief Get the current microstepping mode
     * @return Number of microsteps per full step
     */
    virtual uint16_t getMicrostepMode() const = 0;
    
    /**
     * @brief Set the rotation direction
     * @param clockwise true for clockwise, false for counter-clockwise
     */
    virtual void setDirection(bool clockwise) = 0;
    
    /**
     * @brief Get the current rotation direction
     * @return true for clockwise, false for counter-clockwise
     */
    virtual bool getDirection() const = 0;
    
    /**
     * @brief Rotate the motor for a specified number of degrees
     * @param degrees Number of degrees to rotate (positive or negative)
     * @param stepsPerRevolution Number of full steps per revolution (motor-specific)
     * @return true if operation started successfully, false otherwise
     */
    virtual bool runForDegrees(float degrees, uint16_t stepsPerRevolution) = 0;
    
    /**
     * @brief Get the current motor state
     * @return MotorState struct containing all state information
     */
    virtual MotorState getMotorState() const = 0;
    
    /**
     * @brief Check if the motor is currently running
     * @return true if motor is running, false otherwise
     */
    virtual bool isRunning() const = 0;
};

#endif // ABSTRACT_MOTOR_CONTROLLER_H
