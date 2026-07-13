#include "web_server.h"
#include <WiFi.h>
#include <ArduinoJson.h>
#include <SPIFFS.h>
#include "drive_controller.h"
#include "wifi_manager.h"
#include "cloud_client.h"
#include "version.h"

namespace {
constexpr int kHttpPort = 80;
}

WebServerController::WebServerController()
    : server(kHttpPort),
      drive(nullptr),
      wifi(nullptr),
      cloud(nullptr) {}

void WebServerController::begin(DriveController& driveController, WifiManager& wifiManager, CloudClient& cloudClient) {
    Serial.println("[WEB] Initializing WebServer...");

    // Initialize SPIFFS
    if (!SPIFFS.begin(true)) {
        Serial.println("[WEB] ERROR: SPIFFS mount failed");
        return;
    }
    Serial.println("[WEB] SPIFFS mounted successfully");

    drive = &driveController;
    wifi = &wifiManager;
    cloud = &cloudClient;
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

    // Version/Info route
    server.on("/api/info", HTTP_GET, [this]() {
        this->handleInfo();
    });

    // Drive routes (joystick control)
    // No per-request logging here: the joystick posts at ~10 Hz
    server.on("/api/drive", HTTP_POST, [this]() {
        this->handleDrive();
    });

    server.on("/api/drive/status", HTTP_GET, [this]() {
        this->handleDriveStatus();
    });

    server.on("/api/drive/config", HTTP_GET, [this]() {
        this->handleDriveConfigGet();
    });

    server.on("/api/drive/config", HTTP_POST, [this]() {
        this->handleDriveConfigPost();
    });

    server.on("/api/drive/center", HTTP_POST, [this]() {
        Serial.println("[API] POST /api/drive/center");
        this->handleDriveCenter();
    });

    server.on("/api/drive/stop", HTTP_POST, [this]() {
        Serial.println("[API] POST /api/drive/stop");
        this->handleDriveStop();
    });

    // WiFi routes
    server.on("/api/wifi/config", HTTP_POST, [this]() {
        Serial.println("[API] POST /api/wifi/config");
        this->handleWiFiConfig();
    });

    server.on("/api/wifi/status", HTTP_GET, [this]() {
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
        this->serveFile("/index.html", "text/html");
    });

    server.on("/app.js", [this]() {
        this->serveFile("/app.js", "application/javascript");
    });

    server.on("/styles.css", [this]() {
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

void WebServerController::handleDrive() {
    if (drive == nullptr) {
        sendJson(500, "{\"error\":\"Drive controller unavailable\"}");
        return;
    }

    if (!server.hasArg("plain")) {
        sendJson(400, "{\"error\":\"No data received\"}");
        return;
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, server.arg("plain"));
    if (error || doc["x"].isNull() || doc["y"].isNull()) {
        sendJson(400, "{\"error\":\"Expected JSON with x and y\"}");
        return;
    }

    drive->setTarget(doc["x"].as<float>(), doc["y"].as<float>());
    server.send(200, "application/json", "{\"status\":\"ok\"}");
}

void WebServerController::handleDriveStatus() {
    if (drive == nullptr) {
        sendJson(500, "{\"error\":\"Drive controller unavailable\"}");
        return;
    }

    const DriveStatus status = drive->getStatus();

    JsonDocument doc;
    doc["x"] = status.x;
    doc["y"] = status.y;
    doc["steeringDeg"] = status.steeringDeg;
    doc["steerLimitDeg"] = status.steerLimitDeg;
    doc["maxFrequency"] = status.maxFrequency;
    doc["driving"] = status.driving;
    doc["failsafe"] = status.failsafe;

    String jsonResponse;
    serializeJson(doc, jsonResponse);
    server.send(200, "application/json", jsonResponse);
}

void WebServerController::handleDriveConfigGet() {
    if (drive == nullptr) {
        sendJson(500, "{\"error\":\"Drive controller unavailable\"}");
        return;
    }

    JsonDocument doc;
    doc["steerLimitDeg"] = drive->getSteerLimitDeg();
    doc["maxFrequency"] = drive->getMaxFrequency();

    String jsonResponse;
    serializeJson(doc, jsonResponse);
    server.send(200, "application/json", jsonResponse);
}

void WebServerController::handleDriveConfigPost() {
    Serial.println("[API] POST /api/drive/config");

    if (drive == nullptr) {
        sendJson(500, "{\"error\":\"Drive controller unavailable\"}");
        return;
    }

    if (!server.hasArg("plain")) {
        sendJson(400, "{\"error\":\"No data received\"}");
        return;
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, server.arg("plain"));
    if (error) {
        sendJson(400, "{\"error\":\"Invalid JSON\"}");
        return;
    }

    const float steerLimitDeg = doc["steerLimitDeg"] | drive->getSteerLimitDeg();
    const uint32_t maxFrequency = doc["maxFrequency"] | drive->getMaxFrequency();

    if (!drive->setConfig(steerLimitDeg, maxFrequency)) {
        sendJson(400, "{\"error\":\"Invalid configuration values\"}");
        return;
    }

    sendJson(200, "{\"status\":\"configuration saved\"}");
}

void WebServerController::handleDriveCenter() {
    if (drive == nullptr) {
        sendJson(500, "{\"error\":\"Drive controller unavailable\"}");
        return;
    }

    drive->centerSteering();
    sendJson(200, "{\"status\":\"steering centered\"}");
}

void WebServerController::handleDriveStop() {
    if (drive == nullptr) {
        sendJson(500, "{\"error\":\"Drive controller unavailable\"}");
        return;
    }

    drive->stop();
    sendJson(200, "{\"status\":\"stopped\"}");
}

void WebServerController::handleInfo() {
    JsonDocument doc;
    doc["version"] = VERSION_SEMVER;
    doc["fullVersion"] = VERSION_FULL;
    doc["branch"] = VERSION_BRANCH;
    doc["commit"] = VERSION_SHORT_SHA;
    doc["commitFull"] = VERSION_SHA;
    doc["buildTimestamp"] = VERSION_BUILD_TIMESTAMP;
    doc["platform"] = "ESP32";
    doc["board"] = "ESP32-PICO-KIT V4";

    String jsonResponse;
    serializeJson(doc, jsonResponse);
    server.send(200, "application/json", jsonResponse);
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

void WebServerController::handleWiFiStatus() {
    if (wifi == nullptr) {
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
    server.send(200, "application/json", jsonResponse);
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
    Serial.print("[CLOUD] API Key provided: ");
    Serial.println(apiKey.isEmpty() ? "false (keeping existing)" : "true");
    Serial.print("[CLOUD] Enabled: ");
    Serial.println(enabled ? "true" : "false");

    // If API key is empty, keep the existing one
    if (apiKey.isEmpty()) {
        String currentEndpoint, currentKey;
        bool currentEnabled;
        cloud->getConfig(currentEndpoint, currentKey, currentEnabled);
        apiKey = currentKey;
        Serial.println("[CLOUD] Using existing API key");
    }

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

    sendJson(200, jsonResponse);
}
