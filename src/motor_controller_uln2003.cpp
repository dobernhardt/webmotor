#include "motor_controller_uln2003.h"
#include "config.h"
#include "state.h"
#include <Arduino.h>

namespace {
    // ULN2003 step sequences
    // Wave drive (single coil energized) - 4 steps
    constexpr uint8_t kWaveDriveSequence[4] = {
        0b0001,  // IN1
        0b0010,  // IN2
        0b0100,  // IN3
        0b1000   // IN4
    };
    
    // Full step (two coils energized) - 4 steps
    constexpr uint8_t kFullStepSequence[4] = {
        0b0011,  // IN1 + IN2
        0b0110,  // IN2 + IN3
        0b1100,  // IN3 + IN4
        0b1001   // IN4 + IN1
    };
    
    // Half step (alternating single/double coil) - 8 steps
    constexpr uint8_t kHalfStepSequence[8] = {
        0b0001,  // IN1
        0b0011,  // IN1 + IN2
        0b0010,  // IN2
        0b0110,  // IN2 + IN3
        0b0100,  // IN3
        0b1100,  // IN3 + IN4
        0b1000,  // IN4
        0b1001   // IN4 + IN1
    };
    
    // Conservative defaults for educational use
    constexpr uint16_t kDefaultMicrosteps = 2;  // Full step mode
    constexpr uint32_t kDefaultFrequency = 100;
    constexpr uint32_t kMaxFrequency = 1000;  // ULN2003 is slower than TMC2209
    constexpr uint32_t kMinFrequency = 1;
    constexpr bool kDefaultDirection = true;
    constexpr MotorMode kDefaultMode = MotorMode::STOPPED;
    // Step delay in microseconds (conservative for educational use)
    constexpr uint32_t kMinStepDelayUs = 1000;  // 1ms minimum between steps
}  // namespace

MotorControllerULN2003::MotorControllerULN2003()
    : currentState{ kDefaultMicrosteps, kDefaultFrequency, kDefaultDirection, kDefaultMode },
      stepTaskHandle(nullptr),
      taskRunning(false),
      degreeRotationActive(false),
      targetSteps(0),
      currentSteps(0),
      stepMode(StepMode::FULL_STEP),
      currentStepIndex(0) {
}

void MotorControllerULN2003::begin() {
    Serial.println("[MOTOR] Initializing ULN2003 motor controller");
    
    // Configure all control pins as outputs
    pinMode(ULN2003_IN1_PIN, OUTPUT);
    pinMode(ULN2003_IN2_PIN, OUTPUT);
    pinMode(ULN2003_IN3_PIN, OUTPUT);
    pinMode(ULN2003_IN4_PIN, OUTPUT);
    
    // Initialize step mode based on default microsteps
    stepMode = microstepsToStepMode(currentState.microsteps);
    
    // Release all coils initially
    releaseCoils();
    
    Serial.println("[MOTOR] ULN2003 motor controller initialized");
}

// AbstractMotorController interface implementation

void MotorControllerULN2003::start() {
    setMode(MotorMode::RUNNING);
}

void MotorControllerULN2003::stop() {
    setMode(MotorMode::STOPPED);
}

void MotorControllerULN2003::release() {
    setMode(MotorMode::RELEASED);
}

void MotorControllerULN2003::setMicrostepMode(uint16_t microsteps) {
    setMicrosteps(microsteps);
}

uint16_t MotorControllerULN2003::getMicrostepMode() const {
    return getMicrosteps();
}

bool MotorControllerULN2003::runForDegrees(float degrees, uint16_t stepsPerRevolution) {
    if (currentState.mode == MotorMode::RUNNING && degreeRotationActive) {
        Serial.println("[MOTOR] WARNING: Already running for degrees, ignoring request");
        return false;
    }
    
    if (stepsPerRevolution == 0) {
        Serial.println("[MOTOR] ERROR: Invalid stepsPerRevolution (must be > 0)");
        return false;
    }
    
    // Calculate total steps needed
    // For ULN2003, microsteps represents the step mode which affects steps per revolution
    // For 28BYJ-48: wave/full = 4096, half = 8192
    float stepsMultiplier = (stepMode == StepMode::HALF_STEP) ? 2.0f : 1.0f;
    float totalStepsFloat = (degrees / 360.0f) * stepsPerRevolution * stepsMultiplier;
    uint32_t totalSteps = abs((int32_t)totalStepsFloat);
    
    if (totalSteps == 0) {
        Serial.println("[MOTOR] WARNING: Degrees too small, no movement");
        return false;
    }
    
    // Set direction based on sign of degrees
    bool direction = (degrees >= 0.0f);
    setDirection(direction);
    
    // Initialize degree rotation state
    degreeRotationActive = true;
    targetSteps = totalSteps;
    currentSteps = 0;
    
    Serial.printf("[MOTOR] Running for %.2f degrees (%u steps, %s)\n", 
                 degrees, totalSteps, direction ? "CW" : "CCW");
    
    // Start the motor
    start();
    
    return true;
}

