#include <Arduino.h>
#include "wifi_manager.h"
#include "web_server.h"
#include "motor_controller.h"

WifiManager wifiManager;
WebServer webServer;
MotorController motorController;

void setup() {
    Serial.begin(115200);
    
    // Initialize Wi-Fi manager
    wifiManager.begin();

    // Initialize motor controller
    motorController.begin();

    // Initialize web server
    webServer.begin();
}

void loop() {
    // Handle Wi-Fi and web server tasks
    wifiManager.handle();
    webServer.handle();
}