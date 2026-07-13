#include "drive_controller.h"
#include "config.h"
#include <math.h>

namespace {
    constexpr const char* NVS_NAMESPACE = "drive";
    constexpr const char* KEY_STEER_LIMIT = "steerlimit";
    constexpr const char* KEY_MAX_FREQ = "maxfreq";

    constexpr float kDeadzone = 0.05f;
    constexpr float kStepsPerDeg = (float)STEPS_PER_REVOLUTION / 360.0f;
    constexpr uint64_t kSteerPeriodUs = 1000000ULL / STEER_FREQUENCY;
}

DriveController::DriveController(MotorControllerULN2003& driveMotor, MotorControllerULN2003& steerMotor)
    : driveMotor_(driveMotor),
      steerMotor_(steerMotor),
      driveTimer_(nullptr),
      steerTimer_(nullptr),
      driveTimerRunning_(false),
      driveForward_(true),
      driveFrequency_(0),
      steerTarget_(0),
      targetX_(0.0f),
      targetY_(0.0f),
      lastTargetMs_(0),
      failsafe_(false),
      steerLimitDeg_(DEFAULT_STEER_LIMIT_DEG),
      maxFrequency_(DEFAULT_MAX_FREQUENCY) {
}

void DriveController::begin() {
    Serial.println("[DRIVE] Initializing drive controller");

    driveMotor_.begin();
    steerMotor_.begin();
    loadConfig();

    esp_timer_create_args_t driveArgs = {};
    driveArgs.callback = &DriveController::driveTimerCallback;
    driveArgs.arg = this;
    driveArgs.name = "driveStep";
    ESP_ERROR_CHECK(esp_timer_create(&driveArgs, &driveTimer_));

    esp_timer_create_args_t steerArgs = {};
    steerArgs.callback = &DriveController::steerTimerCallback;
    steerArgs.arg = this;
    steerArgs.name = "steerStep";
    ESP_ERROR_CHECK(esp_timer_create(&steerArgs, &steerTimer_));

    // Steering runs at a fixed rate and idles when position == target
    ESP_ERROR_CHECK(esp_timer_start_periodic(steerTimer_, kSteerPeriodUs));

    Serial.printf("[DRIVE] Ready (steerLimit=%.1f deg, maxFrequency=%u Hz)\n",
                  steerLimitDeg_, maxFrequency_);
}

void DriveController::handle() {
    if (driveFrequency_ > 0 && (millis() - lastTargetMs_) > DRIVE_FAILSAFE_TIMEOUT_MS) {
        Serial.println("[DRIVE] Failsafe: no joystick update, stopping drive");
        failsafe_ = true;
        targetY_ = 0.0f;
        driveFrequency_ = 0;
        updateDriveTimer();
    }
}

void DriveController::setTarget(float x, float y) {
    x = constrain(x, -1.0f, 1.0f);
    y = constrain(y, -1.0f, 1.0f);
    if (fabsf(x) < kDeadzone) x = 0.0f;
    if (fabsf(y) < kDeadzone) y = 0.0f;

    targetX_ = x;
    targetY_ = y;
    lastTargetMs_ = millis();
    failsafe_ = false;

    steerTarget_ = lroundf(x * (float)steerLimitSteps());

    uint32_t frequency = (uint32_t)lroundf(fabsf(y) * (float)maxFrequency_);
    if (frequency < MIN_DRIVE_FREQUENCY) {
        frequency = 0;
    }
    driveForward_ = (y >= 0.0f);

    if (frequency != driveFrequency_) {
        driveFrequency_ = frequency;
        updateDriveTimer();
    }
}

void DriveController::stop() {
    Serial.println("[DRIVE] Stop requested");
    targetX_ = 0.0f;
    targetY_ = 0.0f;
    lastTargetMs_ = millis();
    driveFrequency_ = 0;
    steerTarget_ = steerMotor_.getPosition();  // freeze steering where it is
    updateDriveTimer();
}

