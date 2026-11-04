RGB_MATRIX_EFFECT(REDBLADE_DIRECT)
#ifdef RGB_MATRIX_CUSTOM_EFFECT_IMPLS
#include "redblade_core.h"
static bool REDBLADE_DIRECT(effect_params_t *params) {
    RGB_MATRIX_USE_LIMITS(led_min,led_max);
    if(params->iter==0) redblade_core_begin_frame();
    for(uint8_t i=led_min;i<led_max;++i) {
        const rgb_t rgb=hsv_to_rgb_nocie(redblade_core_color(i));
        rgb_matrix_set_color(i,rgb.r,rgb.g,rgb.b);
    }
    return rgb_matrix_check_finished_leds(led_max);
}
#endif
