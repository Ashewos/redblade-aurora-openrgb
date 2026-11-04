#include "qmk_fixture.h"
#include <assert.h>
#include <stdio.h>
#define QMK_KEYBOARD_H "keymap_stub.h"
#include "../firmware/openrgb_core/keymap.c"
layer_state_t layer_state;
void gpio_set_pin_output(uint32_t pin) { assert(pin==LED_FN_PIN); }
void rgb_matrix_set_suspend_state(bool suspend) { (void)suspend; }
int main(void) {
    keyboard_pre_init_user();keyboard_post_init_user();assert(indicator_pin_high);
    layer_state=layer_state_set_user(2);housekeeping_task_user();assert(indicator_pin_high);
    keyrecord_t key={.event={.key={.row=5,.col=1},.pressed=true}};
    assert(process_record_user(KC_LGUI,&key));assert(indicator_pin_high);
    suspend_power_down_user();suspend_wakeup_init_user();assert(indicator_pin_high);
    layer_state=layer_state_set_user(0);assert(indicator_pin_high);
    puts("PASS: actual Core keymap leaves dedicated Windows indicator off without a lock feature");
}
