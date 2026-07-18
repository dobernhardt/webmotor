#include "platform_controller.h"
#include "config.h"
#include <math.h>

namespace {
    constexpr float kDeadzone = 0.05f;
    // Steps per degree of platform angle, gear reduction included (config.h)
    constexpr float kRotationStepsPerDeg = ROTATION_STEPS_PER_PLATFORM_DEG;
    constexpr float kTiltStepsPerDeg = TILT_STEPS_PER_PLATFORM_DEG;
    constexpr uint64_t kAxisPeriodUs = 1000000ULL / AXIS_STEP_FREQUENCY;
}

PlatformController::PlatformController(MotorControllerULN2003& rotationMotor, MotorControllerULN2003& tiltMotor)
    : rotationMotor_(rotationMotor),
      tiltMotor_(tiltMotor),
      rotationTimer_(nullptr),
      tiltTimer_(nullptr),
      rotationTarget_(0),
      tiltTarget_(0),
      targetX_(0.0f),
      targetY_(0.0f),
      rotationLimitDeg_(DEFAULT_ROTATION_LIMIT_DEG),
      tiltLimitDeg_(DEFAULT_TILT_LIMIT_DEG) {
}

void PlatformController::begin() {
    Serial.println("[PLATFORM] Initializing platform controller");

    rotationMotor_.begin();
    tiltMotor_.begin();

    // Power-on position is defined as center. Energize both motors right
    // away: the tilt axis carries load and must hold from the first moment.
    rotationMotor_.holdCurrent();
    tiltMotor_.holdCurrent();

    esp_timer_create_args_t rotationArgs = {};
    rotationArgs.callback = &PlatformController::rotationTimerCallback;
    rotationArgs.arg = this;
    rotationArgs.name = "rotationStep";
    ESP_ERROR_CHECK(esp_timer_create(&rotationArgs, &rotationTimer_));

    esp_timer_create_args_t tiltArgs = {};
    tiltArgs.callback = &PlatformController::tiltTimerCallback;
    tiltArgs.arg = this;
    tiltArgs.name = "tiltStep";
    ESP_ERROR_CHECK(esp_timer_create(&tiltArgs, &tiltTimer_));

    // Both axes run at a fixed rate and idle when position == target
    ESP_ERROR_CHECK(esp_timer_start_periodic(rotationTimer_, kAxisPeriodUs));
    ESP_ERROR_CHECK(esp_timer_start_periodic(tiltTimer_, kAxisPeriodUs));

    Serial.printf("[PLATFORM] Ready (rotationLimit=%.1f deg, tiltLimit=%.1f deg, "
                  "%.2f/%.2f steps per platform deg, %u steps/s)\n",
                  rotationLimitDeg_, tiltLimitDeg_,
                  kRotationStepsPerDeg, kTiltStepsPerDeg, (unsigned)AXIS_STEP_FREQUENCY);
}

void PlatformController::setTarget(float x, float y) {
    x = constrain(x, -1.0f, 1.0f);
    y = constrain(y, -1.0f, 1.0f);
    if (fabsf(x) < kDeadzone) x = 0.0f;
    if (fabsf(y) < kDeadzone) y = 0.0f;

    targetX_ = x;
    targetY_ = y;

    rotationTarget_ = lroundf(x * (float)rotationLimitSteps());
    tiltTarget_ = lroundf(y * (float)tiltLimitSteps());
}

void PlatformController::stop() {
    Serial.println("[PLATFORM] Stop requested - freezing both axes");
    rotationTarget_ = rotationMotor_.getPosition();
    tiltTarget_ = tiltMotor_.getPosition();

    const int32_t rotationLimit = rotationLimitSteps();
    const int32_t tiltLimit = tiltLimitSteps();
    targetX_ = (rotationLimit > 0) ? (float)rotationTarget_ / (float)rotationLimit : 0.0f;
    targetY_ = (tiltLimit > 0) ? (float)tiltTarget_ / (float)tiltLimit : 0.0f;
}

void PlatformController::centerAxes() {
    Serial.println("[PLATFORM] Axes centered (current position = 0 deg)");
    rotationTarget_ = 0;
    tiltTarget_ = 0;
    targetX_ = 0.0f;
    targetY_ = 0.0f;
    rotationMotor_.resetPosition();
    tiltMotor_.resetPosition();
    rotationMotor_.holdCurrent();
    tiltMotor_.holdCurrent();
}

bool PlatformController::setLimits(float rotationLimitDeg, float tiltLimitDeg) {
    if (rotationLimitDeg < 0.0f || rotationLimitDeg > MAX_ROTATION_LIMIT_DEG) {
        Serial.printf("[PLATFORM] Invalid rotation limit: %.1f\n", rotationLimitDeg);
        return false;
    }
    if (tiltLimitDeg < 0.0f || tiltLimitDeg > MAX_TILT_LIMIT_DEG) {
        Serial.printf("[PLATFORM] Invalid tilt limit: %.1f\n", tiltLimitDeg);
        return false;
    }

    rotationLimitDeg_ = rotationLimitDeg;
    tiltLimitDeg_ = tiltLimitDeg;

    // Re-apply the current joystick target under the new limits
    const int32_t rotationLimit = rotationLimitSteps();
    const int32_t tiltLimit = tiltLimitSteps();
    rotationTarget_ = constrain((int32_t)lroundf(targetX_ * (float)rotationLimit), -rotationLimit, rotationLimit);
    tiltTarget_ = constrain((int32_t)lroundf(targetY_ * (float)tiltLimit), -tiltLimit, tiltLimit);

    Serial.printf("[PLATFORM] Limits updated: rotation=%.1f deg, tilt=%.1f deg\n",
                  rotationLimitDeg_, tiltLimitDeg_);
    return true;
}

PlatformStatus PlatformController::getStatus() const {
    PlatformStatus status;
    status.x = targetX_;
    status.y = targetY_;
    status.rotationDeg = (float)rotationMotor_.getPosition() / kRotationStepsPerDeg;
    status.tiltDeg = (float)tiltMotor_.getPosition() / kTiltStepsPerDeg;
    status.rotationLimitDeg = rotationLimitDeg_;
    status.tiltLimitDeg = tiltLimitDeg_;
    status.moving = (rotationMotor_.getPosition() != rotationTarget_) ||
                    (tiltMotor_.getPosition() != tiltTarget_);
    return status;
}

void PlatformController::rotationTimerCallback(void* arg) {
    PlatformController* self = static_cast<PlatformController*>(arg);
    int32_t position = self->rotationMotor_.getPosition();
    int32_t target = self->rotationTarget_;
    if (position == target) {
        return;
    }
    self->rotationMotor_.stepOnce(target > position);
}

void PlatformController::tiltTimerCallback(void* arg) {
    PlatformController* self = static_cast<PlatformController*>(arg);
    int32_t position = self->tiltMotor_.getPosition();
    int32_t target = self->tiltTarget_;
    if (position == target) {
        return;
    }
    self->tiltMotor_.stepOnce(target > position);
}

int32_t PlatformController::rotationLimitSteps() const {
    return (int32_t)lroundf(rotationLimitDeg_ * kRotationStepsPerDeg);
}

int32_t PlatformController::tiltLimitSteps() const {
    return (int32_t)lroundf(tiltLimitDeg_ * kTiltStepsPerDeg);
}
