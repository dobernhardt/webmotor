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
    motor = &motorController;
    wifi = &wifiManager;
    cachedState = motor->getMotorState();

    registerRoutes();
    server.begin();
    Serial.println("HTTP server started");
}

void WebServerController::handle() {
    server.handleClient();
}

void WebServerController::registerRoutes() {
    server.on("/api/motor/status", HTTP_GET, [this]() { this->handleMotorStatus(); });
    server.on("/api/motor/control", HTTP_POST, [this]() { this->handleMotorControl(); });
    server.on("/api/wifi/config", HTTP_POST, [this]() { this->handleWiFiConfig(); });
}

void WebServerController::handleMotorStatus() {
    if (motor != nullptr) {
        cachedState = motor->getMotorState();
    }

    StaticJsonDocument<200> doc;
    doc["microsteps"] = cachedState.microsteps;
    doc["frequency"] = cachedState.frequency;
    doc["direction"] = cachedState.direction;
    doc["mode"] = cachedState.mode;

    String jsonResponse;
    serializeJson(doc, jsonResponse);
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
    if (!server.hasArg("plain")) {
        sendJson(400, "{\"error\":\"No data received\"}");
        return;
    }

    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, server.arg("plain"));

    if (error) {
        sendJson(400, "{\"error\":\"Invalid JSON\"}");
        return;
    }

    if (motor == nullptr) {
        sendJson(500, "{\"error\":\"Motor controller unavailable\"}");
        return;
    }

    if (doc.containsKey("microsteps")) {
        motor->setMicrosteps(doc["microsteps"].as<uint16_t>());
    }
    if (doc.containsKey("frequency")) {
        motor->setFrequency(doc["frequency"].as<uint32_t>());
    }
    if (doc.containsKey("direction")) {
        motor->setDirection(doc["direction"].as<bool>());
    }
    if (doc.containsKey("mode")) {
        const MotorMode mode = parseMode(doc["mode"], motor->getMode());
        motor->setMode(mode);
    }

    cachedState = motor->getMotorState();
    sendJson(200, "{\"status\":\"updated\"}");
}

void WebServerController::handleWiFiConfig() {
    if (!server.hasArg("plain")) {
        sendJson(400, "{\"error\":\"No data received\"}");
        return;
    }

    if (wifi == nullptr) {
        sendJson(500, "{\"error\":\"Wi-Fi manager unavailable\"}");
        return;
    }

    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, server.arg("plain"));
    if (error || !doc.containsKey("ssid") || !doc.containsKey("password")) {
        sendJson(400, "{\"error\":\"Invalid JSON\"}");
        return;
    }

    const String ssid = doc["ssid"].as<String>();
    const String password = doc["password"].as<String>();
    wifi->saveCredentials(ssid, password);

    sendJson(200, "{\"status\":\"credentials saved\"}");
}

void WebServerController::sendJson(int statusCode, const String& payload) {
    server.send(statusCode, "application/json", payload);
}