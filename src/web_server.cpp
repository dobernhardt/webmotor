#include "web_server.h"
#include <WiFi.h>
#include <ArduinoJson.h>
#include <SPIFFS.h>
#include <FastLED.h>
#include "motor_controller_tmc2209.h"
#include "wifi_manager.h"
#include "cloud_client.h"
#include "version.h"

// Reference to the LED array from main.cpp
extern CRGB leds[];

namespace {
constexpr int kHttpPort = 80;
}

WebServerController::WebServerController()
    : server(kHttpPort),
      motor(nullptr),
      wifi(nullptr),
      cloud(nullptr),
      cachedState{0, 0, true, MotorMode::STOPPED} {}

void WebServerController::begin(MotorControllerTMC2209& motorController, WifiManager& wifiManager, CloudClient& cloudClient) {
    Serial.println("[WEB] Initializing WebServer...");
    
    // Initialize SPIFFS
    if (!SPIFFS.begin(true)) {
        Serial.println("[WEB] ERROR: SPIFFS mount failed");
        return;
    }
    Serial.println("[WEB] SPIFFS mounted successfully");
    
    motor = &motorController;
    wifi = &wifiManager;
    cloud = &cloudClient;
    cachedState = motor->getMotorState();
    Serial.println("[WEB] Controllers linked");

    Serial.println("[WEB] Registering HTTP routes...");
    registerRoutes();
    
    server.begin();
    Serial.print("[WEB] HTTP server listening on port ");
    Serial.println(kHttpPort);
    Serial.println("[WEB] WebServer initialization complete");
    
    // Initial LED update based on current state
    updateStatusLED();
}

void WebServerController::handle() {
    server.handleClient();
}

void WebServerController::registerRoutes() {
    Serial.println("[WEB] Setting up API endpoints...");
    
    // Version/Info route
    server.on("/api/info", HTTP_GET, [this]() { 
        Serial.println("[API] GET /api/info");
        this->handleInfo(); 
    });
    
    // API routes
    server.on("/api/motor/status", HTTP_GET, [this]() { 
        Serial.println("[API] GET /api/motor/status");
        this->handleMotorStatus(); 
    });
    
    server.on("/api/motor/control", HTTP_POST, [this]() { 
        Serial.println("[API] POST /api/motor/control");
        this->handleMotorControl(); 
    });
    
    server.on("/api/wifi/config", HTTP_POST, [this]() { 
        Serial.println("[API] POST /api/wifi/config");
        this->handleWiFiConfig(); 
    });
    
    server.on("/api/wifi/status", HTTP_GET, [this]() { 
        Serial.println("[API] GET /api/wifi/status");
        this->handleWiFiStatus(); 
    });
    
    // Cloud configuration routes
    server.on("/api/cloud/config", HTTP_POST, [this]() { 
        Serial.println("[API] POST /api/cloud/config");
        this->handleCloudConfig(); 
    });
    
    server.on("/api/cloud/status", HTTP_GET, [this]() { 
        Serial.println("[API] GET /api/cloud/status");
        this->handleCloudStatus(); 
    });
    
    server.on("/api/cloud/test", HTTP_GET, [this]() { 
        Serial.println("[API] GET /api/cloud/test");
        this->handleCloudTest(); 
    });
    
    // Static file routes
    server.on("/", [this]() { 
        Serial.println("[WEB] Serving index.html");
        this->serveFile("/index.html", "text/html");
    });
    
    server.on("/app.js", [this]() { 
        Serial.println("[WEB] Serving app.js");
        this->serveFile("/app.js", "application/javascript");
    });
    
    server.on("/styles.css", [this]() { 
        Serial.println("[WEB] Serving styles.css");
        this->serveFile("/styles.css", "text/css");
    });
    
    // Catch-all for 404
    server.onNotFound([this]() {
        Serial.print("[WEB] 404 - File not found: ");
        Serial.println(server.uri());
        server.send(404, "text/plain", "File not found");
    });
    
    Serial.println("[WEB] Routes registered successfully");
}

