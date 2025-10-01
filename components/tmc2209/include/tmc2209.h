#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

typedef struct {
    int pin_step;
    int pin_dir;
    int pin_en;
    int pin_ms1;
    int pin_ms2;
} tmc2209_config_t;

typedef enum {
    TMC_MICROSTEP_1 = 0,    // Full step
    TMC_MICROSTEP_2 = 1,    // Half step
    TMC_MICROSTEP_4 = 2,    // Quarter step
    TMC_MICROSTEP_8 = 3,    // Eighth step
    TMC_MICROSTEP_16 = 4    // Sixteenth step
} tmc2209_microstep_t;

typedef enum {
    TMC_MODE_STOPPED = 0,
    TMC_MODE_RUNNING = 1,
    TMC_MODE_RELEASED = 2
} tmc2209_mode_t;

// Initialize TMC2209 with given configuration
esp_err_t tmc2209_init(const tmc2209_config_t *config);

// Set microstepping mode
esp_err_t tmc2209_set_microstep(tmc2209_microstep_t microstep);

// Get current microstepping mode
tmc2209_microstep_t tmc2209_get_microstep(void);

// Set stepping frequency in Hz
esp_err_t tmc2209_set_frequency(uint32_t freq_hz);

// Get current stepping frequency
uint32_t tmc2209_get_frequency(void);

// Set motor direction (true = clockwise, false = counterclockwise)
esp_err_t tmc2209_set_direction(bool clockwise);

// Get current direction
bool tmc2209_get_direction(void);

// Set motor mode (running, stopped, released)
esp_err_t tmc2209_set_mode(tmc2209_mode_t mode);

// Get current motor mode
tmc2209_mode_t tmc2209_get_mode(void);