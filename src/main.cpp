#include <Arduino.h>
#include <FastLED.h>
#include "wifi_manager.h"
#include "web_server.h"
#include "motor_controller_tmc2209.h"

// ATOM S3 Lite RGB LED configuration
#define LED_PIN 35
#define NUM_LEDS 1
CRGB leds[NUM_LEDS];

// LED State Management
enum LEDState {
    LED_BOOT,
    LED_WIFI_AP,
    LED_WIFI_CONNECTED,
    LED_WIFI_DISCONNECTED,
    LED_MOTOR_IDLE,
    LED_MOTOR_RUNNING_FORWARD,
    LED_MOTOR_RUNNING_REVERSE,
    LED_MOTOR_STOPPED,
    LED_MOTOR_RELEASED,
    LED_ERROR,
    LED_HEARTBEAT
};

struct LEDPattern {
    CRGB color1;
    CRGB color2;
    int pulseSpeed;    // ms for pulse cycle
    bool isSolid;      // true for solid color, false for pattern
    bool isDualColor;  // true for alternating colors
};

// LED Pattern definitions
LEDPattern ledPatterns[] = {
    // LED_BOOT
    {CRGB::Red, CRGB::Black, 100, false, false},        // Fast red blink
    // LED_WIFI_AP  
    {CRGB::Orange, CRGB::Blue, 1000, false, true},      // Orange/Blue alternating (AP mode)
    // LED_WIFI_CONNECTED
    {CRGB::Green, CRGB::Black, 2000, false, false},     // Slow green pulse (connected)
    // LED_WIFI_DISCONNECTED
    {CRGB::Red, CRGB::Black, 500, false, false},        // Medium red blink (disconnected)
    // LED_MOTOR_IDLE
    {CRGB::Blue, CRGB::Black, 3000, false, false},      // Very slow blue pulse (idle)
    // LED_MOTOR_RUNNING_FORWARD
    {CRGB::Green, CRGB::Black, 200, false, false},      // Fast green pulse (forward)
    // LED_MOTOR_RUNNING_REVERSE
    {CRGB::Purple, CRGB::Black, 200, false, false},     // Fast purple pulse (reverse)
    // LED_MOTOR_STOPPED
    {CRGB::Yellow, CRGB::Black, 0, true, false},        // Solid yellow (stopped)
    // LED_MOTOR_RELEASED
    {CRGB::Cyan, CRGB::Black, 1000, false, false},      // Slow cyan pulse (released)
    // LED_ERROR
    {CRGB::Red, CRGB::White, 150, false, true},         // Fast red/white alternating (error)
    // LED_HEARTBEAT
    {CRGB(10, 0, 10), CRGB::Black, 100, false, false}   // Dim purple quick pulse
};

LEDState currentLEDState = LED_BOOT;
unsigned long lastLEDUpdate = 0;
bool ledPhase = false;

WifiManager wifiManager;
WebServerController webServer;
MotorControllerTMC2209 motorController;

void updateLED() {
    unsigned long currentTime = millis();
    LEDPattern pattern = ledPatterns[currentLEDState];
    
    if (pattern.isSolid) {
        // Solid color
        leds[0] = pattern.color1;
        FastLED.show();
        return;
    }
    
    if (currentTime - lastLEDUpdate >= pattern.pulseSpeed) {
        lastLEDUpdate = currentTime;
        ledPhase = !ledPhase;
        
        if (pattern.isDualColor) {
            // Alternating between two colors
            leds[0] = ledPhase ? pattern.color1 : pattern.color2;
        } else {
            // Pulsing single color
            leds[0] = ledPhase ? pattern.color1 : CRGB::Black;
        }
        
        FastLED.show();
    }
}

void setLEDState(LEDState newState) {
    if (currentLEDState != newState) {
        currentLEDState = newState;
        lastLEDUpdate = 0; // Force immediate update
        ledPhase = false;
        
        // Log state changes for debugging
        const char* stateNames[] = {
            "BOOT", "WIFI_AP", "WIFI_CONNECTED", "WIFI_DISCONNECTED",
            "MOTOR_IDLE", "MOTOR_RUNNING_FORWARD", "MOTOR_RUNNING_REVERSE", 
            "MOTOR_STOPPED", "MOTOR_RELEASED", "ERROR", "HEARTBEAT"
        };
        Serial.print("[LED] State changed to: ");
        Serial.println(stateNames[newState]);
    }
}

