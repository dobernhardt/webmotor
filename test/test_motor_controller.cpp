#include <Arduino.h>
#include <unity.h>
#include "motor_controller.h"

MotorController motorController;

void setUp() {
    motorController.reset(); // Reset motor controller state before each test
}

void test_setMicrosteps() {
    motorController.setMicrosteps(1);
    TEST_ASSERT_EQUAL(1, motorController.getMicrosteps());

    motorController.setMicrosteps(16);
    TEST_ASSERT_EQUAL(16, motorController.getMicrosteps());

    motorController.setMicrosteps(0); // Invalid value
    TEST_ASSERT_EQUAL(1, motorController.getMicrosteps()); // Should remain unchanged
}

void test_setFrequency() {
    motorController.setFrequency(5000);
    TEST_ASSERT_EQUAL(5000, motorController.getFrequency());

    motorController.setFrequency(10000);
    TEST_ASSERT_EQUAL(10000, motorController.getFrequency());

    motorController.setFrequency(15000); // Invalid value
    TEST_ASSERT_EQUAL(10000, motorController.getFrequency()); // Should remain unchanged
}

void test_setDirection() {
    motorController.setDirection(true); // CW
    TEST_ASSERT_TRUE(motorController.getDirection());

    motorController.setDirection(false); // CCW
    TEST_ASSERT_FALSE(motorController.getDirection());
}

void test_setMode() {
    motorController.setMode(STOPPED);
    TEST_ASSERT_EQUAL(STOPPED, motorController.getMode());

    motorController.setMode(RUNNING);
    TEST_ASSERT_EQUAL(RUNNING, motorController.getMode());

    motorController.setMode(RELEASED);
    TEST_ASSERT_EQUAL(RELEASED, motorController.getMode());
}

void setup() {
    UNITY_BEGIN();
    RUN_TEST(test_setMicrosteps);
    RUN_TEST(test_setFrequency);
    RUN_TEST(test_setDirection);
    RUN_TEST(test_setMode);
    UNITY_END();
}

void loop() {
    // No need to implement loop for tests
}