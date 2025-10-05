#include "motor_controller.h"
#include "config.h"
#include "state.h"
#include <Arduino.h>
#include <driver/rmt.h>
#include <soc/rtc.h>

namespace {
    // Allowed microstepping ratios supported by the ATOM S3 Lite driver wiring.
    constexpr uint16_t kAllowedMicrosteps[] = {1, 2, 4, 8, 16};
    // Fallback microstepping ratio when an unsupported value is requested.
    constexpr uint16_t kDefaultMicrosteps = kAllowedMicrosteps[0];
    // Start idle to avoid unintentional motion after reset.
    constexpr uint32_t kDefaultFrequency = 0;
    // Limit the step rate so RMT timings stay within safe limits for the motor driver.
    constexpr uint32_t kMaxFrequency = 10000;
    // Default winding direction matching the reference assembly guide.
    constexpr bool kDefaultDirection = true;
    // Initialize the controller in a non-driving state for safety.
    constexpr MotorMode kDefaultMode = MotorMode::STOPPED;
    // Set RMT to 1 MHz base clock for predictable pulse generation.
    constexpr uint8_t kRmtClockDiv = 80;  // 1 MHz base clock
}  // namespace

MotorController::MotorController()
    : currentState{ kDefaultMicrosteps, kDefaultFrequency, kDefaultDirection, kDefaultMode },
      rmtChannel(RMT_CHANNEL_0) {
}

void MotorController::begin() {
    Serial.println("[MOTOR] Initializing motor controller");
    
    // Configure direction pin
    pinMode(DIR_PIN, OUTPUT);
    digitalWrite(DIR_PIN, currentState.direction ? HIGH : LOW);
    Serial.printf("[MOTOR] Direction pin set: %s\n", currentState.direction ? "HIGH (CW)" : "LOW (CCW)");
    
    // Configure enable pin
    pinMode(EN_PIN, OUTPUT);
    digitalWrite(EN_PIN, LOW);
    Serial.println("[MOTOR] Enable pin set LOW (motor enabled)");
    
    // Configure microstepping pins for TMC2209
    pinMode(MS1_PIN, OUTPUT);
    pinMode(MS2_PIN, OUTPUT);
    setMicrostepPins(currentState.microsteps);
    Serial.println("[MOTOR] Microstepping pins configured");
    
    configureRMT();
    Serial.println("[MOTOR] RMT configured");
    
    updateRMT();
    Serial.println("[MOTOR] Motor controller initialized");
}


void MotorController::setMicrosteps(uint16_t microsteps) {
    bool valid = false;
    for (size_t i = 0; i < (sizeof(kAllowedMicrosteps) / sizeof(uint16_t)); ++i) {
        if (kAllowedMicrosteps[i] == microsteps) {
            valid = true;
            break;
        }
    }
    
    uint16_t newMicrosteps = valid ? microsteps : kDefaultMicrosteps;
    
    if (currentState.microsteps != newMicrosteps) {
        currentState.microsteps = newMicrosteps;
        setMicrostepPins(currentState.microsteps);
        Serial.printf("[MOTOR] Microstepping changed to: %u\n", currentState.microsteps);
        updateRMT();
    }
}

uint16_t MotorController::getMicrosteps() const {
    return currentState.microsteps;
}

