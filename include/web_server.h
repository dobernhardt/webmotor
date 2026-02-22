#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <Arduino.h>
#include <WebServer.h>
#include "state.h"

class AbstractMotorController;
class WifiManager;
class CloudClient;

class WebServerController {
public:
    WebServerController();
    void begin(AbstractMotorController& motorController, WifiManager& wifiManager, CloudClient& cloudClient);
    void handle();

private:
    ::WebServer server;
    AbstractMotorController* motor;
    WifiManager* wifi;
    CloudClient* cloud;
    MotorState cachedState;

    void registerRoutes();
    void handleMotorStatus();
    void handleMotorControl();
    void handleWiFiConfig();
    void handleWiFiStatus();
    void handleCloudConfig();
    void handleCloudStatus();
    void handleCloudTest();
    void handleInfo();
    void sendJson(int statusCode, const String& payload);
    void serveFile(const String& path, const String& contentType);
    void updateStatusLED();
};

#endif // WEB_SERVER_H