void blinkRGB(CRGB color, int count, int delayMs) {
    LEDState oldState = currentLEDState;
    for (int i = 0; i < count; i++) {
        leds[0] = color;
        FastLED.show();
        delay(delayMs);
        leds[0] = CRGB::Black;
        FastLED.show();
        delay(delayMs);
    }
    currentLEDState = oldState; // Restore previous state
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    // Initialize ATOM S3 Lite RGB LED
    FastLED.addLeds<WS2812, LED_PIN, GRB>(leds, NUM_LEDS);
    FastLED.setBrightness(50); // Lower brightness for embedded device
    
    // Boot sequence
    Serial.println("[BOOT] WebMotor Controller starting...");
    setLEDState(LED_BOOT);
    
    // Show boot pattern for 2 seconds
    for (int i = 0; i < 200; i++) {
        updateLED();
        delay(10);
    }
    
    // Initialize Wi-Fi Manager
    Serial.println("[INIT] Starting Wi-Fi Manager...");
    blinkRGB(CRGB::Blue, 2, 300);
    wifiManager.begin();
    Serial.println("[INIT] Wi-Fi Manager ready");
    
    // Initialize Motor Controller
    Serial.println("[INIT] Starting Motor Controller...");
    blinkRGB(CRGB::Green, 3, 300);
    motorController.begin();
    Serial.println("[INIT] Motor Controller ready");
    
    // Initialize Web Server
    Serial.println("[INIT] Starting Web Server...");
    blinkRGB(CRGB::Yellow, 4, 300);
    webServer.begin(motorController, wifiManager);
    Serial.println("[INIT] Web Server ready");
    
    // Success indication
    Serial.println("[BOOT] System initialization complete!");
    blinkRGB(CRGB::White, 3, 200);
    
    // Set initial operational state
    setLEDState(LED_MOTOR_IDLE);
}

void loop() {
    static unsigned long lastHeartbeat = 0;
    static unsigned long lastStatusReport = 0;
    static unsigned long heartbeatEnd = 0;
    static unsigned long loopCount = 0;
    static LEDState lastKnownState = LED_BOOT;
    
    // Handle system tasks
    wifiManager.handle();
    webServer.handle();
    
    loopCount++;
    unsigned long currentTime = millis();
    
    // Update LED state based on current system status (only when not in heartbeat)
    if (currentTime > heartbeatEnd) {
        LEDState newState = LED_MOTOR_IDLE; // Default state
        
        // Determine current state priority (highest priority first)
        if (!wifiManager.isConnected() && !wifiManager.isAccessPoint()) {
            newState = LED_WIFI_DISCONNECTED;
        } else if (wifiManager.isAccessPoint()) {
            newState = LED_WIFI_AP;
        } else {
            // Get current motor state
            MotorState motorState = motorController.getMotorState();
            
            if (motorState.mode == MotorMode::RUNNING) {
                // Check direction for forward/reverse indication
                newState = motorState.direction ? LED_MOTOR_RUNNING_FORWARD : LED_MOTOR_RUNNING_REVERSE;
            } else if (motorState.mode == MotorMode::STOPPED) {
                newState = LED_MOTOR_STOPPED;
            } else if (motorState.mode == MotorMode::RELEASED) {
                newState = LED_MOTOR_RELEASED;
            } else if (wifiManager.isConnected()) {
                newState = LED_WIFI_CONNECTED;
            }
        }
        
        setLEDState(newState);
        lastKnownState = newState;
    }
    
    // Heartbeat override - brief pulse every 10 seconds
    if (currentTime - lastHeartbeat >= 10000) {
        lastHeartbeat = currentTime;
        heartbeatEnd = currentTime + 200; // 200ms heartbeat pulse
        
        Serial.print("[HEARTBEAT] Loop #");
        Serial.println(loopCount);
        
        // Temporary heartbeat state
        setLEDState(LED_HEARTBEAT);
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
        
        if (wifiManager.isAccessPoint()) {
            Serial.println("WiFi: Access Point Mode");
        } else if (wifiManager.isConnected()) {
            Serial.print("WiFi: Connected to ");
            Serial.println(wifiManager.getSSID());
        } else {
            Serial.println("WiFi: Disconnected");
        }
        
        MotorState motorState = motorController.getMotorState();
        Serial.print("Motor State: ");
        switch(motorState.mode) {
            case MotorMode::RUNNING:
                Serial.print("Running ");
                Serial.print(motorState.direction ? "Forward" : "Reverse");
                Serial.print(", Freq: ");
                Serial.print(motorState.frequency);
                Serial.println(" Hz");
                break;
            case MotorMode::STOPPED:
                Serial.println("Stopped");
                break;
            case MotorMode::RELEASED:
                Serial.println("Released");
                break;
        }
        
        Serial.println("=== END STATUS ===");
    }
    
    // Update LED based on current state
    updateLED();
    
    // Efficient delay for ATOM S3 Lite
    delay(10);
}