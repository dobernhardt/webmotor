#include "web_server.h"
#include <WiFi.h>
#include <ArduinoJson.h>
#include "motor_controller.h"
#include "wifi_manager.h"

namespace {
constexpr int kHttpPort = 80;
}

WebServerController::WebServerController()
    : server(kHttpPort),
      motor(nullptr),
      wifi(nullptr),
      cachedState{0, 0, true, MotorMode::STOPPED} {}

void WebServerController::begin(MotorController& motorController, WifiManager& wifiManager) {
    Serial.println("[WEB] Initializing WebServer...");
    
    motor = &motorController;
    wifi = &wifiManager;
    cachedState = motor->getMotorState();
    Serial.println("[WEB] Controllers linked");

    Serial.println("[WEB] Registering HTTP routes...");
    registerRoutes();
    
    server.begin();
    Serial.print("[WEB] HTTP server listening on port ");
    Serial.println(kHttpPort);
    Serial.println("[WEB] WebServer initialization complete");
}

void WebServerController::handle() {
    server.handleClient();
}

void WebServerController::registerRoutes() {
    Serial.println("[WEB] Setting up API endpoints...");
    
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
    
    // Simple test page
    server.on("/", [this]() { 
        Serial.println("[WEB] Root page requested");
        String html = "<!DOCTYPE html><html><head><title>WebMotor</title></head><body>";
        html += "<h1>WebMotor Controller</h1>";
        html += "<p>ATOM S3 Lite Educational Project</p>";
        html += "<h2>API Endpoints:</h2><ul>";
        html += "<li>GET <a href='/api/motor/status'>/api/motor/status</a></li>";
        html += "<li>POST /api/motor/control</li>";
        html += "<li>POST /api/wifi/config</li></ul>";
        html += "<p>Free Memory: " + String(ESP.getFreeHeap()) + " bytes</p>";
        html += "</body></html>";
        server.send(200, "text/html", html);
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
    sendJson(200, "{\"status\":\"credentials saved\"}");
}

void WebServerController::sendJson(int statusCode, const String& payload) {
    Serial.print("[HTTP] Response ");
    Serial.print(statusCode);
    Serial.print(": ");
    Serial.println(payload);
    
    server.send(statusCode, "application/json", payload);
}