#include <Arduino.h>
#include <ArduinoJson.h>
#include "config.h"
#include "wifi_manager.h"
#include "web_server.h"
#include "motor_controller_uln2003.h"
#include "drive_controller.h"
#include "cloud_client.h"
#include "version.h"

WifiManager wifiManager;
WebServerController webServer;
MotorControllerULN2003 driveMotor(DRIVE_IN1_PIN, DRIVE_IN2_PIN, DRIVE_IN3_PIN, DRIVE_IN4_PIN);
MotorControllerULN2003 steerMotor(STEER_IN1_PIN, STEER_IN2_PIN, STEER_IN3_PIN, STEER_IN4_PIN);
DriveController driveController(driveMotor, steerMotor);
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
        driveController.stop();
    } else if (action.equalsIgnoreCase("center")) {
        driveController.centerSteering();
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

    Serial.println("[INIT] Starting Drive Controller...");
    driveController.begin();
    Serial.println("[INIT] Drive Controller ready");

    Serial.println("[INIT] Starting Cloud Client...");
    cloudClient.begin();
    Serial.println("[INIT] Cloud Client ready");

    Serial.println("[INIT] Starting Web Server...");
    webServer.begin(driveController, wifiManager, cloudClient);
    Serial.println("[INIT] Web Server ready");

    Serial.println("[BOOT] System initialization complete!");
}

void loop() {
    static unsigned long lastStatusReport = 0;

    wifiManager.handle();
    webServer.handle();
    cloudClient.handle();
    driveController.handle();

    // Apply the latest joystick target from the cloud (latest-value polling)
    float x, y;
    if (cloudClient.getDriveTarget(x, y)) {
        driveController.setTarget(x, y);
    }

    // Apply drive configuration changed in the cloud frontend
    float steerLimitDeg;
    uint32_t maxFrequency;
    if (cloudClient.getDriveConfigUpdate(steerLimitDeg, maxFrequency)) {
        Serial.println("[CLOUD] Applying drive config from cloud");
        driveController.setConfig(steerLimitDeg, maxFrequency);
    }

    // Process discrete cloud commands (stop, center)
    if (cloudClient.hasCommand()) {
        processCloudCommand(cloudClient.getCommand());
    }

    // Keep the cloud updated with the current drive status
    cloudClient.setDriveStatus(driveController.getStatus());

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

        DriveStatus status = driveController.getStatus();
        Serial.printf("Drive: %s (x=%.2f, y=%.2f), steering %.1f deg (limit %.1f), maxFreq %u Hz%s\n",
                      status.driving ? "driving" : "stopped",
                      status.x, status.y,
                      status.steeringDeg, status.steerLimitDeg,
                      status.maxFrequency,
                      status.failsafe ? " [FAILSAFE]" : "");

        Serial.println("=== END STATUS ===");
    }

    delay(5);
}
