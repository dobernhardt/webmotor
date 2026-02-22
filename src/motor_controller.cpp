#include "motor_controller.h"
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

MotorController::MotorController()
    : currentState{ kDefaultMicrosteps, kDefaultFrequency, kDefaultDirection, kDefaultMode },
      stepTaskHandle(nullptr),
      taskRunning(false) {
}

void MotorController::begin() {
    Serial.println("[MOTOR] Initializing motor controller");
    
    // Configure all pins
    pinMode(DIR_PIN, OUTPUT);
    pinMode(EN_PIN, OUTPUT);
    pinMode(MS1_PIN, OUTPUT);
    pinMode(MS2_PIN, OUTPUT);
    pinMode(STEP_PIN, OUTPUT);
    
    // Set initial pin states
    setPinStates();
    digitalWrite(STEP_PIN, LOW);
    
    Serial.println("[MOTOR] Motor controller initialized");
}


void MotorController::setMicrosteps(uint16_t microsteps) {
    uint16_t validatedMicrosteps = validateMicrosteps(microsteps);
    
    if (currentState.microsteps != validatedMicrosteps) {
        currentState.microsteps = validatedMicrosteps;
        setMicrostepPins(currentState.microsteps);
        Serial.printf("[MOTOR] Microstepping changed to: %u\n", currentState.microsteps);
    }
}

uint16_t MotorController::getMicrosteps() const {
    return currentState.microsteps;
}

void MotorController::setFrequency(uint32_t frequency) {
    uint32_t validatedFrequency = validateFrequency(frequency);
    
    if (currentState.frequency != validatedFrequency) {
        Serial.printf("[MOTOR] Frequency: %u -> %u Hz\n", currentState.frequency, validatedFrequency);
        currentState.frequency = validatedFrequency;
    }
}

uint32_t MotorController::getFrequency() const {
    return currentState.frequency;
}

void MotorController::setDirection(bool clockwise) {
    currentState.direction = clockwise;
    digitalWrite(DIR_PIN, currentState.direction ? HIGH : LOW);
}

bool MotorController::getDirection() const {
    return currentState.direction;
}

void MotorController::setMode(MotorMode mode) {
    if (currentState.mode != mode) {
        Serial.printf("[MOTOR] Mode: %d -> %d\n", (int)currentState.mode, (int)mode);
        currentState.mode = mode;
        
        if (mode == MotorMode::RELEASED) {
            stopStepTask();
            digitalWrite(EN_PIN, HIGH);
            digitalWrite(STEP_PIN, LOW);
            Serial.println("[MOTOR] Motor released");
        } else if (mode == MotorMode::STOPPED) {
            stopStepTask();
            digitalWrite(EN_PIN, LOW);
            digitalWrite(STEP_PIN, LOW);
            Serial.println("[MOTOR] Motor stopped with holding torque");
        } else if (mode == MotorMode::RUNNING) {
            digitalWrite(EN_PIN, LOW);
            startStepTask();
            Serial.println("[MOTOR] Motor running");
        }
    }
}

MotorMode MotorController::getMode() const {
    return currentState.mode;
}

MotorState MotorController::getMotorState() const {
    return currentState;
}

void MotorController::stepTask(void* parameter) {
    MotorController* controller = static_cast<MotorController*>(parameter);
    
    Serial.println("[MOTOR] Step task started");
    
    while (controller->taskRunning) {
        if (controller->currentState.mode == MotorMode::RUNNING && 
            controller->currentState.frequency > 0) {
            
            // Calculate delay between steps in microseconds
            uint32_t periodUs = 1000000UL / controller->currentState.frequency;
            uint32_t halfPeriodUs = periodUs / 2;
            
            // Generate step pulse
            digitalWrite(STEP_PIN, HIGH);
            delayMicroseconds(kPulseWidthUs);
            digitalWrite(STEP_PIN, LOW);
            
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

void MotorController::startStepTask() {
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

void MotorController::stopStepTask() {
    if (stepTaskHandle != nullptr) {
        taskRunning = false;
        // Wait for task to finish
        while (stepTaskHandle != nullptr) {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        Serial.println("[MOTOR] Step task stopped");
    }
}

void MotorController::setMicrostepPins(uint16_t microsteps) {
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

void MotorController::setPinStates() {
    // Set direction pin
    digitalWrite(DIR_PIN, currentState.direction ? HIGH : LOW);
    
    // Enable driver for holding torque (will be controlled by updateRMT)
    digitalWrite(EN_PIN, LOW);
    
    // Set microstepping pins
    setMicrostepPins(currentState.microsteps);
}

uint16_t MotorController::validateMicrosteps(uint16_t requested) {
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

uint32_t MotorController::validateFrequency(uint32_t requested) {
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

bool MotorController::isRunning() const {
    return currentState.mode == MotorMode::RUNNING && currentState.frequency > 0;
}