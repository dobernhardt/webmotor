#include <Arduino.h>
#include <ArduinoJson.h>
#include "config.h"
#include "wifi_manager.h"
#include "web_server.h"
#include "motor_controller_uln2003.h"
#include "platform_controller.h"
#include "cloud_client.h"
#include "version.h"

WifiManager wifiManager;
WebServerController webServer;
MotorControllerULN2003 rotationMotor(ROTATION_IN1_PIN, ROTATION_IN2_PIN, ROTATION_IN3_PIN, ROTATION_IN4_PIN);
MotorControllerULN2003 tiltMotor(TILT_IN1_PIN, TILT_IN2_PIN, TILT_IN3_PIN, TILT_IN4_PIN);
PlatformController platformController(rotationMotor, tiltMotor);
CloudClient cloudClient;

void processCloudCommand(const String& commandJson) {
    Serial.print("[CLOUD] Processing command: ");
    Serial.println(commandJson);

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, commandJson);
    if (error) {
        Serial.print("[CLOUD] Invalid command JSON: ");
        Serial.println(error.c_str());
        return;
    }

    const String action = doc["action"] | "";

    if (action.equalsIgnoreCase("stop")) {
        platformController.stop();
    } else if (action.equalsIgnoreCase("center")) {
        platformController.centerAxes();
    } else {
        Serial.print("[CLOUD] Unknown action: ");
        Serial.println(action);
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("==============================================");
    Serial.println("[BOOT] WebMotor Controller starting...");
    Serial.print("[VERSION] "); Serial.println(VERSION_SEMVER);
    Serial.print("[BUILD] "); Serial.println(VERSION_BUILD_TIMESTAMP);
    Serial.print("[COMMIT] "); Serial.print(VERSION_SHORT_SHA);
    Serial.print(" ("); Serial.print(VERSION_BRANCH); Serial.println(")");
    Serial.println("==============================================");

    Serial.println("[INIT] Starting Wi-Fi Manager...");
    wifiManager.begin();
    Serial.println("[INIT] Wi-Fi Manager ready");

    Serial.println("[INIT] Starting Platform Controller...");
    platformController.begin();
    Serial.println("[INIT] Platform Controller ready");

    Serial.println("[INIT] Starting Cloud Client...");
    cloudClient.begin();
    Serial.println("[INIT] Cloud Client ready");

    Serial.println("[INIT] Starting Web Server...");
    webServer.begin(platformController, wifiManager, cloudClient);
    Serial.println("[INIT] Web Server ready");

    Serial.println("[BOOT] System initialization complete!");
}

void loop() {
    static unsigned long lastStatusReport = 0;

    wifiManager.handle();
    webServer.handle();
    cloudClient.handle();

    // Apply the latest joystick target from the cloud (latest-value polling)
    float x, y;
    if (cloudClient.getTarget(x, y)) {
        platformController.setTarget(x, y);
    }

    // Apply axis limits changed in the cloud frontend (WebUI owns them)
    float rotationLimitDeg, tiltLimitDeg;
    if (cloudClient.getLimitsUpdate(rotationLimitDeg, tiltLimitDeg)) {
        Serial.println("[CLOUD] Applying axis limits from cloud");
        platformController.setLimits(rotationLimitDeg, tiltLimitDeg);
    }

    // Process discrete cloud commands (stop, center)
    if (cloudClient.hasCommand()) {
        processCloudCommand(cloudClient.getCommand());
    }

    // Keep the cloud updated with the current platform status
    cloudClient.setStatus(platformController.getStatus());

    // Status report every 60 seconds
    unsigned long currentTime = millis();
    if (currentTime - lastStatusReport >= 60000) {
        lastStatusReport = currentTime;

        Serial.println("=== STATUS REPORT ===");
        Serial.print("Uptime: ");
        Serial.print(currentTime / 1000);
        Serial.println(" seconds");
        Serial.print("Free Heap: ");
        Serial.print(ESP.getFreeHeap());
        Serial.println(" bytes");

        if (wifiManager.isAccessPoint()) {
            Serial.println("WiFi: Access Point Mode");
        } else if (wifiManager.isConnected()) {
            Serial.print("WiFi: Connected to ");
            Serial.println(wifiManager.getSSID());
        } else {
            Serial.println("WiFi: Disconnected");
        }

        PlatformStatus status = platformController.getStatus();
        Serial.printf("Platform: %s (x=%.2f, y=%.2f), rotation %.1f deg (limit %.1f), tilt %.1f deg (limit %.1f)\n",
                      status.moving ? "moving" : "idle",
                      status.x, status.y,
                      status.rotationDeg, status.rotationLimitDeg,
                      status.tiltDeg, status.tiltLimitDeg);

        Serial.println("=== END STATUS ===");
    }

    delay(5);
}
