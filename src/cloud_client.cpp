#include "cloud_client.h"
#include <ArduinoJson.h>
#include <WiFi.h>

namespace {
constexpr const char* NVS_NAMESPACE = "cloud";
constexpr const char* KEY_ENDPOINT = "endpoint";
constexpr const char* KEY_API_KEY = "apikey";
constexpr const char* KEY_ENABLED = "enabled";
constexpr int HTTP_TIMEOUT_MS = 10000;      // state push / generic requests
constexpr int SYNC_HTTP_TIMEOUT_MS = 5000;  // sync endpoint answers immediately
}

CloudClient::CloudClient()
    : enabled_(false),
      currentStatus_{},
      lastStatePush_(0),
      hasTarget_(false),
      targetX_(0.0f),
      targetY_(0.0f),
      hasLimitsUpdate_(false),
      cloudRotationLimitDeg_(0.0f),
      cloudTiltLimitDeg_(0.0f),
      cloudLimitsSeen_(false),
      lastSeenRotationLimitDeg_(0.0f),
      lastSeenTiltLimitDeg_(0.0f),
      hasPendingCommand_(false),
      targetActive_(false),
      syncTaskHandle_(nullptr),
      syncTaskRunning_(false) {
    syncMutex_ = xSemaphoreCreateMutex();
}

void CloudClient::begin() {
    Serial.println("[CLOUD] Initializing cloud client...");
    loadConfig();

    if (enabled_ && !apiEndpoint_.isEmpty() && !apiKey_.isEmpty()) {
        Serial.println("[CLOUD] Cloud sync enabled");
        Serial.print("[CLOUD] Endpoint: ");
        Serial.println(apiEndpoint_);
        startSyncTask();
    } else {
        Serial.println("[CLOUD] Cloud sync disabled");
    }
}

void CloudClient::handle() {
    if (!enabled_ || apiEndpoint_.isEmpty() || apiKey_.isEmpty()) {
        return;
    }

    if (WiFi.status() != WL_CONNECTED) {
        return;
    }

    unsigned long now = millis();
    if (now - lastStatePush_ >= STATE_PUSH_INTERVAL) {
        pushState();
        lastStatePush_ = now;
    }
}

void CloudClient::setEnabled(bool enabled) {
    if (enabled_ == enabled) {
        return;
    }

    enabled_ = enabled;
    saveConfig();

    if (enabled && !apiEndpoint_.isEmpty() && !apiKey_.isEmpty()) {
        Serial.println("[CLOUD] Enabling cloud sync...");
        startSyncTask();
    } else {
        Serial.println("[CLOUD] Disabling cloud sync...");
        stopSyncTask();
    }
}

void CloudClient::getConfig(String& apiEndpoint, String& apiKey, bool& enabled) const {
    apiEndpoint = apiEndpoint_;
    apiKey = apiKey_;
    enabled = enabled_;
}

bool CloudClient::setConfig(const String& apiEndpoint, const String& apiKey, bool enabled) {
    apiEndpoint_ = apiEndpoint;
    apiKey_ = apiKey;

    const bool wasEnabled = enabled_;
    enabled_ = enabled;
    bool result = saveConfig();

    if (enabled_ && !wasEnabled && !apiEndpoint_.isEmpty() && !apiKey_.isEmpty()) {
        startSyncTask();
    } else if (!enabled_ && wasEnabled) {
        stopSyncTask();
    }

    return result;
}

bool CloudClient::testConnection() {
    if (apiEndpoint_.isEmpty() || apiKey_.isEmpty()) {
        Serial.println("[CLOUD] Cannot test connection - configuration incomplete");
        return false;
    }

    Serial.println("[CLOUD] Testing connection...");

    http_.begin(apiEndpoint_ + "/health");
    http_.setTimeout(5000);

    int httpCode = http_.GET();
    http_.end();

    if (httpCode == HTTP_CODE_OK) {
        Serial.println("[CLOUD] Connection test successful");
        return true;
    } else {
        Serial.print("[CLOUD] Connection test failed. HTTP code: ");
        Serial.println(httpCode);
        return false;
    }
}

void CloudClient::setStatus(const PlatformStatus& status) {
    currentStatus_ = status;
}

bool CloudClient::getTarget(float& x, float& y) {
    if (!hasTarget_) {
        return false;
    }

    bool result = false;
    if (xSemaphoreTake(syncMutex_, pdMS_TO_TICKS(50)) == pdTRUE) {
        if (hasTarget_) {
            x = targetX_;
            y = targetY_;
            hasTarget_ = false;
            result = true;
        }
        xSemaphoreGive(syncMutex_);
    }
    return result;
}

