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
    pinMode(DIR_PIN, OUTPUT);
    digitalWrite(DIR_PIN, currentState.direction ? HIGH : LOW);
    pinMode(EN_PIN, OUTPUT);
    digitalWrite(EN_PIN, LOW);
    configureRMT();
    updateRMT();
}

void MotorController::reset() {
    currentState.microsteps = kDefaultMicrosteps;
    currentState.frequency = kDefaultFrequency;
    currentState.direction = kDefaultDirection;
    currentState.mode = kDefaultMode;
    digitalWrite(DIR_PIN, currentState.direction ? HIGH : LOW);
    digitalWrite(EN_PIN, LOW);
    updateRMT();
}

void MotorController::setMicrosteps(uint16_t microsteps) {
    bool valid = false;
    for (size_t i = 0; i < (sizeof(kAllowedMicrosteps) / sizeof(uint16_t)); ++i) {
        if (kAllowedMicrosteps[i] == microsteps) {
            valid = true;
            break;
        }
    }
    currentState.microsteps = valid ? microsteps : kDefaultMicrosteps;
    updateRMT();
}

uint16_t MotorController::getMicrosteps() const {
    return currentState.microsteps;
}

void MotorController::setFrequency(uint32_t frequency) {
    if (frequency <= kMaxFrequency) {
        currentState.frequency = frequency;
        updateRMT();
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
    currentState.mode = mode;
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
    // If the motor should be released or the target speed is zero, stop sending pulses
    // and disable the stepper driver (EN high disables most drivers).
    if (currentState.mode == MotorMode::RELEASED || currentState.frequency == 0) {
        rmt_tx_stop(rmtChannel);
        digitalWrite(EN_PIN, HIGH);
        return;
    }

    // Make sure the driver is enabled while we may emit pulses.
    digitalWrite(EN_PIN, LOW);

    // Modes like STOPPED keep the driver enabled but do not emit pulses.
    if (currentState.mode != MotorMode::RUNNING) {
        rmt_tx_stop(rmtChannel);
        return;
    }

    // Compute the pulse timing: APB clock divided by the RMT clock divider gives the
    // timer tick rate (1 MHz here). Half the period is the high (and low) duration.
    const uint32_t baseClock = APB_CLK_FREQ / kRmtClockDiv;
    uint32_t halfPeriod = baseClock / (currentState.frequency * 2U);
    if (halfPeriod == 0) {
        halfPeriod = 1;  // Clamp to the shortest possible pulse supported by hardware.
    }

    // Prepare a single RMT item: first level high, then low, both with the same duration.
    // The RMT peripheral will loop this item and generate evenly spaced step pulses.
    rmt_item32_t pulse = {};
    pulse.level0 = 1;
    pulse.duration0 = halfPeriod;
    pulse.level1 = 0;
    pulse.duration1 = halfPeriod;

    // Write the pulse template to the RMT channel and start transmission in continuous mode.
    rmt_write_items(rmtChannel, &pulse, 1, true);
    rmt_tx_start(rmtChannel, true);
}