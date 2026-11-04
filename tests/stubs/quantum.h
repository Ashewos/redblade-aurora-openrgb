#pragma once
#include <stdbool.h>
#include <stdint.h>
#define RGB_MATRIX_LED_COUNT 109
#define RAW_EPSIZE 32
#define RGB_MATRIX_MAXIMUM_BRIGHTNESS 255
#define RGB_MATRIX_CUSTOM_REDBLADE_DIRECT 91
#define RGB_MATRIX_SOLID_COLOR 1
#include "../../firmware/aurora_via/config.h"
typedef struct { uint8_t h, s, v; } hsv_t;
typedef struct {
    struct { uint8_t x, y; } point[109];
    uint8_t flags[109];
} led_config_t;
extern led_config_t g_led_config;
bool rgb_matrix_is_enabled(void);
uint8_t rgb_matrix_get_mode(void);
uint8_t rgb_matrix_get_speed(void);
uint8_t rgb_matrix_get_hue(void);
uint8_t rgb_matrix_get_sat(void);
uint8_t rgb_matrix_get_val(void);
void rgb_matrix_enable_noeeprom(void);
void rgb_matrix_disable_noeeprom(void);
void rgb_matrix_mode_noeeprom(uint8_t mode);
void rgb_matrix_set_speed_noeeprom(uint8_t speed);
void rgb_matrix_sethsv_noeeprom(uint8_t hue, uint8_t sat, uint8_t val);
uint32_t timer_read32(void);

