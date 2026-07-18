#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <Preferences.h>

class WifiManager {
public:
    WifiManager();
    void begin();
    void handle();
    void saveCredentials(const String& ssid, const String& password);
    String getSSID() const;
    bool isConnected() const;
    bool isAccessPoint() const;

private:
    Preferences preferences;
    DNSServer dnsServer;
    String storedSSID;
    String storedPassword;
    bool apModeActive;
    bool reconnectPending;
    unsigned long reconnectRequestedAt;
    unsigned long lastReconnectAttempt;

    void loadCredentials();
    bool credentialsAvailable() const;
    void connectToWiFi();
    void startAccessPoint();
    void stopAccessPoint();
};

#endif // WIFI_MANAGER_H