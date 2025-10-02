#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <Arduino.h>
#include <WebServer.h>
#include "config.h"
#include "state.h"

class WebServer {
public:
    WebServer();
    void begin();
    void handleClient();
    
private:
    WebServer server;

    void setupRoutes();
    void handleMotorStatus();
    void handleMotorControl();
    void handleWiFiConfig();
    void sendResponse(int code, const String& message);
};

#endif // WEB_SERVER_H