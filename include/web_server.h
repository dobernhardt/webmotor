#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <Arduino.h>
#include <WebServer.h>
#include "state.h"

class MotorControllerTMC2209;
class WifiManager;

class WebServerController {
public:
    WebServerController();
    void begin(MotorControllerTMC2209& motorController, WifiManager& wifiManager);
    void handle();

private:
    ::WebServer server;
    MotorControllerTMC2209* motor;
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