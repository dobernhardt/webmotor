#include "tmc2209.h"
#include "driver/gpio.h"
#include "driver/rmt_tx.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "tmc2209";

static struct {
    tmc2209_config_t config;
    tmc2209_microstep_t current_microstep;
    uint32_t current_freq;
    bool current_dir;
    tmc2209_mode_t current_mode;
    rmt_channel_handle_t rmt_chan;
    rmt_encoder_handle_t rmt_encoder;
} tmc2209_state;

static esp_err_t init_rmt(void) {
    rmt_tx_channel_config_t tx_chan_config = {
        .gpio_num = tmc2209_state.config.pin_step,
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10000000, // 10MHz resolution
        .mem_block_symbols = 64,
        .trans_queue_depth = 4,
    };
    
    ESP_ERROR_CHECK(rmt_new_tx_channel(&tx_chan_config, &tmc2209_state.rmt_chan));
    ESP_ERROR_CHECK(rmt_enable(tmc2209_state.rmt_chan));
    return ESP_OK;
}

esp_err_t tmc2209_init(const tmc2209_config_t *config) {
    ESP_LOGI(TAG, "Initializing TMC2209 driver");
    
    // Store configuration
    memcpy(&tmc2209_state.config, config, sizeof(tmc2209_config_t));
    
    // Configure GPIO pins
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << config->pin_step) |
                        (1ULL << config->pin_dir) |
                        (1ULL << config->pin_en) |
                        (1ULL << config->pin_ms1) |
                        (1ULL << config->pin_ms2),
        .pull_down_en = 0,
        .pull_up_en = 0
    };
    
    ESP_ERROR_CHECK(gpio_config(&io_conf));
    
    // Set initial state
    tmc2209_state.current_microstep = TMC_MICROSTEP_16;
    tmc2209_state.current_freq = 0;
    tmc2209_state.current_dir = true;
    tmc2209_state.current_mode = TMC_MODE_STOPPED;
    
    // Apply initial settings
    tmc2209_set_microstep(tmc2209_state.current_microstep);
    tmc2209_set_direction(tmc2209_state.current_dir);
    gpio_set_level(config->pin_en, 1); // Enable motor driver
    
    // Initialize RMT peripheral
    ESP_ERROR_CHECK(init_rmt());
    
    ESP_LOGI(TAG, "TMC2209 driver initialized");
    return ESP_OK;
}

esp_err_t tmc2209_set_microstep(tmc2209_microstep_t microstep) {
    if (microstep > TMC_MICROSTEP_16) {
        return ESP_ERR_INVALID_ARG;
    }
    
    gpio_set_level(tmc2209_state.config.pin_ms1, microstep & 0x01);
    gpio_set_level(tmc2209_state.config.pin_ms2, (microstep >> 1) & 0x01);
    tmc2209_state.current_microstep = microstep;
    
    ESP_LOGI(TAG, "Set microstep mode to 1/%d", 1 << microstep);
    return ESP_OK;
}

tmc2209_microstep_t tmc2209_get_microstep(void) {
    return tmc2209_state.current_microstep;
}

esp_err_t tmc2209_set_frequency(uint32_t freq_hz) {
    if (freq_hz > 10000) { // Max 10kHz stepping frequency
        return ESP_ERR_INVALID_ARG;
    }
    
    if (freq_hz > 0 && tmc2209_state.current_mode == TMC_MODE_RUNNING) {
        // Calculate timing. For 10MHz clock (100ns resolution)
        // Pulse width = 1µs = 10 ticks
        rmt_symbol_word_t symbol = {
            .level0 = 1,
            .duration0 = 10,  // 1µs pulse width
            .level1 = 0,
            .duration1 = (10000000 / freq_hz) - 10  // Remaining period
        };
        
        rmt_transmit_config_t tx_config = {
            .loop_count = -1, // Infinite transmission
        };
        
        ESP_ERROR_CHECK(rmt_transmit(tmc2209_state.rmt_chan, 
                                   &symbol,
                                   sizeof(rmt_symbol_word_t),
                                   &tx_config));
    } else {
        ESP_ERROR_CHECK(rmt_disable(tmc2209_state.rmt_chan));
    }
    
    tmc2209_state.current_freq = freq_hz;
    ESP_LOGI(TAG, "Set frequency to %d Hz", freq_hz);
    return ESP_OK;
}

uint32_t tmc2209_get_frequency(void) {
    return tmc2209_state.current_freq;
}

esp_err_t tmc2209_set_direction(bool clockwise) {
    gpio_set_level(tmc2209_state.config.pin_dir, clockwise);
    tmc2209_state.current_dir = clockwise;
    ESP_LOGI(TAG, "Set direction to %s", clockwise ? "clockwise" : "counterclockwise");
    return ESP_OK;
}

bool tmc2209_get_direction(void) {
    return tmc2209_state.current_dir;
}

esp_err_t tmc2209_set_mode(tmc2209_mode_t mode) {
    switch (mode) {
        case TMC_MODE_STOPPED:
            tmc2209_state.current_freq = 0;
            gpio_set_level(tmc2209_state.config.pin_en, 1);
            ESP_ERROR_CHECK(rmt_disable(tmc2209_state.rmt_chan));
            break;
        case TMC_MODE_RUNNING:
            gpio_set_level(tmc2209_state.config.pin_en, 1);
            if (tmc2209_state.current_freq > 0) {
                ESP_ERROR_CHECK(rmt_enable(tmc2209_state.rmt_chan));
                tmc2209_set_frequency(tmc2209_state.current_freq);
            }
            break;
        case TMC_MODE_RELEASED:
            tmc2209_state.current_freq = 0;
            gpio_set_level(tmc2209_state.config.pin_en, 0);
            ESP_ERROR_CHECK(rmt_disable(tmc2209_state.rmt_chan));
            break;
        default:
            return ESP_ERR_INVALID_ARG;
    }
    
    tmc2209_state.current_mode = mode;
    ESP_LOGI(TAG, "Set mode to %d", mode);
    return ESP_OK;
}

tmc2209_mode_t tmc2209_get_mode(void) {
    return tmc2209_state.current_mode;
}