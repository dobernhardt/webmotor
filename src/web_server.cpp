#include "web_server.h"
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include "config.h"
#include "motor_controller.h"
#include "state.h"

WebServer server;

MotorState motorState;

void handleMotorStatus() {
    String jsonResponse;
    StaticJsonDocument<200> doc;
    doc["microsteps"] = motorState.microsteps;
    doc["frequency"] = motorState.frequency;
    doc["direction"] = motorState.direction;
    doc["mode"] = motorState.mode;
    serializeJson(doc, jsonResponse);
    server.send(200, "application/json", jsonResponse);
}

void handleMotorControl() {
    if (server.hasArg("plain")) {
        String body = server.arg("plain");
        StaticJsonDocument<200> doc;
        DeserializationError error = deserializeJson(doc, body);
        
        if (!error) {
            if (doc.containsKey("frequency")) {
                motorState.frequency = doc["frequency"];
            }
            if (doc.containsKey("microsteps")) {
                motorState.microsteps = doc["microsteps"];
            }
            if (doc.containsKey("direction")) {
                motorState.direction = doc["direction"];
            }
            if (doc.containsKey("mode")) {
                motorState.mode = doc["mode"];
            }
            server.send(200, "application/json", "{\"status\":\"updated\"}");
        } else {
            server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        }
    } else {
        server.send(400, "application/json", "{\"error\":\"No data received\"}");
    }
}

void handleWiFiConfig() {
    if (server.hasArg("plain")) {
        String body = server.arg("plain");
        StaticJsonDocument<200> doc;
        DeserializationError error = deserializeJson(doc, body);
        
        if (!error && doc.containsKey("ssid") && doc.containsKey("password")) {
            // Save credentials and trigger reboot logic here
            server.send(200, "application/json", "{\"status\":\"credentials saved\"}");
        } else {
            server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        }
    } else {
        server.send(400, "application/json", "{\"error\":\"No data received\"}");
    }
}

void setupWebServer() {
    server.on("/api/motor/status", HTTP_GET, handleMotorStatus);
    server.on("/api/motor/control", HTTP_POST, handleMotorControl);
    server.on("/api/wifi/config", HTTP_POST, handleWiFiConfig);
    server.begin();
}

void loopWebServer() {
    server.handleClient();
}