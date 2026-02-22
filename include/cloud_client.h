#ifndef CLOUD_CLIENT_H
#define CLOUD_CLIENT_H

#include <Arduino.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include "state.h"

/**
 * @brief Cloud client for Azure Function integration
 * 
 * Handles communication with Azure Function backend:
 * - Long-polling for commands from cloud
 * - Pushing motor state to cloud
 * - Configuration management via NVS
 */
class CloudClient {
public:
    CloudClient();
    
    /**
     * @brief Initialize cloud client and load configuration
     */
    void begin();
    
    /**
     * @brief Main update loop - handles polling and state updates
     */
    void handle();
    
    /**
     * @brief Check if cloud client is enabled
     */
    bool isEnabled() const { return enabled_; }
    
    /**
     * @brief Enable or disable cloud sync
     */
    void setEnabled(bool enabled);
    
    /**
     * @brief Get cloud configuration
     */
    void getConfig(String& apiEndpoint, String& apiKey, bool& enabled) const;
    
    /**
     * @brief Set cloud configuration
     * @return true if configuration was saved successfully
     */
    bool setConfig(const String& apiEndpoint, const String& apiKey, bool enabled);
    
    /**
     * @brief Test connection to cloud backend
     * @return true if connection successful
     */
    bool testConnection();
    
    /**
     * @brief Set motor state to push to cloud
     */
    void setMotorState(const MotorState& state);
    
    /**
     * @brief Check if there's a pending command
     */
    bool hasCommand() const { return hasPendingCommand_; }
    
    /**
     * @brief Get pending command (clears the command)
     */
    String getCommand();
    
private:
    // Configuration
    Preferences preferences_;
    String apiEndpoint_;
    String apiKey_;
    bool enabled_;
    
    // State management
    MotorState currentState_;
    unsigned long lastStatePush_;
    unsigned long lastCommandPoll_;
    
    // Command handling
    bool hasPendingCommand_;
    String pendingCommand_;
    
    // HTTP client
    HTTPClient http_;
    
    // Timing constants (in milliseconds)
    static constexpr unsigned long STATE_PUSH_INTERVAL = 2000;  // 2 seconds
    static constexpr unsigned long COMMAND_POLL_INTERVAL = 100; // Check every 100ms if we should poll
    
    /**
     * @brief Load configuration from NVS
     */
    void loadConfig();
    
    /**
     * @brief Save configuration to NVS
     */
    bool saveConfig();
    
    /**
     * @brief Push current motor state to cloud
     */
    void pushState();
    
    /**
     * @brief Poll for commands from cloud (long poll with 30s timeout)
     */
    void pollCommands();
    
    /**
     * @brief Send HTTP request with API key authentication
     */
    int sendRequest(const String& endpoint, const String& method, const String& payload = "");
};

#endif // CLOUD_CLIENT_H
