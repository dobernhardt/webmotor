#ifndef CLOUD_CLIENT_H
#define CLOUD_CLIENT_H

#include <Arduino.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
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
     * This will start/stop the background polling task
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
    SemaphoreHandle_t commandMutex_;  // Protect command access
    
    // HTTP client
    HTTPClient http_;
    HTTPClient pollHttp_;  // Separate HTTP client for polling task
    
    // FreeRTOS task for background polling
    TaskHandle_t pollTaskHandle_;
    volatile bool pollTaskRunning_;
    
    // Timing constants (in milliseconds)
    static constexpr unsigned long STATE_PUSH_INTERVAL = 2000;  // 2 seconds
    static constexpr unsigned long COMMAND_POLL_INTERVAL = 0;    // Not used with task-based polling
    
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
     * @brief Poll for commands from cloud (called by background task)
     */
    void pollCommands();
    
    /**
     * @brief Start background polling task
     */
    void startPollTask();
    
    /**
     * @brief Stop background polling task
     */
    void stopPollTask();
    
    /**
     * @brief Static task function for FreeRTOS
     */
    static void pollTaskFunction(void* parameter);
    
    /**
     * @brief Send HTTP request with API key authentication
     */
    int sendRequest(const String& endpoint, const String& method, const String& payload = "");
};

#endif // CLOUD_CLIENT_H
