#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "tmc2209.h"
#include "wifi_manager.h"
#include "web_server.h"

static const char *TAG = "main";

// Pin definitions for TMC2209
#define PIN_STEP 4
#define PIN_DIR  5
#define PIN_EN   6
#define PIN_MS1  7
#define PIN_MS2  8

// WiFi connection callback
static void wifi_connected_handler(void)
{
    ESP_LOGI(TAG, "WiFi connected, starting web server");
    ESP_ERROR_CHECK(web_server_init());
    ESP_ERROR_CHECK(web_server_start_mdns());
}

// WiFi disconnection callback
static void wifi_disconnected_handler(void)
{
    ESP_LOGI(TAG, "WiFi disconnected");
}

void app_main(void)
{
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize TMC2209
    tmc2209_config_t motor_config = {
        .pin_step = PIN_STEP,
        .pin_dir = PIN_DIR,
        .pin_en = PIN_EN,
        .pin_ms1 = PIN_MS1,
        .pin_ms2 = PIN_MS2
    };
    ESP_ERROR_CHECK(tmc2209_init(&motor_config));

    // Initialize WiFi manager
    ESP_ERROR_CHECK(wifi_manager_init());
    ESP_ERROR_CHECK(wifi_manager_start(wifi_connected_handler, wifi_disconnected_handler));

    ESP_LOGI(TAG, "WebMotor initialization complete");
}