bool CloudClient::getLimitsUpdate(float& rotationLimitDeg, float& tiltLimitDeg) {
    if (!hasLimitsUpdate_) {
        return false;
    }

    bool result = false;
    if (xSemaphoreTake(syncMutex_, pdMS_TO_TICKS(50)) == pdTRUE) {
        if (hasLimitsUpdate_) {
            rotationLimitDeg = cloudRotationLimitDeg_;
            tiltLimitDeg = cloudTiltLimitDeg_;
            hasLimitsUpdate_ = false;
            result = true;
        }
        xSemaphoreGive(syncMutex_);
    }
    return result;
}

String CloudClient::getCommand() {
    if (!hasPendingCommand_) {
        return "";
    }

    if (xSemaphoreTake(syncMutex_, portMAX_DELAY) == pdTRUE) {
        hasPendingCommand_ = false;
        String cmd = pendingCommand_;
        pendingCommand_ = "";
        xSemaphoreGive(syncMutex_);
        return cmd;
    }

    return "";
}

void CloudClient::loadConfig() {
    preferences_.begin(NVS_NAMESPACE, true); // Read-only

    apiEndpoint_ = preferences_.getString(KEY_ENDPOINT, "");
    apiKey_ = preferences_.getString(KEY_API_KEY, "");
    enabled_ = preferences_.getBool(KEY_ENABLED, false);

    preferences_.end();

    Serial.println("[CLOUD] Configuration loaded from NVS");
    Serial.print("[CLOUD] Endpoint: ");
    Serial.println(apiEndpoint_.isEmpty() ? "(empty)" : apiEndpoint_);
    Serial.print("[CLOUD] API Key: ");
    Serial.println(apiKey_.isEmpty() ? "(empty)" : "(set)");
    Serial.print("[CLOUD] Enabled: ");
    Serial.println(enabled_ ? "true" : "false");
}

bool CloudClient::saveConfig() {
    preferences_.begin(NVS_NAMESPACE, false); // Read-write

    preferences_.putString(KEY_ENDPOINT, apiEndpoint_);
    preferences_.putString(KEY_API_KEY, apiKey_);
    preferences_.putBool(KEY_ENABLED, enabled_);

    preferences_.end();

    Serial.println("[CLOUD] Configuration saved to NVS");
    return true;
}

void CloudClient::pushState() {
    JsonDocument doc;
    doc["x"] = currentStatus_.x;
    doc["y"] = currentStatus_.y;
    doc["rotationDeg"] = currentStatus_.rotationDeg;
    doc["tiltDeg"] = currentStatus_.tiltDeg;
    doc["rotationLimitDeg"] = currentStatus_.rotationLimitDeg;
    doc["tiltLimitDeg"] = currentStatus_.tiltLimitDeg;
    doc["moving"] = currentStatus_.moving;

    String payload;
    serializeJson(doc, payload);

    int httpCode = sendRequest("/state", "POST", payload);

    if (httpCode != HTTP_CODE_OK && httpCode > 0) {
        Serial.print("[CLOUD] Failed to push state. HTTP code: ");
        Serial.println(httpCode);
    }
}

