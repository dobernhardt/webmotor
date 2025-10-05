#include <Arduino.h>
#include <FastLED.h>
#include "wifi_manager.h"
#include "web_server.h"
#include "motor_controller.h"

// ATOM S3 Lite RGB LED configuration
#define LED_PIN 35
#define NUM_LEDS 1
CRGB leds[NUM_LEDS];

WifiManager wifiManager;
WebServerController webServer;
MotorController motorController;

void blinkRGB(CRGB color, int count, int delayMs) {
    for (int i = 0; i < count; i++) {
        leds[0] = color;
        FastLED.show();
        delay(delayMs);
        leds[0] = CRGB::Black;
        FastLED.show();
        delay(delayMs);
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    // Initialize ATOM S3 Lite RGB LED
    FastLED.addLeds<WS2812, LED_PIN, GRB>(leds, NUM_LEDS);
    FastLED.setBrightness(50); // Lower brightness for embedded device
    
    // Boot sequence - 5 rapid red blinks to confirm code is running
    Serial.println("[BOOT] WebMotor Controller starting...");
    blinkRGB(CRGB::Red, 5, 100);
    
    delay(500);
    
    // Initialize Wi-Fi Manager - 2 blue blinks
    Serial.println("[INIT] Starting Wi-Fi Manager...");
    blinkRGB(CRGB::Blue, 2, 300);
    wifiManager.begin();
    Serial.println("[INIT] Wi-Fi Manager ready");
    
    // Initialize Motor Controller - 3 green blinks
    Serial.println("[INIT] Starting Motor Controller...");
    blinkRGB(CRGB::Green, 3, 300);
    motorController.begin();
    Serial.println("[INIT] Motor Controller ready");
    
    // Initialize Web Server - 4 yellow blinks
    Serial.println("[INIT] Starting Web Server...");
    blinkRGB(CRGB::Yellow, 4, 300);
    webServer.begin(motorController, wifiManager);
    Serial.println("[INIT] Web Server ready");
    
    // Success - solid white for 1 second
    Serial.println("[BOOT] System initialization complete!");
    leds[0] = CRGB::White;
    FastLED.show();
    delay(1000);
    leds[0] = CRGB::Black;
    FastLED.show();
}

void loop() {
    static unsigned long lastHeartbeat = 0;
    static unsigned long lastStatusReport = 0;
    static unsigned long loopCount = 0;
    
    // Handle system tasks
    wifiManager.handle();
    webServer.handle();
    
    loopCount++;
    
    unsigned long currentTime = millis();
    
    // Heartbeat - gentle purple pulse every 10 seconds
    if (currentTime - lastHeartbeat >= 10000) {
        lastHeartbeat = currentTime;
        Serial.print("[HEARTBEAT] Loop #");
        Serial.println(loopCount);
        
        // Gentle purple pulse
        leds[0] = CRGB(50, 0, 50);
        FastLED.show();
        delay(100);
        leds[0] = CRGB::Black;
        FastLED.show();
    }
    
    // Status report every 60 seconds
    if (currentTime - lastStatusReport >= 60000) {
        lastStatusReport = currentTime;
        
        Serial.println("=== STATUS REPORT ===");
        Serial.print("Uptime: ");
        Serial.print(currentTime / 1000);
        Serial.println(" seconds");
        Serial.print("Free Heap: ");
        Serial.print(ESP.getFreeHeap());
        Serial.println(" bytes");
        
        // WiFi status with colored indication
        if (wifiManager.isAccessPoint()) {
            Serial.println("WiFi: Access Point Mode");
            blinkRGB(CRGB::Orange, 1, 200); // Orange for AP mode
        } else if (wifiManager.isConnected()) {
            Serial.print("WiFi: Connected to ");
            Serial.println(wifiManager.getSSID());
            blinkRGB(CRGB::Cyan, 1, 200); // Cyan for connected
        } else {
            Serial.println("WiFi: Disconnected");
            blinkRGB(CRGB::Red, 1, 200); // Red for disconnected
        }
        
        Serial.println("=== END STATUS ===");
    }
    
    // Efficient delay for ATOM S3 Lite
    delay(10);
}