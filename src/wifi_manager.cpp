#include "wifi_manager.h"
#include <WiFi.h>
#include <Preferences.h>

WifiManager::WifiManager() : preferences() {}

void WifiManager::begin() {
    preferences.begin("webmotor", false);
    loadCredentials();
    connectToWiFi();
}

void WifiManager::loadCredentials() {
    ssid = preferences.getString("ssid", "");
    password = preferences.getString("password", "");
}

void WifiManager::connectToWiFi() {
    if (ssid.length() > 0) {
        WiFi.begin(ssid.c_str(), password.c_str());
        unsigned long startAttemptTime = millis();

        while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 10000) {
            delay(100);
        }

        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("Connected to Wi-Fi");
        } else {
            startAPMode();
        }
    } else {
        startAPMode();
    }
}

void WifiManager::startAPMode() {
    WiFi.softAP("WebMotor-Config", nullptr);
    Serial.println("AP Mode started. Connect to WebMotor-Config");
}

void WifiManager::saveCredentials(const String& newSSID, const String& newPassword) {
    preferences.putString("ssid", newSSID);
    preferences.putString("password", newPassword);
    preferences.end();
    WiFi.disconnect();
    connectToWiFi();
}