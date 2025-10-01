#include "wifi_manager.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "freertos/event_groups.h"
#include <string.h>

static const char *TAG = "wifi_manager";

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static EventGroupHandle_t s_wifi_event_group;
static esp_netif_t *s_sta_netif = NULL;
static esp_netif_t *s_ap_netif = NULL;
static wifi_connected_handler_t s_connected_handler = NULL;
static wifi_disconnected_handler_t s_disconnected_handler = NULL;
static char s_ip_addr[16] = {0};

static void event_handler(void* arg, esp_event_base_t event_base,
                         int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_STA_START:
                esp_wifi_connect();
                break;
            case WIFI_EVENT_STA_DISCONNECTED:
                ESP_LOGI(TAG, "WiFi disconnected, trying to reconnect...");
                esp_wifi_connect();
                xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
                if (s_disconnected_handler) {
                    s_disconnected_handler();
                }
                break;
            case WIFI_EVENT_AP_STACONNECTED:
                ESP_LOGI(TAG, "Station connected to AP");
                break;
            case WIFI_EVENT_AP_STADISCONNECTED:
                ESP_LOGI(TAG, "Station disconnected from AP");
                break;
        }
    } else if (event_base == IP_EVENT) {
        if (event_id == IP_EVENT_STA_GOT_IP) {
            ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
            strncpy(s_ip_addr, ip4addr_ntoa(&event->ip_info.ip), sizeof(s_ip_addr)-1);
            ESP_LOGI(TAG, "Got IP: %s", s_ip_addr);
            xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
            if (s_connected_handler) {
                s_connected_handler();
            }
        }
    }
}

esp_err_t wifi_manager_init(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    
    s_sta_netif = esp_netif_create_default_wifi_sta();
    s_ap_netif = esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL));

    ESP_LOGI(TAG, "WiFi manager initialized");
    return ESP_OK;
}

esp_err_t wifi_manager_set_config(const char *ssid, const char *password)
{
    nvs_handle_t nvs;
    esp_err_t ret = nvs_open("storage", NVS_READWRITE, &nvs);
    if (ret != ESP_OK) return ret;

    ret = nvs_set_str(nvs, "wifi_ssid", ssid);
    if (ret != ESP_OK) {
        nvs_close(nvs);
        return ret;
    }

    ret = nvs_set_str(nvs, "wifi_pass", password);
    if (ret != ESP_OK) {
        nvs_close(nvs);
        return ret;
    }

    ret = nvs_commit(nvs);
    nvs_close(nvs);
    
    ESP_LOGI(TAG, "WiFi configuration saved");
    return ret;
}

esp_err_t wifi_manager_get_config(wifi_config_data_t *config)
{
    nvs_handle_t nvs;
    esp_err_t ret = nvs_open("storage", NVS_READONLY, &nvs);
    if (ret != ESP_OK) return ret;

    size_t len = sizeof(config->ssid);
    ret = nvs_get_str(nvs, "wifi_ssid", config->ssid, &len);
    if (ret != ESP_OK) {
        nvs_close(nvs);
        return ret;
    }

    len = sizeof(config->password);
    ret = nvs_get_str(nvs, "wifi_pass", config->password, &len);
    
    nvs_close(nvs);
    return ret;
}

esp_err_t wifi_manager_start(wifi_connected_handler_t connected_handler,
                           wifi_disconnected_handler_t disconnected_handler)
{
    wifi_config_data_t wifi_config;
    esp_err_t ret = wifi_manager_get_config(&wifi_config);
    
    if (ret != ESP_OK) {
        ESP_LOGI(TAG, "No WiFi configuration found, starting AP mode");
        return wifi_manager_start_ap();
    }

    s_connected_handler = connected_handler;
    s_disconnected_handler = disconnected_handler;

    wifi_config_t wifi_sta_config = {
        .sta = {
            .pmf_cfg = {
                .capable = true,
                .required = false
            },
        },
    };
    memcpy(wifi_sta_config.sta.ssid, wifi_config.ssid, sizeof(wifi_sta_config.sta.ssid));
    memcpy(wifi_sta_config.sta.password, wifi_config.password, sizeof(wifi_sta_config.sta.password));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_sta_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "WiFi started in station mode");
    return ESP_OK;
}

esp_err_t wifi_manager_start_ap(void)
{
    wifi_config_t wifi_ap_config = {
        .ap = {
            .ssid = "WebMotor-Config",
            .ssid_len = strlen("WebMotor-Config"),
            .max_connection = 4,
            .authmode = WIFI_AUTH_OPEN  // Changed from WIFI_AUTH_WPA_WPA2_PSK to WIFI_AUTH_OPEN
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_ap_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "WiFi started in AP mode (open network)");
    return ESP_OK;
}

bool wifi_manager_is_connected(void)
{
    return (xEventGroupGetBits(s_wifi_event_group) & WIFI_CONNECTED_BIT) != 0;
}

const char *wifi_manager_get_ip(void)
{
    return s_ip_addr;
}