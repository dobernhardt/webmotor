#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <Arduino.h>
#include <WebServer.h>

class DriveController;
class WifiManager;
class CloudClient;

class WebServerController {
public:
    WebServerController();
    void begin(DriveController& driveController, WifiManager& wifiManager, CloudClient& cloudClient);
    void handle();

private:
    ::WebServer server;
    DriveController* drive;
    WifiManager* wifi;
    CloudClient* cloud;

    void registerRoutes();
    void handleDrive();
    void handleDriveStatus();
    void handleDriveConfigGet();
    void handleDriveConfigPost();
    void handleDriveCenter();
    void handleDriveStop();
    void handleWiFiConfig();
    void handleWiFiStatus();
    void handleCloudConfig();
    void handleCloudStatus();
    void handleCloudTest();
    void handleInfo();
    void sendJson(int statusCode, const String& payload);
    void serveFile(const String& path, const String& contentType);
};

#endif // WEB_SERVER_H