void WebServerController::handleMotorStatus() {
    Serial.println("[API] Processing motor status request");
    
    if (motor != nullptr) {
        cachedState = motor->getMotorState();
    }

    JsonDocument doc;
    doc["microsteps"] = cachedState.microsteps;
    doc["frequency"] = cachedState.frequency;
    doc["direction"] = cachedState.direction;
    
    const char* modeStr;
    switch(cachedState.mode) {
        case MotorMode::RUNNING: modeStr = "running"; break;
        case MotorMode::STOPPED: modeStr = "stopped"; break;
        default: modeStr = "released"; break;
    }
    doc["mode"] = modeStr;

    String jsonResponse;
    serializeJson(doc, jsonResponse);
    
    Serial.print("[API] Sending motor status: ");
    Serial.println(jsonResponse);
    
    sendJson(200, jsonResponse);
}
void WebServerController::handleInfo() {
    Serial.println("[API] Processing info request");

    JsonDocument doc;
    doc["version"] = VERSION_SEMVER;
    doc["fullVersion"] = VERSION_FULL;
    doc["branch"] = VERSION_BRANCH;
    doc["commit"] = VERSION_SHORT_SHA;
    doc["commitFull"] = VERSION_SHA;
    doc["buildTimestamp"] = VERSION_BUILD_TIMESTAMP;
    doc["platform"] = "ESP32-S3";
    doc["board"] = "ATOM S3 LITE";

    String jsonResponse;
    serializeJson(doc, jsonResponse);
    
    Serial.print("[API] Sending version info: ");
    Serial.println(jsonResponse);
    
    sendJson(200, jsonResponse);
}
namespace {
MotorMode parseMode(const JsonVariant& value, MotorMode fallback) {
    if (value.is<int>()) {
        const int val = value.as<int>();
        switch (val) {
            case STOPPED:
            case RUNNING:
            case RELEASED:
                return static_cast<MotorMode>(val);
            default:
                return fallback;
        }
    }

    if (value.is<const char*>()) {
        const String modeStr = value.as<const char*>();
        if (modeStr.equalsIgnoreCase("STOPPED")) {
            return MotorMode::STOPPED;
        }
        if (modeStr.equalsIgnoreCase("RUNNING")) {
            return MotorMode::RUNNING;
        }
        if (modeStr.equalsIgnoreCase("RELEASED")) {
            return MotorMode::RELEASED;
        }
    }

    return fallback;
}
}

void WebServerController::handleMotorControl() {
    Serial.println("[API] Processing motor control request");
    
    if (!server.hasArg("plain")) {
        Serial.println("[API] ERROR: No JSON payload");
        sendJson(400, "{\"error\":\"No data received\"}");
        return;
    }

    String body = server.arg("plain");
    Serial.print("[API] Request payload: ");
    Serial.println(body);

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, body);

    if (error) {
        Serial.print("[API] ERROR: JSON parse failed - ");
        Serial.println(error.c_str());
        sendJson(400, "{\"error\":\"Invalid JSON\"}");
        return;
    }

    if (motor == nullptr) {
        Serial.println("[API] ERROR: Motor controller unavailable");
        sendJson(500, "{\"error\":\"Motor controller unavailable\"}");
        return;
    }

    if (!doc["microsteps"].isNull()) {
        uint16_t microsteps = doc["microsteps"].as<uint16_t>();
        Serial.print("[MOTOR] Setting microsteps: ");
        Serial.println(microsteps);
        motor->setMicrosteps(microsteps);
    }
    if (!doc["frequency"].isNull()) {
        uint32_t frequency = doc["frequency"].as<uint32_t>();
        Serial.print("[MOTOR] Setting frequency: ");
        Serial.print(frequency);
        Serial.println(" Hz");
        motor->setFrequency(frequency);
    }
    if (!doc["direction"].isNull()) {
        bool direction = doc["direction"].as<bool>();
        Serial.print("[MOTOR] Setting direction: ");
        Serial.println(direction ? "CW" : "CCW");
        motor->setDirection(direction);
    }
    if (!doc["mode"].isNull()) {
        const MotorMode mode = parseMode(doc["mode"], motor->getMode());
        Serial.print("[MOTOR] Setting mode: ");
        Serial.println((int)mode);
        motor->setMode(mode);
    }

    cachedState = motor->getMotorState();
    Serial.println("[API] Motor control completed successfully");
    
    // Update LED to reflect new motor state
    updateStatusLED();
    
    sendJson(200, "{\"status\":\"updated\"}");
}

