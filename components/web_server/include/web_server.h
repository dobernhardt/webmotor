#pragma once

#include "esp_err.h"

// Initialize and start the web server
esp_err_t web_server_init(void);

// Start mDNS service
esp_err_t web_server_start_mdns(void);