#include "cloud_client.h"
#include <ArduinoJson.h>
#include <WiFi.h>

namespace {
constexpr const char* NVS_NAMESPACE = "cloud";
constexpr const char* KEY_ENDPOINT = "endpoint";
constexpr const char* KEY_API_KEY = "apikey";
constexpr const char* KEY_ENABLED = "enabled";
constexpr int HTTP_TIMEOUT_MS = 35000; // 35 seconds (30s long poll + 5s margin)
}

CloudClient::CloudClient()
    : enabled_(false),
      lastStatePush_(0),
      lastCommandPoll_(0),
      hasPendingCommand_(false),
      currentState_{0, 0, true, MotorMode::STOPPED} {}

void CloudClient::begin() {
    Serial.println("[CLOUD] Initializing cloud client...");
    loadConfig();
    
    if (enabled_ && !apiEndpoint_.isEmpty() && !apiKey_.isEmpty()) {
        Serial.println("[CLOUD] Cloud sync enabled");
        Serial.print("[CLOUD] Endpoint: ");
        Serial.println(apiEndpoint_);
    } else {
        Serial.println("[CLOUD] Cloud sync disabled");
    }
}

void CloudClient::handle() {
    if (!enabled_ || apiEndpoint_.isEmpty() || apiKey_.isEmpty()) {
        return;
    }
    
    // Check WiFi connection
    if (WiFi.status() != WL_CONNECTED) {
        return;
    }
    
    // Log when cloud sync actually starts working (only once)
    static bool syncStartLogged = false;
    if (!syncStartLogged) {
        Serial.println("[CLOUD] *** Cloud sync is now ACTIVE - polling for commands and pushing state ***");
        syncStartLogged = true;
    }
    
    unsigned long now = millis();
    
    // Push state periodically
    if (now - lastStatePush_ >= STATE_PUSH_INTERVAL) {
        pushState();
        lastStatePush_ = now;
    }
    
    // Poll for commands (only if not currently waiting for a command)
    if (!hasPendingCommand_ && now - lastCommandPoll_ >= COMMAND_POLL_INTERVAL) {
        pollCommands();
        lastCommandPoll_ = now;
    }
}

void CloudClient::setEnabled(bool enabled) {
    enabled_ = enabled;
    saveConfig();
}

void CloudClient::getConfig(String& apiEndpoint, String& apiKey, bool& enabled) const {
    Serial.print("[CLOUD] getConfig called - enabled_: ");
    Serial.println(enabled_ ? "true" : "false");
    
    apiEndpoint = apiEndpoint_;
    apiKey = apiKey_;
    enabled = enabled_;
}

bool CloudClient::setConfig(const String& apiEndpoint, const String& apiKey, bool enabled) {
    Serial.print("[CLOUD] setConfig called - enabled parameter: ");
    Serial.println(enabled ? "true" : "false");
    
    apiEndpoint_ = apiEndpoint;
    apiKey_ = apiKey;
    enabled_ = enabled;
    
    Serial.print("[CLOUD] enabled_ after assignment: ");
    Serial.println(enabled_ ? "true" : "false");
    
    bool result = saveConfig();
    
    Serial.print("[CLOUD] enabled_ after saveConfig: ");
    Serial.println(enabled_ ? "true" : "false");
    
    return result;
}

bool CloudClient::testConnection() {
    if (apiEndpoint_.isEmpty() || apiKey_.isEmpty()) {
        Serial.println("[CLOUD] Cannot test connection - configuration incomplete");
        return false;
    }
    
    Serial.println("[CLOUD] Testing connection...");
    
    http_.begin(apiEndpoint_ + "/health");
    http_.setTimeout(5000); // 5 second timeout for health check
    
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

void CloudClient::setMotorState(const MotorState& state) {
    currentState_ = state;
}

String CloudClient::getCommand() {
    if (!hasPendingCommand_) {
        return "";
    }
    
    hasPendingCommand_ = false;
    String cmd = pendingCommand_;
    pendingCommand_ = "";
    return cmd;
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
    Serial.print("[CLOUD] saveConfig - saving enabled_: ");
    Serial.println(enabled_ ? "true" : "false");
    
    preferences_.begin(NVS_NAMESPACE, false); // Read-write
    
    preferences_.putString(KEY_ENDPOINT, apiEndpoint_);
    preferences_.putString(KEY_API_KEY, apiKey_);
    preferences_.putBool(KEY_ENABLED, enabled_);
    
    preferences_.end();
    
    Serial.println("[CLOUD] Configuration saved to NVS");
    
    // Verify by reading back
    preferences_.begin(NVS_NAMESPACE, true);
    bool savedValue = preferences_.getBool(KEY_ENABLED, false);
    preferences_.end();
    Serial.print("[CLOUD] Verification read from NVS - enabled: ");
    Serial.println(savedValue ? "true" : "false");
    
    return true;
}

void CloudClient::pushState() {
    JsonDocument doc;
    doc["microsteps"] = currentState_.microsteps;
    doc["frequency"] = currentState_.frequency;
    doc["direction"] = currentState_.direction;
    
    // Convert mode to string
    const char* modeStr;
    switch(currentState_.mode) {
        case MotorMode::RUNNING: modeStr = "RUNNING"; break;
        case MotorMode::STOPPED: modeStr = "STOPPED"; break;
        case MotorMode::RELEASED: modeStr = "RELEASED"; break;
        default: modeStr = "UNKNOWN"; break;
    }
    doc["mode"] = modeStr;
    doc["enabled"] = true;
    doc["moving"] = (currentState_.mode == MotorMode::RUNNING);
    doc["homed"] = false; // This project doesn't have homing
    
    String payload;
    serializeJson(doc, payload);
    
    int httpCode = sendRequest("/state", "POST", payload);
    
    if (httpCode == HTTP_CODE_OK) {
        // Successful push (log at debug level to avoid spam)
        // Serial.println("[CLOUD] State pushed successfully");
    } else if (httpCode > 0) {
        Serial.print("[CLOUD] Failed to push state. HTTP code: ");
        Serial.println(httpCode);
    }
}

void CloudClient::pollCommands() {
    int httpCode = sendRequest("/commands/poll", "GET");
    
    if (httpCode == HTTP_CODE_OK) {
        String payload = http_.getString();
        http_.end();
        
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, payload);
        
        if (error) {
            Serial.print("[CLOUD] Failed to parse poll response: ");
            Serial.println(error.c_str());
            return;
        }
        
        // Check if there's a command
        if (!doc["command"].isNull() && !doc["command"]["action"].isNull()) {
            String commandPayload;
            serializeJson(doc["command"], commandPayload);
            
            pendingCommand_ = commandPayload;
            hasPendingCommand_ = true;
            
            Serial.print("[CLOUD] Command received: ");
            Serial.println(commandPayload);
        }
    } else if (httpCode > 0) {
        Serial.print("[CLOUD] Failed to poll commands. HTTP code: ");
        Serial.println(httpCode);
        http_.end();
    } else {
        // Connection error
        Serial.print("[CLOUD] Connection error: ");
        Serial.println(http_.errorToString(httpCode));
        http_.end();
    }
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
        // GET or other methods
        httpCode = http_.GET();
    }
    
    return httpCode;
}
