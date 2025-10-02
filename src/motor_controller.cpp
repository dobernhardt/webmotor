#include "motor_controller.h"
#include "config.h"
#include "state.h"
#include <Arduino.h>
#include <driver/rmt.h>

class MotorController {
public:
    MotorController();
    void begin();
    void setMicrostepping(int microsteps);
    void setFrequency(int frequency);
    void setDirection(bool direction);
    void setMode(int mode);
    MotorState getState();

private:
    void generatePulse();
    void updateRMT();

    MotorState state;
    rmt_channel_t rmtChannel;
};

MotorController::MotorController() {
    state.microsteps = 16; // Default microstepping
    state.frequency = 0;    // Default frequency
    state.direction = true;  // Default direction (CW)
    state.mode = 0;          // Default mode (STOPPED)
    rmtChannel = RMT_CHANNEL_0; // Assign RMT channel
}

void MotorController::begin() {
    // Initialize RMT for pulse generation
    rmt_config_t rmtConfig;
    rmtConfig.channel = rmtChannel;
    rmtConfig.gpio_num = STEP_PIN; // STEP pin from config.h
    rmtConfig.mem_block_num = 1;
    rmtConfig.rmt_mode = RMT_MODE_TX;
    rmtConfig.clk_div = RMT_CLK_DIV; // Set clock divider
    rmtConfig.tx_config.carrier_en = RMT_CARRIER_DISABLE;
    rmtConfig.tx_config.idle_level = RMT_IDLE_LEVEL_LOW;
    rmtConfig.tx_config.idle_output_en = RMT_IDLE_OUTPUT_ENABLE;

    rmt_config(&rmtConfig);
    rmt_driver_install(rmtChannel, 0, 0);
}

void MotorController::setMicrostepping(int microsteps) {
    if (microsteps == 1 || microsteps == 2 || microsteps == 4 || microsteps == 8 || microsteps == 16) {
        state.microsteps = microsteps;
        // Update MS1 and MS2 pins based on microsteps
        digitalWrite(MS1_PIN, (microsteps > 1) ? HIGH : LOW);
        digitalWrite(MS2_PIN, (microsteps > 4) ? HIGH : LOW);
    }
}

void MotorController::setFrequency(int frequency) {
    if (frequency >= 0 && frequency <= 10000) {
        state.frequency = frequency;
        updateRMT();
    }
}

void MotorController::setDirection(bool direction) {
    state.direction = direction;
    digitalWrite(DIR_PIN, direction ? HIGH : LOW);
}

void MotorController::setMode(int mode) {
    state.mode = mode;
    if (mode == 1) { // RUNNING
        generatePulse();
    } else {
        rmt_disable(rmtChannel); // Stop pulses
    }
}

MotorState MotorController::getState() {
    return state;
}

void MotorController::generatePulse() {
    // Implement pulse generation logic using RMT
    // This function will be called when the motor is set to RUNNING mode
}

void MotorController::updateRMT() {
    // Update RMT configuration based on the current frequency
    // Ensure the pulse width and timing are correct
}