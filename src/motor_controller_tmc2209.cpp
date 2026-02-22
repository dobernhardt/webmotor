#include "motor_controller_tmc2209.h"
#include "config.h"
#include "state.h"
#include <Arduino.h>

namespace {
    // TMC2209 supported microstepping ratios (no 1x via MS pins)
    constexpr uint16_t kAllowedMicrosteps[] = {2, 4, 8, 16};
    constexpr size_t kNumAllowedMicrosteps = sizeof(kAllowedMicrosteps) / sizeof(uint16_t);
    // TMC2209 default is 8 microsteps
    constexpr uint16_t kDefaultMicrosteps = 8;
    // Conservative defaults for educational use
    constexpr uint32_t kDefaultFrequency = 100;
    constexpr uint32_t kMaxFrequency = 5000;  // More conservative for ATOM S3 Lite
    constexpr uint32_t kMinFrequency = 1;
    constexpr bool kDefaultDirection = true;
    constexpr MotorMode kDefaultMode = MotorMode::STOPPED;
    // Pulse width in microseconds (conservative for educational use)
    constexpr uint32_t kPulseWidthUs = 5;
}  // namespace

MotorControllerTMC2209::MotorControllerTMC2209()
    : currentState{ kDefaultMicrosteps, kDefaultFrequency, kDefaultDirection, kDefaultMode },
      stepTaskHandle(nullptr),
      taskRunning(false),
      degreeRotationActive(false),
      targetSteps(0),
      currentSteps(0) {
}

void MotorControllerTMC2209::begin() {
    Serial.println("[MOTOR] Initializing TMC2209 motor controller");
    
    // Configure all pins
    pinMode(DIR_PIN, OUTPUT);
    pinMode(EN_PIN, OUTPUT);
    pinMode(MS1_PIN, OUTPUT);
    pinMode(MS2_PIN, OUTPUT);
    pinMode(STEP_PIN, OUTPUT);
    
    // Set initial pin states
    setPinStates();
    digitalWrite(STEP_PIN, LOW);
    
    Serial.println("[MOTOR] TMC2209 motor controller initialized");
}

// AbstractMotorController interface implementation

void MotorControllerTMC2209::start() {
    setMode(MotorMode::RUNNING);
}

void MotorControllerTMC2209::stop() {
    setMode(MotorMode::STOPPED);
}

void MotorControllerTMC2209::release() {
    setMode(MotorMode::RELEASED);
}

void MotorControllerTMC2209::setMicrostepMode(uint16_t microsteps) {
    setMicrosteps(microsteps);
}

uint16_t MotorControllerTMC2209::getMicrostepMode() const {
    return getMicrosteps();
}

