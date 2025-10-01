#include "web_server.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "mdns.h"
#include "cJSON.h"
#include "tmc2209.h"
#include "wifi_manager.h"

static const char *TAG = "web_server";
static httpd_handle_t server = NULL;

// Embedded files declarations
extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[] asm("_binary_index_html_end");
extern const uint8_t style_css_start[] asm("_binary_style_css_start");
extern const uint8_t style_css_end[] asm("_binary_style_css_end");
extern const uint8_t script_js_start[] asm("_binary_script_js_start");
extern const uint8_t script_js_end[] asm("_binary_script_js_end");

// Handler for root path (serves index.html)
static esp_err_t root_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, (const char *)index_html_start, index_html_end - index_html_start);
}

// Handler for style.css
static esp_err_t style_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/css");
    return httpd_resp_send(req, (const char *)style_css_start, style_css_end - style_css_start);
}

// Handler for script.js
static esp_err_t script_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/javascript");
    return httpd_resp_send(req, (const char *)script_js_start, script_js_end - script_js_start);
}

// Handler for motor status GET
static esp_err_t motor_status_handler(httpd_req_t *req)
{
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "microsteps", 1 << tmc2209_get_microstep());
    cJSON_AddNumberToObject(root, "frequency", tmc2209_get_frequency());
    cJSON_AddBoolToObject(root, "direction", tmc2209_get_direction());
    cJSON_AddNumberToObject(root, "mode", tmc2209_get_mode());

    char *json_str = cJSON_Print(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json_str);

    free(json_str);
    cJSON_Delete(root);
    return ESP_OK;
}

// Handler for motor control POST
static esp_err_t motor_control_handler(httpd_req_t *req)
{
    char buf[100];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        return ESP_FAIL;
    }
    buf[ret] = '\0';

    cJSON *root = cJSON_Parse(buf);
    if (root == NULL) {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
    }

    // Handle microsteps
    cJSON *microsteps = cJSON_GetObjectItem(root, "microsteps");
    if (microsteps != NULL && cJSON_IsNumber(microsteps)) {
        int ms_val = microsteps->valueint;
        tmc2209_microstep_t ms;
        switch (ms_val) {
            case 1: ms = TMC_MICROSTEP_1; break;
            case 2: ms = TMC_MICROSTEP_2; break;
            case 4: ms = TMC_MICROSTEP_4; break;
            case 8: ms = TMC_MICROSTEP_8; break;
            case 16: ms = TMC_MICROSTEP_16; break;
            default: ms = TMC_MICROSTEP_16;
        }
        tmc2209_set_microstep(ms);
    }

    // Handle frequency
    cJSON *frequency = cJSON_GetObjectItem(root, "frequency");
    if (frequency != NULL && cJSON_IsNumber(frequency)) {
        tmc2209_set_frequency(frequency->valueint);
    }

    // Handle direction
    cJSON *direction = cJSON_GetObjectItem(root, "direction");
    if (direction != NULL && cJSON_IsBool(direction)) {
        tmc2209_set_direction(cJSON_IsTrue(direction));
    }

    // Handle mode
    cJSON *mode = cJSON_GetObjectItem(root, "mode");
    if (mode != NULL && cJSON_IsNumber(mode)) {
        tmc2209_set_mode(mode->valueint);
    }

    cJSON_Delete(root);
    httpd_resp_sendstr(req, "{\"status\":\"ok\"}");
    return ESP_OK;
}

// Handler for WiFi configuration
static esp_err_t wifi_config_handler(httpd_req_t *req)
{
    if (req->method == HTTP_GET) {
        wifi_config_data_t config;
        esp_err_t err = wifi_manager_get_config(&config);
        
        cJSON *root = cJSON_CreateObject();
        if (err == ESP_OK) {
            cJSON_AddStringToObject(root, "ssid", config.ssid);
            // Don't send password for security
            cJSON_AddBoolToObject(root, "configured", true);
        } else {
            cJSON_AddBoolToObject(root, "configured", false);
        }
        
        char *json_str = cJSON_Print(root);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, json_str);
        
        free(json_str);
        cJSON_Delete(root);
        return ESP_OK;
    } else if (req->method == HTTP_POST) {
        char buf[100];
        int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
        if (ret <= 0) {
            return ESP_FAIL;
        }
        buf[ret] = '\0';

        cJSON *root = cJSON_Parse(buf);
        if (root == NULL) {
            return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        }

        cJSON *ssid = cJSON_GetObjectItem(root, "ssid");
        cJSON *password = cJSON_GetObjectItem(root, "password");

        if (ssid == NULL || password == NULL || !cJSON_IsString(ssid) || !cJSON_IsString(password)) {
            cJSON_Delete(root);
            return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing ssid or password");
        }

        esp_err_t err = wifi_manager_set_config(ssid->valuestring, password->valuestring);
        cJSON_Delete(root);

        if (err != ESP_OK) {
            return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to save configuration");
        }

        httpd_resp_sendstr(req, "{\"status\":\"ok\",\"message\":\"Configuration saved. Device will reboot.\"}");
        
        // Schedule a reboot
        esp_restart();
        return ESP_OK;
    }

    return ESP_FAIL;
}

esp_err_t web_server_init(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.uri_match_fn = httpd_uri_match_wildcard;

    ESP_LOGI(TAG, "Starting HTTP server");
    esp_err_t ret = httpd_start(&server, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server: %s", esp_err_to_name(ret));
        return ret;
    }

    // URI handlers
    httpd_uri_t uri_root = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = root_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &uri_root);

    httpd_uri_t uri_style = {
        .uri = "/style.css",
        .method = HTTP_GET,
        .handler = style_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &uri_style);

    httpd_uri_t uri_script = {
        .uri = "/script.js",
        .method = HTTP_GET,
        .handler = script_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &uri_script);

    httpd_uri_t uri_motor_status = {
        .uri = "/api/motor/status",
        .method = HTTP_GET,
        .handler = motor_status_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &uri_motor_status);

    httpd_uri_t uri_motor_control = {
        .uri = "/api/motor/control",
        .method = HTTP_POST,
        .handler = motor_control_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &uri_motor_control);

    httpd_uri_t uri_wifi_config = {
        .uri = "/api/wifi/config",
        .method = HTTP_GET,
        .handler = wifi_config_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &uri_wifi_config);

    httpd_uri_t uri_wifi_config_post = {
        .uri = "/api/wifi/config",
        .method = HTTP_POST,
        .handler = wifi_config_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &uri_wifi_config_post);

    return ESP_OK;
}

esp_err_t web_server_start_mdns(void)
{
    esp_err_t err = mdns_init();
    if (err) {
        ESP_LOGE(TAG, "MDNS Init failed: %s", esp_err_to_name(err));
        return err;
    }

    mdns_hostname_set("webmotor");
    mdns_instance_name_set("WebMotor Stepper Controller");

    mdns_txt_item_t serviceTxtData[] = {
        {"board", "esp32"},
        {"path", "/"}
    };

    err = mdns_service_add("WebMotor", "_http", "_tcp", 80, serviceTxtData,
                          sizeof(serviceTxtData) / sizeof(serviceTxtData[0]));
    if (err) {
        ESP_LOGE(TAG, "MDNS service add failed: %s", esp_err_to_name(err));
        return err;
    }

    return ESP_OK;
}