// Legacy compatibility methods

void MotorControllerULN2003::setMicrosteps(uint16_t microsteps) {
    uint16_t validatedMicrosteps = validateMicrosteps(microsteps);
    
    if (currentState.microsteps != validatedMicrosteps) {
        currentState.microsteps = validatedMicrosteps;
        stepMode = microstepsToStepMode(currentState.microsteps);
        currentStepIndex = 0;  // Reset step position
        
        const char* modeStr[] = {"Wave Drive", "Full Step", "Half Step"};
        Serial.printf("[MOTOR] Step mode changed to: %s (microsteps=%u)\n", 
                     modeStr[(int)stepMode], currentState.microsteps);
    }
}

uint16_t MotorControllerULN2003::getMicrosteps() const {
    return currentState.microsteps;
}

void MotorControllerULN2003::setFrequency(uint32_t frequency) {
    uint32_t validatedFrequency = validateFrequency(frequency);
    
    if (currentState.frequency != validatedFrequency) {
        Serial.printf("[MOTOR] Frequency: %u -> %u Hz\n", currentState.frequency, validatedFrequency);
        currentState.frequency = validatedFrequency;
    }
}

uint32_t MotorControllerULN2003::getFrequency() const {
    return currentState.frequency;
}

void MotorControllerULN2003::setDirection(bool clockwise) {
    if (currentState.direction != clockwise) {
        currentState.direction = clockwise;
        Serial.printf("[MOTOR] Direction: %s\n", clockwise ? "CW" : "CCW");
    }
}

bool MotorControllerULN2003::getDirection() const {
    return currentState.direction;
}

void MotorControllerULN2003::setMode(MotorMode mode) {
    if (currentState.mode != mode) {
        Serial.printf("[MOTOR] Mode: %d -> %d\n", (int)currentState.mode, (int)mode);
        currentState.mode = mode;
        
        if (mode == MotorMode::RELEASED) {
            stopStepTask();
            releaseCoils();
            degreeRotationActive = false;
            Serial.println("[MOTOR] Motor released (coils de-energized)");
        } else if (mode == MotorMode::STOPPED) {
            stopStepTask();
            // Keep coils energized for holding torque
            uint8_t pattern = getStepPattern(currentStepIndex, currentState.direction);
            setPinStates(pattern);
            degreeRotationActive = false;
            Serial.println("[MOTOR] Motor stopped with holding torque");
        } else if (mode == MotorMode::RUNNING) {
            startStepTask();
            Serial.println("[MOTOR] Motor running");
        }
    }
}

MotorMode MotorControllerULN2003::getMode() const {
    return currentState.mode;
}

MotorState MotorControllerULN2003::getMotorState() const {
    return currentState;
}