bool MotorControllerTMC2209::runForDegrees(float degrees, uint16_t stepsPerRevolution) {
    if (currentState.mode == MotorMode::RUNNING && degreeRotationActive) {
        Serial.println("[MOTOR] WARNING: Already running for degrees, ignoring request");
        return false;
    }
    
    if (stepsPerRevolution == 0) {
        Serial.println("[MOTOR] ERROR: Invalid stepsPerRevolution (must be > 0)");
        return false;
    }
    
    // Calculate total steps needed including microstepping
    float totalStepsFloat = (degrees / 360.0f) * stepsPerRevolution * currentState.microsteps;
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

void MotorControllerTMC2209::setMicrosteps(uint16_t microsteps) {
    uint16_t validatedMicrosteps = validateMicrosteps(microsteps);
    
    if (currentState.microsteps != validatedMicrosteps) {
        currentState.microsteps = validatedMicrosteps;
        setMicrostepPins(currentState.microsteps);
        Serial.printf("[MOTOR] Microstepping changed to: %u\n", currentState.microsteps);
    }
}

uint16_t MotorControllerTMC2209::getMicrosteps() const {
    return currentState.microsteps;
}

void MotorControllerTMC2209::setFrequency(uint32_t frequency) {
    uint32_t validatedFrequency = validateFrequency(frequency);
    
    if (currentState.frequency != validatedFrequency) {
        Serial.printf("[MOTOR] Frequency: %u -> %u Hz\n", currentState.frequency, validatedFrequency);
        currentState.frequency = validatedFrequency;
    }
}

uint32_t MotorControllerTMC2209::getFrequency() const {
    return currentState.frequency;
}

void MotorControllerTMC2209::setDirection(bool clockwise) {
    currentState.direction = clockwise;
    digitalWrite(DIR_PIN, currentState.direction ? HIGH : LOW);
}

bool MotorControllerTMC2209::getDirection() const {
    return currentState.direction;
}

void MotorControllerTMC2209::setMode(MotorMode mode) {
    if (currentState.mode != mode) {
        Serial.printf("[MOTOR] Mode: %d -> %d\n", (int)currentState.mode, (int)mode);
        currentState.mode = mode;
        
        if (mode == MotorMode::RELEASED) {
            stopStepTask();
            digitalWrite(EN_PIN, HIGH);
            digitalWrite(STEP_PIN, LOW);
            degreeRotationActive = false;
            Serial.println("[MOTOR] Motor released");
        } else if (mode == MotorMode::STOPPED) {
            stopStepTask();
            digitalWrite(EN_PIN, LOW);
            digitalWrite(STEP_PIN, LOW);
            degreeRotationActive = false;
            Serial.println("[MOTOR] Motor stopped with holding torque");
        } else if (mode == MotorMode::RUNNING) {
            digitalWrite(EN_PIN, LOW);
            startStepTask();
            Serial.println("[MOTOR] Motor running");
        }
    }
}

MotorMode MotorControllerTMC2209::getMode() const {
    return currentState.mode;
}

MotorState MotorControllerTMC2209::getMotorState() const {
    return currentState;
}

void MotorControllerTMC2209::stepTask(void* parameter) {
    MotorControllerTMC2209* controller = static_cast<MotorControllerTMC2209*>(parameter);
    
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
            uint32_t halfPeriodUs = periodUs / 2;
            
            // Generate step pulse
            digitalWrite(STEP_PIN, HIGH);
            delayMicroseconds(kPulseWidthUs);
            digitalWrite(STEP_PIN, LOW);
            
            // Increment step counter for degree rotation
            if (controller->degreeRotationActive) {
                controller->currentSteps++;
            }
            
            // Wait for rest of half period
            if (halfPeriodUs > kPulseWidthUs) {
                delayMicroseconds(halfPeriodUs - kPulseWidthUs);
            }
            
        } else {
            // Motor not running, check every 10ms
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
    
    Serial.println("[MOTOR] Step task stopped");
    controller->stepTaskHandle = nullptr;
    vTaskDelete(nullptr);
}

void MotorControllerTMC2209::startStepTask() {
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

void MotorControllerTMC2209::stopStepTask() {
    if (stepTaskHandle != nullptr) {
        taskRunning = false;
        // Wait for task to finish
        while (stepTaskHandle != nullptr) {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        Serial.println("[MOTOR] Step task stopped");
    }
}

void MotorControllerTMC2209::setMicrostepPins(uint16_t microsteps) {
    bool ms1State = LOW;
    bool ms2State = LOW;
    
    switch (microsteps) {
        case 2:
            ms1State = HIGH;
            ms2State = LOW;
            break;
        case 4:
            ms1State = LOW; 
            ms2State = HIGH;
            break;
        case 8:
            ms1State = LOW;
            ms2State = LOW;
            break;
        case 16:
            ms1State = HIGH;
            ms2State = HIGH;
            break;
        default:
            ms1State = LOW;
            ms2State = LOW;
            Serial.printf("[MOTOR] WARNING: Unsupported microsteps %u, using 8\n", microsteps);
            break;
    }
    
    digitalWrite(MS1_PIN, ms1State);
    delayMicroseconds(10);
    digitalWrite(MS2_PIN, ms2State);
    delayMicroseconds(10);
    
    Serial.printf("[MOTOR] Microstepping pins set - MS1: %s, MS2: %s (%u microsteps)\n",
                 ms1State ? "HIGH" : "LOW", 
                 ms2State ? "HIGH" : "LOW",
                 microsteps);
}

void MotorControllerTMC2209::setPinStates() {
    // Set direction pin
    digitalWrite(DIR_PIN, currentState.direction ? HIGH : LOW);
    
    // Enable driver for holding torque (will be controlled by updateRMT)
    digitalWrite(EN_PIN, LOW);
    
    // Set microstepping pins
    setMicrostepPins(currentState.microsteps);
}

uint16_t MotorControllerTMC2209::validateMicrosteps(uint16_t requested) {
    // Check if requested microsteps is in allowed list
    for (size_t i = 0; i < kNumAllowedMicrosteps; ++i) {
        if (kAllowedMicrosteps[i] == requested) {
            return requested;
        }
    }
    
    // Return default if not found
    Serial.printf("[MOTOR] Invalid microsteps %u, using default %u\n", requested, kDefaultMicrosteps);
    return kDefaultMicrosteps;
}

uint32_t MotorControllerTMC2209::validateFrequency(uint32_t requested) {
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

bool MotorControllerTMC2209::isRunning() const {
    return currentState.mode == MotorMode::RUNNING && currentState.frequency > 0;
}
