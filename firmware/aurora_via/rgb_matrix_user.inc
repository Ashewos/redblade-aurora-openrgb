// SPDX-License-Identifier: GPL-2.0-or-later
RGB_MATRIX_EFFECT(REDBLADE_DIRECT)
#ifdef RGB_MATRIX_CUSTOM_EFFECT_IMPLS
#include "redblade_rgb.h"
static bool REDBLADE_DIRECT(effect_params_t *params) {
    RGB_MATRIX_USE_LIMITS(led_min, led_max);
    for (uint8_t i = led_min; i < led_max; ++i) {
        rgb_t rgb = rgb_matrix_hsv_to_rgb(redblade_rgb_color(i));
        rgb_matrix_set_color(i, rgb.r, rgb.g, rgb.b);
    }
    return rgb_matrix_check_finished_leds(led_max);
}
#endif