void MotorController::setFrequency(uint32_t frequency) {
    Serial.printf("[MOTOR] Setting frequency: %u Hz\n", frequency);
    
    // Clamp frequency to safe limits for ATOM S3 Lite
    if (frequency > kMaxFrequency) {
        frequency = kMaxFrequency;
        Serial.printf("[MOTOR] WARNING: Frequency clamped to %u Hz\n", frequency);
    }
    
    // Store previous state for recovery
    MotorMode previousMode = currentState.mode;
    bool wasRunning = (currentState.mode == MotorMode::RUNNING);
    
    if (currentState.frequency != frequency) {
        Serial.printf("[MOTOR] Frequency changed: %u -> %u\n", currentState.frequency, frequency);
        
        // If motor is running, we need to safely change frequency
        if (wasRunning) {
            Serial.println("[MOTOR] Safely stopping RMT for frequency change");
            
            // Temporarily stop the motor to change frequency
            currentState.mode = MotorMode::STOPPED;
            updateRMT();
            
            // Small delay for ATOM S3 Lite stability
            delay(5);
        }
        
        // Update frequency
        currentState.frequency = frequency;
        
        // If motor was running, restart it with new frequency
        if (wasRunning && frequency > 0) {
            Serial.println("[MOTOR] Restarting RMT with new frequency");
            currentState.mode = MotorMode::RUNNING;
            updateRMT();
        } else if (wasRunning && frequency == 0) {
            Serial.println("[MOTOR] Frequency set to 0, motor remains stopped");
            // Keep motor stopped if frequency is 0
        } else {
            // Motor wasn't running, just update RMT configuration
            updateRMT();
        }
    } else {
        Serial.println("[MOTOR] Frequency unchanged, no RMT update needed");
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
    MotorMode oldMode = currentState.mode;
    currentState.mode = mode;
    Serial.printf("[MOTOR] Mode changed: %d -> %d\n", static_cast<int>(oldMode), static_cast<int>(mode));
    updateRMT();
}

MotorMode MotorController::getMode() const {
    return currentState.mode;
}

void MotorController::updateMotorState() {
    updateRMT();
}

MotorState MotorController::getMotorState() const {
    return currentState;
}

void MotorController::configureRMT() {
    // Start from a zeroed configuration to avoid residual settings between runs.
    rmt_config_t config = {};
    // Use the dedicated channel reserved for the step signal.
    config.channel = rmtChannel;
    // Send pulses out on the step pin connected to the driver.
    config.gpio_num = static_cast<gpio_num_t>(STEP_PIN);
    // Allocate one memory block, enough for our repeating waveform.
    config.mem_block_num = 1;
    // Divide the APB clock down to 1 MHz for predictable pulse spacing.
    config.clk_div = kRmtClockDiv;
    // Set the peripheral to transmit mode to generate step pulses.
    config.rmt_mode = RMT_MODE_TX;
    // Disable hardware looping; software restarts the sequence when required.
    config.tx_config.loop_en = false;
    // Keep the output actively driven even when no pulses are sent.
    config.tx_config.idle_output_en = true;
    // Hold the line low when idle to prevent unintended steps.
    config.tx_config.idle_level = RMT_IDLE_LEVEL_LOW;
    // Disable carrier modulation to emit clean, direct pulses.
    config.tx_config.carrier_en = false;

    // Apply the configuration and install the driver resources.
    rmt_config(&config);
    rmt_driver_install(rmtChannel, 0, 0);
}

void MotorController::updateRMT() {
    Serial.println("[MOTOR] updateRMT() called");
    
    // For ATOM S3 Lite stability, always do a clean stop first
    Serial.println("[MOTOR] Ensuring RMT is stopped");
    rmt_tx_stop(rmtChannel);
    
    // Longer delay for embedded device stability
    delay(2);
    
    // Handle RELEASED mode - disable driver completely
    if (currentState.mode == MotorMode::RELEASED) {
        Serial.println("[MOTOR] Mode is RELEASED - disabling driver");
        digitalWrite(EN_PIN, HIGH);  // Disable driver - motor spins freely
        Serial.println("[MOTOR] Motor disabled (EN=HIGH) - shaft can rotate freely");
        return;
    }

    // For RUNNING and STOPPED modes, enable the driver to maintain holding torque
    digitalWrite(EN_PIN, LOW);
    Serial.println("[MOTOR] Motor enabled (EN=LOW) - driver active");

    // For STOPPED mode or zero frequency, keep driver enabled but no pulses
    if (currentState.mode == MotorMode::STOPPED || currentState.frequency == 0) {
        Serial.printf("[MOTOR] Mode is STOPPED or frequency is 0 - no pulses but holding torque maintained\n");
        return;
    }

    // Only reach here if mode is RUNNING and frequency > 0
    Serial.printf("[MOTOR] Starting RMT with frequency: %u Hz\n", currentState.frequency);
    
    // Calculate timing for ATOM S3 Lite
    const uint32_t baseClock = APB_CLK_FREQ / kRmtClockDiv;
    uint32_t halfPeriod = baseClock / (currentState.frequency * 2U);
    
    // Conservative clamping for embedded device stability
    if (halfPeriod == 0) {
        halfPeriod = 1;
        Serial.println("[MOTOR] WARNING: Half period clamped to 1 (frequency too high)");
    } else if (halfPeriod > 16383) {  // More conservative limit for ATOM S3 Lite
        halfPeriod = 16383;
        Serial.printf("[MOTOR] WARNING: Half period clamped to %u (frequency too low)\n", halfPeriod);
    }
    
    Serial.printf("[MOTOR] Pulse timing - Base clock: %u Hz, Half period: %u ticks\n", baseClock, halfPeriod);

    // Prepare pulse pattern
    rmt_item32_t pulse = {};
    pulse.level0 = 1;
    pulse.duration0 = halfPeriod;
    pulse.level1 = 0;
    pulse.duration1 = halfPeriod;

    Serial.println("[MOTOR] Writing new RMT pulse pattern");
    
    // Write new pattern (non-blocking for embedded stability)
    esp_err_t writeResult = rmt_write_items(rmtChannel, &pulse, 1, true);
    if (writeResult != ESP_OK) {
        Serial.printf("[MOTOR] ERROR: rmt_write_items failed with code %d\n", writeResult);
        recoverRMT();
        return;
    }
    
    // Small delay for ATOM S3 Lite
    delayMicroseconds(200);
    
    Serial.println("[MOTOR] Starting RMT transmission");
    esp_err_t startResult = rmt_tx_start(rmtChannel, true);
    if (startResult != ESP_OK) {
        Serial.printf("[MOTOR] ERROR: rmt_tx_start failed with code %d\n", startResult);
        recoverRMT();
        return;
    }
    
    Serial.printf("[MOTOR] RMT started successfully - Frequency: %u Hz, Direction: %s\n", 
                 currentState.frequency, 
                 currentState.direction ? "CW" : "CCW");
}

void MotorController::recoverRMT() {
    Serial.println("[MOTOR] Attempting RMT recovery for ATOM S3 Lite");
    
    // Stop any ongoing transmission
    rmt_tx_stop(rmtChannel);
    delay(5);
    
    // Full RMT reset sequence optimized for embedded device
    rmt_driver_uninstall(rmtChannel);
    delay(10);  // Longer delay for ATOM S3 Lite
    
    // Reconfigure RMT
    configureRMT();
    
    // Set safe state
    digitalWrite(EN_PIN, HIGH);
    currentState.mode = MotorMode::STOPPED;
    
    Serial.println("[MOTOR] RMT recovery completed, motor stopped safely");
}

void MotorController::setMicrostepPins(uint16_t microsteps) {
    bool ms1State = LOW;
    bool ms2State = LOW;
    
    // Configure MS1/MS2 pins for TMC2209 microstepping control
    // TMC2209 microstepping table:
    // MS2=0, MS1=0 -> 8 microsteps (default)
    // MS2=0, MS1=1 -> 2 microsteps
    // MS2=1, MS1=0 -> 4 microsteps
    // MS2=1, MS1=1 -> 16 microsteps
    // Note: TMC2209 doesn't support 1 microstep via MS pins
    
    switch (microsteps) {
        case 1:
            // TMC2209 doesn't support 1 microstep, fallback to 8
            ms1State = LOW;
            ms2State = LOW;
            Serial.println("[MOTOR] WARNING: 1 microstep not supported by TMC2209, using 8");
            break;
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
            ms2State = LOW;  // Default TMC2209 setting
            break;
        case 16:
            ms1State = HIGH;
            ms2State = HIGH;
            break;
        default:
            // Default to 8 microsteps for safety on educational device
            ms1State = LOW;
            ms2State = LOW;
            Serial.printf("[MOTOR] WARNING: Unsupported microsteps %u, using 8\n", microsteps);
            break;
    }
    
    // Set physical pins - small delay for embedded device stability
    digitalWrite(MS1_PIN, ms1State);
    delayMicroseconds(10);
    digitalWrite(MS2_PIN, ms2State);
    delayMicroseconds(10);
    
    Serial.printf("[MOTOR] Microstepping pins set - MS1: %s, MS2: %s (targeting %u microsteps)\n",
                 ms1State ? "HIGH" : "LOW", 
                 ms2State ? "HIGH" : "LOW",
                 microsteps);
}