void MotorControllerULN2003::stepTask(void* parameter) {
    MotorControllerULN2003* controller = static_cast<MotorControllerULN2003*>(parameter);
    
    Serial.println("[MOTOR] Step task started");
    
    while (controller->taskRunning) {
        if (controller->currentState.mode == MotorMode::RUNNING && 
            controller->currentState.frequency > 0) {
            
            // Check if we're doing a degree rotation and have reached target
            if (controller->degreeRotationActive) {
                if (controller->currentSteps >= controller->targetSteps) {
                    Serial.println("[MOTOR] Degree rotation complete, stopping");
                    controller->degreeRotationActive = false;
                    controller->currentSteps = 0;
                    controller->targetSteps = 0;
                    // Stop the motor but maintain holding torque
                    controller->setMode(MotorMode::STOPPED);
                    continue;
                }
            }
            
            // Calculate delay between steps in microseconds
            uint32_t periodUs = 1000000UL / controller->currentState.frequency;
            
            // Ensure minimum delay for ULN2003
            if (periodUs < kMinStepDelayUs) {
                periodUs = kMinStepDelayUs;
            }
            
            // Perform one step
            controller->doStep();
            
            // Increment step counter for degree rotation
            if (controller->degreeRotationActive) {
                controller->currentSteps++;
            }
            
            // Wait for the step period
            delayMicroseconds(periodUs);
            
        } else {
            // Motor not running, check every 10ms
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
    
    Serial.println("[MOTOR] Step task stopped");
    controller->stepTaskHandle = nullptr;
    vTaskDelete(nullptr);
}

void MotorControllerULN2003::startStepTask() {
    if (stepTaskHandle == nullptr) {
        taskRunning = true;
        xTaskCreatePinnedToCore(
            stepTask,
            "MotorStep",
            2048,           // Stack size (conservative for ATOM S3)
            this,
            1,              // Priority (low for educational use)
            &stepTaskHandle,
            1               // Core 1 (Core 0 typically used for WiFi)
        );
        Serial.println("[MOTOR] Step task created");
    }
}

void MotorControllerULN2003::stopStepTask() {
    if (stepTaskHandle != nullptr) {
        taskRunning = false;
        // Wait for task to finish
        while (stepTaskHandle != nullptr) {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        Serial.println("[MOTOR] Step task stopped");
    }
}

void MotorControllerULN2003::doStep() {
    // Get the sequence length
    uint8_t sequenceLength = getStepSequenceLength();
    
    // Get the step pattern for current position and direction
    uint8_t pattern = getStepPattern(currentStepIndex, currentState.direction);
    
    // Set the pin states
    setPinStates(pattern);
    
    // Advance to next step in sequence
    if (currentState.direction) {
        // Clockwise
        currentStepIndex = (currentStepIndex + 1) % sequenceLength;
    } else {
        // Counter-clockwise
        if (currentStepIndex == 0) {
            currentStepIndex = sequenceLength - 1;
        } else {
            currentStepIndex--;
        }
    }
}

void MotorControllerULN2003::setPinStates(uint8_t pattern) {
    digitalWrite(ULN2003_IN1_PIN, (pattern & 0b0001) ? HIGH : LOW);
    digitalWrite(ULN2003_IN2_PIN, (pattern & 0b0010) ? HIGH : LOW);
    digitalWrite(ULN2003_IN3_PIN, (pattern & 0b0100) ? HIGH : LOW);
    digitalWrite(ULN2003_IN4_PIN, (pattern & 0b1000) ? HIGH : LOW);
}

void MotorControllerULN2003::releaseCoils() {
    digitalWrite(ULN2003_IN1_PIN, LOW);
    digitalWrite(ULN2003_IN2_PIN, LOW);
    digitalWrite(ULN2003_IN3_PIN, LOW);
    digitalWrite(ULN2003_IN4_PIN, LOW);
}

uint16_t MotorControllerULN2003::validateMicrosteps(uint16_t requested) {
    // Map to valid step modes
    // 1 = Wave drive
    // 2 = Full step (default)
    // 4+ = Half step
    if (requested == 1) {
        return 1;  // Wave drive
    } else if (requested == 2) {
        return 2;  // Full step
    } else if (requested >= 4) {
        return 4;  // Half step
    }
    
    // Default to full step
    Serial.printf("[MOTOR] Invalid microsteps %u, using default %u\n", requested, kDefaultMicrosteps);
    return kDefaultMicrosteps;
}

uint32_t MotorControllerULN2003::validateFrequency(uint32_t requested) {
    if (requested > kMaxFrequency) {
        Serial.printf("[MOTOR] Frequency %u too high, clamping to %u\n", requested, kMaxFrequency);
        return kMaxFrequency;
    }
    
    if (requested < kMinFrequency && requested != 0) {
        Serial.printf("[MOTOR] Frequency %u too low, using minimum %u\n", requested, kMinFrequency);
        return kMinFrequency;
    }
    
    return requested;
}

MotorControllerULN2003::StepMode MotorControllerULN2003::microstepsToStepMode(uint16_t microsteps) const {
    if (microsteps == 1) {
        return StepMode::WAVE_DRIVE;
    } else if (microsteps == 2) {
        return StepMode::FULL_STEP;
    } else {
        return StepMode::HALF_STEP;
    }
}

uint8_t MotorControllerULN2003::getStepSequenceLength() const {
    switch (stepMode) {
        case StepMode::WAVE_DRIVE:
            return 4;
        case StepMode::FULL_STEP:
            return 4;
        case StepMode::HALF_STEP:
            return 8;
        default:
            return 4;
    }
}

uint8_t MotorControllerULN2003::getStepPattern(uint8_t stepIndex, bool clockwise) const {
    // Get the pattern from the appropriate sequence array
    switch (stepMode) {
        case StepMode::WAVE_DRIVE:
            return kWaveDriveSequence[stepIndex % 4];
        case StepMode::FULL_STEP:
            return kFullStepSequence[stepIndex % 4];
        case StepMode::HALF_STEP:
            return kHalfStepSequence[stepIndex % 8];
        default:
            return 0;
    }
}

bool MotorControllerULN2003::isRunning() const {
    return currentState.mode == MotorMode::RUNNING && currentState.frequency > 0;
}
