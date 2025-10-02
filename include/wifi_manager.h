#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <WiFi.h>
#include <Preferences.h>

class WifiManager {
public:
    WifiManager();
    void begin();
    void connect();
    void startAP();
    void handleClient();
    void saveCredentials(const char* ssid, const char* password);
    void loadCredentials(char* ssid, char* password);

private:
    Preferences preferences;
    const char* apSSID = "WebMotor-Config";
    const char* apPassword = "12345678"; // Optional: Set a password for AP mode
    bool credentialsAvailable();
};

#endif // WIFI_MANAGER_H