#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <Arduino.h>
#include <WebServer.h>
#include "state.h"

class MotorController;
class WifiManager;

class WebServerController {
public:
    WebServerController();
    void begin(MotorController& motorController, WifiManager& wifiManager);
    void handle();

private:
    ::WebServer server;
    MotorController* motor;
    WifiManager* wifi;
    MotorState cachedState;

    void registerRoutes();
    void handleMotorStatus();
    void handleMotorControl();
    void handleWiFiConfig();
    void handleWiFiStatus();
    void sendJson(int statusCode, const String& payload);
    void serveFile(const String& path, const String& contentType);
    void updateStatusLED();
};

#endif // WEB_SERVER_H