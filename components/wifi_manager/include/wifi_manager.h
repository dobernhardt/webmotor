#pragma once

#include "esp_err.h"

typedef void (*wifi_connected_handler_t)(void);
typedef void (*wifi_disconnected_handler_t)(void);

typedef struct {
    char ssid[32];
    char password[64];
} wifi_config_data_t;

// Initialize WiFi manager
esp_err_t wifi_manager_init(void);

// Start WiFi manager
esp_err_t wifi_manager_start(wifi_connected_handler_t connected_handler,
                           wifi_disconnected_handler_t disconnected_handler);

// Set WiFi configuration
esp_err_t wifi_manager_set_config(const char *ssid, const char *password);

// Get current WiFi configuration
esp_err_t wifi_manager_get_config(wifi_config_data_t *config);

// Start AP mode for configuration
esp_err_t wifi_manager_start_ap(void);

// Check if device is currently connected to WiFi
bool wifi_manager_is_connected(void);

// Get current IP address as string
const char *wifi_manager_get_ip(void);