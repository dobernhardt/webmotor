#ifndef DUAL_STEPPER_CONTROLLER_H
#define DUAL_STEPPER_CONTROLLER_H

#include <Arduino.h>
#include "AbstractMotorController.h"
#include "state.h"

/**
 * Dual stepper controller using 4-wire half-step sequence
 * Motor X: GPIO 5,6,7,8
 * Motor Y: GPIO 7,8,9,10
 */
class DualStepperController : public AbstractMotorController {
private:
    // Motor X pins
    const uint8_t xPins[4] = {5, 6, 7, 8};

    // Motor Y pins (shares 7,8 with X as requested)
    const uint8_t yPins[4] = {7, 8, 9, 10};

    // Half-step sequence (8 states)
    const uint8_t halfStepSequence[8][4] = {
        {1,0,0,0},
        {1,1,0,0},
        {0,1,0,0},
        {0,1,1,0},
        {0,0,1,0},
        {0,0,1,1},
        {0,0,0,1},
        {1,0,0,1}
    };

    uint8_t stepIndex = 0;
    bool running = false;
    bool directionCW = true;

    uint32_t frequencyHz = 100;
    uint32_t lastStepMicros = 0;

    uint16_t microsteps = 2;

    MotorState state;

    void applyStep(const uint8_t pins[4], uint8_t index) {
        for (int i = 0; i < 4; i++) {
            digitalWrite(pins[i], halfStepSequence[index][i]);
        }
    }

    void stepMotor() {
        if (!running) return;

        uint32_t interval = 1000000UL / frequencyHz;
        if (micros() - lastStepMicros < interval) return;

        lastStepMicros = micros();

        if (directionCW) {
            stepIndex = (stepIndex + 1) % 8;
        } else {
            stepIndex = (stepIndex == 0) ? 7 : stepIndex - 1;
        }

        applyStep(xPins, stepIndex);
        applyStep(yPins, stepIndex);

        state.position += directionCW ? 1 : -1;
        state.moving = true;
    }

public:
    void begin() override {
        for (int i = 0; i < 4; i++) {
            pinMode(xPins[i], OUTPUT);
            pinMode(yPins[i], OUTPUT);
        }

        release();
    }

    void start() override {
        running = true;
        state.enabled = true;
    }

    void stop() override {
        running = false;
        state.moving = false;

        // hold last state (no release)
    }

    void release() override {
        running = false;
        state.enabled = false;
        state.moving = false;

        for (int i = 0; i < 4; i++) {
            digitalWrite(xPins[i], LOW);
            digitalWrite(yPins[i], LOW);
        }
    }

    void setFrequency(uint32_t frequencyHz) override {
        this->frequencyHz = max(1UL, frequencyHz);
    }

    uint32_t getFrequency() const override {
        return frequencyHz;
    }

    void setMicrostepMode(uint16_t microsteps) override {
        this->microsteps = microsteps;
    }

    uint16_t getMicrostepMode() const override {
        return microsteps;
    }

    void setDirection(bool clockwise) override {
        directionCW = clockwise;
    }

    bool getDirection() const override {
        return directionCW;
    }

    bool runForDegrees(float degrees, uint16_t stepsPerRevolution) override {
        // simplified implementation
        int steps = (int)((degrees / 360.0f) * stepsPerRevolution);

        for (int i = 0; i < abs(steps); i++) {
            directionCW = (steps > 0);
            stepMotor();
            delayMicroseconds(1000);
        }

        return true;
    }

    MotorState getMotorState() const override {
        return state;
    }

    bool isRunning() const override {
        return running;
    }

    /**
     * Must be called frequently in loop()
     */
    void update() {
        stepMotor();
    }
};

#endif