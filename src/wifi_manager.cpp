#include "wifi_manager.h"

namespace {
constexpr uint32_t kReconnectIntervalMs = 5000;
// Delay between saving credentials and tearing down the AP, so the HTTP
// response still reaches the client before its connection dies.
constexpr uint32_t kApplyCredentialsDelayMs = 750;
constexpr uint16_t kDnsPort = 53;
constexpr const char* kPreferencesNamespace = "webmotor";
constexpr const char* kPreferenceKeySSID = "ssid";
constexpr const char* kPreferenceKeyPassword = "password";
constexpr const char* kApSSID = "WebMotor-Config";
constexpr const char* kApPassword = "12345678";
}

WifiManager::WifiManager()
    : preferences(),
      dnsServer(),
      storedSSID(),
      storedPassword(),
      apModeActive(false),
      reconnectPending(false),
      reconnectRequestedAt(0),
      lastReconnectAttempt(0) {}

void WifiManager::begin() {
    preferences.begin(kPreferencesNamespace, false);
    loadCredentials();
    WiFi.mode(WIFI_STA);
    connectToWiFi();
}

void WifiManager::handle() {
    if (apModeActive) {
        dnsServer.processNextRequest();
    }

    if (reconnectPending) {
        if (millis() - reconnectRequestedAt >= kApplyCredentialsDelayMs) {
            reconnectPending = false;
            stopAccessPoint();
            WiFi.disconnect();
            WiFi.mode(WIFI_STA);
            connectToWiFi();
        }
        return;
    }

    if (apModeActive) {
        return;
    }

    if (WiFi.status() == WL_CONNECTED) {
        return;
    }

    const unsigned long now = millis();
    if (now - lastReconnectAttempt >= kReconnectIntervalMs) {
        lastReconnectAttempt = now;
        connectToWiFi();
    }
}

void WifiManager::saveCredentials(const String& ssid, const String& password) {
    storedSSID = ssid;
    storedPassword = password;

    preferences.putString(kPreferenceKeySSID, storedSSID);
    preferences.putString(kPreferenceKeyPassword, storedPassword);

    reconnectPending = true;
    reconnectRequestedAt = millis();
}

String WifiManager::getSSID() const {
    return storedSSID;
}

bool WifiManager::isConnected() const {
    return WiFi.status() == WL_CONNECTED;
}

bool WifiManager::isAccessPoint() const {
    return apModeActive;
}

void WifiManager::loadCredentials() {
    storedSSID = preferences.getString(kPreferenceKeySSID, "");
    storedPassword = preferences.getString(kPreferenceKeyPassword, "");
}

bool WifiManager::credentialsAvailable() const {
    return !storedSSID.isEmpty() && !storedPassword.isEmpty();
}

void WifiManager::connectToWiFi() {
    if (!credentialsAvailable()) {
        startAccessPoint();
        return;
    }

    Serial.print("Connecting to Wi-Fi network '");
    Serial.print(storedSSID);
    Serial.println("'");

    WiFi.begin(storedSSID.c_str(), storedPassword.c_str());
    lastReconnectAttempt = millis();

    const unsigned long startAttemptTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 10000) {
        delay(100);
    }

    if (WiFi.status() == WL_CONNECTED) {
        apModeActive = false;
        Serial.print("Connected, IP address: ");
        Serial.println(WiFi.localIP());
    } else {
        Serial.println("Failed to connect, falling back to AP mode");
        startAccessPoint();
    }
}

void WifiManager::startAccessPoint() {
    // AP+STA so the web server can scan for networks while the AP is up
    WiFi.mode(WIFI_AP_STA);
    WiFi.disconnect();
    const bool success = WiFi.softAP(kApSSID, kApPassword);
    apModeActive = success;
    Serial.println(success ? "Access point started." : "Failed to start access point.");
    if (success) {
        Serial.print("AP IP address: ");
        Serial.println(WiFi.softAPIP());
        // Answer every DNS query with the AP address: phones detect the
        // captive portal through this and open the config page on connect.
        dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
        dnsServer.start(kDnsPort, "*", WiFi.softAPIP());
    }
}

void WifiManager::stopAccessPoint() {
    if (!apModeActive) {
        return;
    }
    dnsServer.stop();
    WiFi.softAPdisconnect(true);
    apModeActive = false;
}