void CloudClient::syncOnce() {
    String url = apiEndpoint_ + "/sync";

    syncHttp_.begin(url);
    syncHttp_.setTimeout(SYNC_HTTP_TIMEOUT_MS);
    syncHttp_.addHeader("X-API-Key", apiKey_);

    int httpCode = syncHttp_.GET();

    if (httpCode != HTTP_CODE_OK) {
        if (httpCode > 0) {
            Serial.print("[CLOUD] Sync failed. HTTP code: ");
            Serial.println(httpCode);
        } else {
            Serial.print("[CLOUD] Sync connection error: ");
            Serial.println(syncHttp_.errorToString(httpCode));
        }
        syncHttp_.end();
        return;
    }

    String payload = syncHttp_.getString();
    syncHttp_.end();

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload);
    if (error) {
        Serial.print("[CLOUD] Failed to parse sync response: ");
        Serial.println(error.c_str());
        return;
    }

    if (xSemaphoreTake(syncMutex_, pdMS_TO_TICKS(100)) != pdTRUE) {
        Serial.println("[CLOUD] Failed to acquire mutex for sync data");
        return;
    }

    // Latest joystick target - only accept fresh values so a vanished
    // frontend cannot keep moving the platform
    if (!doc["drive"].isNull()) {
        float ageS = doc["drive"]["age_s"] | 1e9f;
        targetActive_ = (ageS < 30.0f);
        if (ageS <= TARGET_MAX_AGE_S) {
            targetX_ = doc["drive"]["x"] | 0.0f;
            targetY_ = doc["drive"]["y"] | 0.0f;
            hasTarget_ = true;
        }
    } else {
        targetActive_ = false;
    }

    // Axis limits - only report when the cloud value changes,
    // so local (test interface) changes are not constantly overwritten
    if (!doc["config"].isNull()) {
        float rotationLimitDeg = doc["config"]["rotationLimitDeg"] | -1.0f;
        float tiltLimitDeg = doc["config"]["tiltLimitDeg"] | -1.0f;
        if (rotationLimitDeg >= 0.0f && tiltLimitDeg >= 0.0f) {
            const bool changed = !cloudLimitsSeen_ ||
                                 rotationLimitDeg != lastSeenRotationLimitDeg_ ||
                                 tiltLimitDeg != lastSeenTiltLimitDeg_;
            if (changed) {
                cloudRotationLimitDeg_ = rotationLimitDeg;
                cloudTiltLimitDeg_ = tiltLimitDeg;
                hasLimitsUpdate_ = true;
            }
            lastSeenRotationLimitDeg_ = rotationLimitDeg;
            lastSeenTiltLimitDeg_ = tiltLimitDeg;
            cloudLimitsSeen_ = true;
        }
    }

    // Discrete command (stop, center, ...)
    if (!doc["command"].isNull() && !doc["command"]["action"].isNull()) {
        String commandPayload;
        serializeJson(doc["command"], commandPayload);
        pendingCommand_ = commandPayload;
        hasPendingCommand_ = true;

        Serial.print("[CLOUD] Command received: ");
        Serial.println(commandPayload);
    }

    xSemaphoreGive(syncMutex_);
}

int CloudClient::sendRequest(const String& endpoint, const String& method, const String& payload) {
    String url = apiEndpoint_ + endpoint;

    http_.begin(url);
    http_.setTimeout(HTTP_TIMEOUT_MS);
    http_.addHeader("Content-Type", "application/json");
    http_.addHeader("X-API-Key", apiKey_);

    int httpCode;
    if (method == "POST") {
        httpCode = http_.POST(payload);
    } else if (method == "PUT") {
        httpCode = http_.PUT(payload);
    } else {
        httpCode = http_.GET();
    }

    http_.end();
    return httpCode;
}

void CloudClient::startSyncTask() {
    if (syncTaskHandle_ != nullptr) {
        Serial.println("[CLOUD] Sync task already running");
        return;
    }

    syncTaskRunning_ = true;

    // Core 0 (Core 1 runs the Arduino loop); 16KB for HTTPClient + JSON
    xTaskCreatePinnedToCore(
        syncTaskFunction,
        "CloudSync",
        16384,
        this,
        1,
        &syncTaskHandle_,
        0
    );

    Serial.println("[CLOUD] Background sync task started on Core 0");
}

void CloudClient::stopSyncTask() {
    if (syncTaskHandle_ == nullptr) {
        return;
    }

    syncTaskRunning_ = false;

    // The task deletes itself after finishing its current cycle (an HTTP
    // call may block for up to SYNC_HTTP_TIMEOUT_MS). Wait for that, and
    // only force-delete if it never comes back.
    for (int i = 0; i < 140 && syncTaskHandle_ != nullptr; i++) {
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    if (syncTaskHandle_ != nullptr) {
        vTaskDelete(syncTaskHandle_);
        syncTaskHandle_ = nullptr;
    }

    Serial.println("[CLOUD] Background sync task stopped");
}

void CloudClient::syncTaskFunction(void* parameter) {
    CloudClient* client = static_cast<CloudClient*>(parameter);

    if (client == nullptr) {
        Serial.println("[CLOUD] ERROR: Null client pointer in sync task");
        vTaskDelete(NULL);
        return;
    }

    Serial.println("[CLOUD] Sync task running - short polling active");

    while (client->syncTaskRunning_) {
        if (WiFi.status() != WL_CONNECTED) {
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        client->syncOnce();

        // Poll fast while someone is steering, relax when idle
        const unsigned long interval = client->targetActive_
            ? SYNC_INTERVAL_ACTIVE_MS
            : SYNC_INTERVAL_IDLE_MS;
        vTaskDelay(pdMS_TO_TICKS(interval));
    }

    Serial.println("[CLOUD] Sync task exiting");
    client->syncTaskHandle_ = nullptr;
    vTaskDelete(NULL);
}