void DriveController::centerSteering() {
    Serial.println("[DRIVE] Steering centered (current position = straight)");
    steerTarget_ = 0;
    steerMotor_.resetPosition();
    steerMotor_.holdCurrent();
}

bool DriveController::setConfig(float steerLimitDeg, uint32_t maxFrequency) {
    if (steerLimitDeg < 0.0f || steerLimitDeg > 180.0f) {
        Serial.printf("[DRIVE] Invalid steering limit: %.1f\n", steerLimitDeg);
        return false;
    }
    if (maxFrequency < MIN_DRIVE_FREQUENCY || maxFrequency > ABS_MAX_FREQUENCY) {
        Serial.printf("[DRIVE] Invalid max frequency: %u\n", maxFrequency);
        return false;
    }

    steerLimitDeg_ = steerLimitDeg;
    maxFrequency_ = maxFrequency;
    saveConfig();

    // Re-apply the current joystick target under the new limits
    int32_t limit = steerLimitSteps();
    steerTarget_ = constrain((int32_t)lroundf(targetX_ * (float)limit), -limit, limit);

    Serial.printf("[DRIVE] Config updated: steerLimit=%.1f deg, maxFrequency=%u Hz\n",
                  steerLimitDeg_, maxFrequency_);
    return true;
}

DriveStatus DriveController::getStatus() const {
    DriveStatus status;
    status.x = targetX_;
    status.y = targetY_;
    status.steeringDeg = (float)steerMotor_.getPosition() / kStepsPerDeg;
    status.steerLimitDeg = steerLimitDeg_;
    status.maxFrequency = maxFrequency_;
    status.driving = (driveFrequency_ > 0);
    status.failsafe = failsafe_;
    return status;
}

void DriveController::driveTimerCallback(void* arg) {
    DriveController* self = static_cast<DriveController*>(arg);
    self->driveMotor_.stepOnce(self->driveForward_);
}

void DriveController::steerTimerCallback(void* arg) {
    DriveController* self = static_cast<DriveController*>(arg);
    int32_t position = self->steerMotor_.getPosition();
    int32_t target = self->steerTarget_;
    if (position == target) {
        return;
    }
    self->steerMotor_.stepOnce(target > position);
}

void DriveController::updateDriveTimer() {
    if (driveTimerRunning_) {
        esp_timer_stop(driveTimer_);
        driveTimerRunning_ = false;
    }
    if (driveFrequency_ > 0) {
        ESP_ERROR_CHECK(esp_timer_start_periodic(driveTimer_, 1000000ULL / driveFrequency_));
        driveTimerRunning_ = true;
    } else {
        // No holding torque needed on the drive wheel; keep the driver cool
        driveMotor_.release();
    }
}

int32_t DriveController::steerLimitSteps() const {
    return (int32_t)lroundf(steerLimitDeg_ * kStepsPerDeg);
}

void DriveController::loadConfig() {
    preferences_.begin(NVS_NAMESPACE, true);
    steerLimitDeg_ = preferences_.getFloat(KEY_STEER_LIMIT, DEFAULT_STEER_LIMIT_DEG);
    maxFrequency_ = preferences_.getUInt(KEY_MAX_FREQ, DEFAULT_MAX_FREQUENCY);
    preferences_.end();

    steerLimitDeg_ = constrain(steerLimitDeg_, 0.0f, 180.0f);
    maxFrequency_ = constrain(maxFrequency_, (uint32_t)MIN_DRIVE_FREQUENCY, (uint32_t)ABS_MAX_FREQUENCY);
}

void DriveController::saveConfig() {
    preferences_.begin(NVS_NAMESPACE, false);
    preferences_.putFloat(KEY_STEER_LIMIT, steerLimitDeg_);
    preferences_.putUInt(KEY_MAX_FREQ, maxFrequency_);
    preferences_.end();
}
