// Exercise actual production keymap hooks, not a second copy of their policy.
#include "qmk_fixture.h"
#include <assert.h>
#include <stdio.h>
#define QMK_KEYBOARD_H "keymap_stub.h"
#include "../firmware/aurora_via/keymap.c"
layer_state_t layer_state;
void gpio_set_pin_output(uint32_t pin) { assert(pin==LED_FN_PIN); }
void rgb_matrix_set_suspend_state(bool suspend) { (void)suspend; }
static void press(uint16_t key) {
    keyrecord_t r={.event={.key={.row=5,.col=1},.pressed=true}};
    assert(!process_record_user(key,&r));r.event.pressed=false;assert(!process_record_user(key,&r));
}
static void fn(bool held) { layer_state=layer_state_set_user(held?2:0);housekeeping_task_user(); }
int main(void) {
    keyboard_pre_init_user();keyboard_post_init_user();assert(indicator_pin_high);
    fn(true);assert(indicator_pin_high); // Fn alone must not claim Windows lock.
    press(RB_GAME);assert(redblade_game_mode() && !indicator_pin_high);
    fn(false);assert(!indicator_pin_high); // Releasing Fn must keep lock status.
    fn(true);press(RB_DARK);assert(!indicator_pin_high); // Dedicated status is independent of RGB Off.
    mods=MOD_MASK_SHIFT;press(RB_GAME);mods=0;assert(indicator_pin_high && !redblade_game_mode());
    press(RB_GAME);press(RB_DEFAULTS);assert(indicator_pin_high && !redblade_game_mode());
    press(RB_GAME);suspend_power_down_user();assert(indicator_pin_high && !redblade_game_mode());
    fn(true);suspend_wakeup_init_user();assert(indicator_pin_high);
    press(RB_GAME);press(RB_BOOT);assert(indicator_pin_high && !redblade_game_mode());
    puts("PASS: actual keymap hooks keep dedicated Windows indicator locked-only across Fn, RGB Off, defaults, suspend/wake and boot");
}