void WebServerController::handleWiFiConfig() {
    Serial.println("[API] Processing WiFi config request");
    
    if (!server.hasArg("plain")) {
        Serial.println("[API] ERROR: No JSON payload for WiFi config");
        sendJson(400, "{\"error\":\"No data received\"}");
        return;
    }

    if (wifi == nullptr) {
        Serial.println("[API] ERROR: Wi-Fi manager unavailable");
        sendJson(500, "{\"error\":\"Wi-Fi manager unavailable\"}");
        return;
    }

    String body = server.arg("plain");
    Serial.print("[API] WiFi config payload: ");
    Serial.println(body);

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, body);
    if (error || doc["ssid"].isNull() || doc["password"].isNull()) {
        Serial.print("[API] ERROR: WiFi JSON parse failed - ");
        if (error) {
            Serial.println(error.c_str());
        } else {
            Serial.println("Missing SSID or password");
        }
        sendJson(400, "{\"error\":\"Invalid JSON\"}");
        return;
    }

    const String ssid = doc["ssid"].as<String>();
    const String password = doc["password"].as<String>();
    
    Serial.print("[WIFI] Saving credentials for SSID: ");
    Serial.println(ssid);
    // Note: Password not logged for security
    
    wifi->saveCredentials(ssid, password);
    
    Serial.println("[WIFI] Credentials saved successfully");
    
    // Update LED to indicate configuration change
    updateStatusLED();
    
    sendJson(200, "{\"status\":\"credentials saved\"}");
}

void WebServerController::handleWiFiStatus() {
    Serial.println("[API] Processing WiFi status request");
    
    if (wifi == nullptr) {
        Serial.println("[API] ERROR: Wi-Fi manager unavailable");
        sendJson(500, "{\"error\":\"Wi-Fi manager unavailable\"}");
        return;
    }

    JsonDocument doc;
    doc["isAccessPoint"] = wifi->isAccessPoint();
    doc["isConnected"] = wifi->isConnected();
    
    if (!wifi->isAccessPoint() && wifi->isConnected()) {
        doc["ssid"] = wifi->getSSID();
        doc["ipAddress"] = WiFi.localIP().toString();
    } else if (wifi->isAccessPoint()) {
        doc["ssid"] = "WebMotor-Config";
        doc["ipAddress"] = WiFi.softAPIP().toString();
    } else {
        doc["ssid"] = "";
        doc["ipAddress"] = "";
    }

    String jsonResponse;
    serializeJson(doc, jsonResponse);
    
    Serial.print("[API] Sending WiFi status: ");
    Serial.println(jsonResponse);
    
    sendJson(200, jsonResponse);
}

void WebServerController::sendJson(int statusCode, const String& payload) {
    Serial.print("[HTTP] Response ");
    Serial.print(statusCode);
    Serial.print(": ");
    Serial.println(payload);
    
    server.send(statusCode, "application/json", payload);
}

void WebServerController::serveFile(const String& path, const String& contentType) {
    if (SPIFFS.exists(path)) {
        File file = SPIFFS.open(path, "r");
        if (file) {
            Serial.print("[WEB] Serving file: ");
            Serial.print(path);
            Serial.print(" (");
            Serial.print(file.size());
            Serial.println(" bytes)");
            
            server.streamFile(file, contentType);
            file.close();
            return;
        }
    }
    
    Serial.print("[WEB] ERROR: File not found - ");
    Serial.println(path);
    server.send(404, "text/plain", "File not found");
}

