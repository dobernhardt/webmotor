#include "motor_controller.h"
#include "config.h"
#include "state.h"
#include <Arduino.h>
#include <driver/rmt.h>
#include <soc/rtc.h>

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
    // RMT configuration for 1 MHz base clock
    constexpr uint8_t kRmtClockDiv = 80;
    constexpr uint32_t kRmtBaseClock = APB_CLK_FREQ / kRmtClockDiv;  // 1 MHz
    // RMT timing limits for stable operation
    constexpr uint16_t kMinRmtTicks = 4;
    constexpr uint16_t kMaxRmtTicks = 32000;  // Conservative for ATOM S3 Lite
}  // namespace

MotorController::MotorController()
    : currentState{ kDefaultMicrosteps, kDefaultFrequency, kDefaultDirection, kDefaultMode },
      rmtChannel(RMT_CHANNEL_0),
      rmtConfigured(false) {
}

void MotorController::begin() {
    Serial.println("[MOTOR] Initializing motor controller");
    
    // Configure all pins
    pinMode(DIR_PIN, OUTPUT);
    pinMode(EN_PIN, OUTPUT);
    pinMode(MS1_PIN, OUTPUT);
    pinMode(MS2_PIN, OUTPUT);
    
    // Set initial pin states
    setPinStates();
    Serial.println("[MOTOR] Pins configured");
    
    // Configure RMT hardware
    configureRMT();
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
    // Clamp frequency to safe range for ATOM S3 Lite
    uint32_t validatedFrequency = validateFrequency(frequency);
    
    if (currentState.frequency != validatedFrequency) {
        Serial.printf("[MOTOR] Frequency: %u -> %u Hz\n", currentState.frequency, validatedFrequency);
        currentState.frequency = validatedFrequency;
        
        // Only update RMT if motor is running
        if (currentState.mode == MotorMode::RUNNING) {
            updateRMT();
        }
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
        updateRMT();
    }
}

MotorMode MotorController::getMode() const {
    return currentState.mode;
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
    
    // Stop RMT transmission cleanly
    rmt_tx_stop(rmtChannel);
    
    // Wait for hardware to settle on ATOM S3 Lite
    vTaskDelay(pdMS_TO_TICKS(5));
    
    if (currentState.mode == MotorMode::RELEASED) {
        digitalWrite(EN_PIN, HIGH);
        Serial.println("[MOTOR] Motor released");
        return;
    }

    digitalWrite(EN_PIN, LOW);

    if (currentState.mode == MotorMode::STOPPED || currentState.frequency == 0) {
        Serial.println("[MOTOR] Motor stopped with holding torque");
        return;
    }

    // Calculate timing with bounds checking
    const uint32_t baseClock = 1000000; // 1MHz from kRmtClockDiv = 80
    uint32_t totalTicks = baseClock / currentState.frequency;
    
    // Clamp to RMT hardware limits
    if (totalTicks < 4) totalTicks = 4;      // Minimum for stable operation
    if (totalTicks > 32766) totalTicks = 32766; // Max RMT duration
    
    uint32_t halfPeriod = totalTicks / 2;
    
    // Create single pulse item
    rmt_item32_t pulse;
    pulse.level0 = 1;
    pulse.duration0 = static_cast<uint16_t>(halfPeriod);
    pulse.level1 = 0;
    pulse.duration1 = static_cast<uint16_t>(halfPeriod);

    // Use blocking write for reliability
    esp_err_t result = rmt_write_items(rmtChannel, &pulse, 1, false);
    if (result != ESP_OK) {
        Serial.printf("[MOTOR] ERROR: RMT write failed: %d\n", result);
        return;
    }
    
    // Start with repeat enabled for continuous stepping
    result = rmt_tx_start(rmtChannel, true);
    if (result != ESP_OK) {
        Serial.printf("[MOTOR] ERROR: RMT start failed: %d\n", result);
    }
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