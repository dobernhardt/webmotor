#include "motor_controller.h"
#include "config.h"
#include "state.h"
#include <Arduino.h>
#include <driver/rmt.h>
#include <soc/rtc.h>

namespace {
constexpr uint16_t kAllowedMicrosteps[] = {1, 2, 4, 8, 16};
constexpr uint16_t kDefaultMicrosteps = kAllowedMicrosteps[0];
constexpr uint32_t kDefaultFrequency = 0;
constexpr uint32_t kMaxFrequency = 10000;
constexpr bool kDefaultDirection = true;
constexpr MotorMode kDefaultMode = MotorMode::STOPPED;
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
    rmt_config_t config = {};
    config.channel = rmtChannel;
    config.gpio_num = static_cast<gpio_num_t>(STEP_PIN);
    config.mem_block_num = 1;
    config.clk_div = kRmtClockDiv;
    config.rmt_mode = RMT_MODE_TX;
    config.tx_config.loop_en = false;
    config.tx_config.idle_output_en = true;
    config.tx_config.idle_level = RMT_IDLE_LEVEL_LOW;
    config.tx_config.carrier_en = false;

    rmt_config(&config);
    rmt_driver_install(rmtChannel, 0, 0);
}

void MotorController::updateRMT() {
    if (currentState.mode == MotorMode::RELEASED || currentState.frequency == 0) {
        rmt_tx_stop(rmtChannel);
        digitalWrite(EN_PIN, HIGH);
        return;
    }

    digitalWrite(EN_PIN, LOW);

    if (currentState.mode != MotorMode::RUNNING) {
        rmt_tx_stop(rmtChannel);
        return;
    }

    const uint32_t baseClock = APB_CLK_FREQ / kRmtClockDiv;
    uint32_t halfPeriod = baseClock / (currentState.frequency * 2U);
    if (halfPeriod == 0) {
        halfPeriod = 1;
    }

    rmt_item32_t pulse = {};
    pulse.level0 = 1;
    pulse.duration0 = halfPeriod;
    pulse.level1 = 0;
    pulse.duration1 = halfPeriod;

    rmt_write_items(rmtChannel, &pulse, 1, true);
    rmt_tx_start(rmtChannel, true);
}