void WebServerController::updateStatusLED() {
    if (!motor || !wifi) return;
    
    // This method provides immediate LED feedback for web API calls
    // The main loop handles the continuous LED state management
    
    Serial.println("[LED] Updating status LED from web server");
    
    // Brief visual confirmation of web API interaction
    // Flash white briefly to indicate web command received
    leds[0] = CRGB::White;
    FastLED.show();
    delay(50);  // Very brief flash
    leds[0] = CRGB::Black;
    FastLED.show();
    delay(50);
    
    // The main loop will update to the appropriate state based on current system status
}

void WebServerController::handleCloudConfig() {
    Serial.println("[API] Processing cloud config request");
    
    if (!server.hasArg("plain")) {
        Serial.println("[API] ERROR: No JSON payload for cloud config");
        sendJson(400, "{\"error\":\"No data received\"}");
        return;
    }

    if (cloud == nullptr) {
        Serial.println("[API] ERROR: Cloud client unavailable");
        sendJson(500, "{\"error\":\"Cloud client unavailable\"}");
        return;
    }

    String body = server.arg("plain");
    Serial.print("[API] Cloud config payload: ");
    Serial.println(body);

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, body);
    if (error) {
        Serial.print("[API] ERROR: Cloud JSON parse failed - ");
        Serial.println(error.c_str());
        sendJson(400, "{\"error\":\"Invalid JSON\"}");
        return;
    }

    String apiEndpoint = doc["apiEndpoint"] | "";
    String apiKey = doc["apiKey"] | "";
    bool enabled = doc["enabled"] | false;
    
    Serial.print("[CLOUD] Saving configuration - Endpoint: ");
    Serial.println(apiEndpoint);
    Serial.print("[CLOUD] Enabled: ");
    Serial.println(enabled ? "true" : "false");
    // Note: API key not logged for security
    
    if (cloud->setConfig(apiEndpoint, apiKey, enabled)) {
        Serial.println("[CLOUD] Configuration saved successfully");
        sendJson(200, "{\"status\":\"configuration saved\"}");
    } else {
        Serial.println("[CLOUD] ERROR: Failed to save configuration");
        sendJson(500, "{\"error\":\"Failed to save configuration\"}");
    }
}

void WebServerController::handleCloudStatus() {
    Serial.println("[API] Processing cloud status request");
    
    if (cloud == nullptr) {
        Serial.println("[API] ERROR: Cloud client unavailable");
        sendJson(500, "{\"error\":\"Cloud client unavailable\"}");
        return;
    }

    String apiEndpoint, apiKey;
    bool enabled;
    cloud->getConfig(apiEndpoint, apiKey, enabled);

    JsonDocument doc;
    doc["apiEndpoint"] = apiEndpoint;
    doc["apiKey"] = apiKey.isEmpty() ? "" : "********"; // Mask API key
    doc["enabled"] = enabled;
    doc["configured"] = !apiEndpoint.isEmpty() && !apiKey.isEmpty();

    String jsonResponse;
    serializeJson(doc, jsonResponse);
    
    Serial.print("[API] Sending cloud status: ");
    Serial.println(jsonResponse);
    
    sendJson(200, jsonResponse);
}

void WebServerController::handleCloudTest() {
    Serial.println("[API] Processing cloud test request");
    
    if (cloud == nullptr) {
        Serial.println("[API] ERROR: Cloud client unavailable");
        sendJson(500, "{\"error\":\"Cloud client unavailable\"}");
        return;
    }

    bool success = cloud->testConnection();

    JsonDocument doc;
    doc["success"] = success;
    doc["message"] = success ? "Connection successful" : "Connection failed";

    String jsonResponse;
    serializeJson(doc, jsonResponse);
    
    Serial.print("[API] Cloud test result: ");
    Serial.println(jsonResponse);
    
    sendJson(200, jsonResponse);
}