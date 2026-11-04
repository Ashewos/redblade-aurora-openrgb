// SPDX-License-Identifier: GPL-2.0-or-later
#include "redblade_controls.h"
#include "redblade_rgb.h"

_Static_assert(RB_GAME == 0x7E40 && RB_BOOT == 0x7E53, "VIA custom keycode ABI changed");
_Static_assert(RB_TURBO_RATE == 0x7E58, "Game helper keycode ABI changed");
static bool game_mode, boot_armed, feedback_active;
static uint32_t boot_started_at, feedback_started_at;
static keypos_t boot_key;
static uint8_t media_held;
static struct { uint8_t code, row, col; } media[3];
static uint8_t turbo_key, turbo_half_period_ms = 10, physical_targets, trigger_held;
static bool turbo_down, auto_w;
static uint32_t turbo_edge_at;
static keypos_t trigger[2];

uint8_t redblade_turbo_key(void) { return turbo_key; }
uint8_t redblade_turbo_rate(void) { return turbo_half_period_ms == 20 ? 25 : turbo_half_period_ms == 10 ? 50 : 100; }
bool redblade_auto_w(void) { return auto_w; }

static uint8_t physical_bit(uint8_t key) {
    return key == KC_E ? 1 : key == KC_F ? 2 : key == KC_SPC ? 4 : key == KC_W ? 8 : 0;
}

static void stop_turbo(void) {
    if (turbo_down) unregister_code(turbo_key);
    turbo_key = 0;
    turbo_down = false;
}

static void stop_w(void) {
    if (auto_w) unregister_code(KC_W);
    auto_w = false;
}

static void stop_automation(void) { stop_turbo(); stop_w(); }

static void toggle_turbo(uint8_t key, keypos_t position) {
    trigger[0] = position; trigger_held |= 1;
    if (turbo_key == key) { stop_turbo(); return; }
    // Do not take ownership from a physically held target key.
    if (physical_targets & physical_bit(key)) return;
    stop_turbo();
    turbo_key = key; turbo_down = true; turbo_edge_at = timer_read32();
    register_code(key);
}

bool redblade_game_mode(void) { return game_mode; }

static void game_set(bool enabled) {
    game_mode = enabled;
    if (enabled) {
        // Release already-held GUI modifiers without disturbing Ctrl/Alt/Shift.
        del_mods(MOD_MASK_GUI);
        del_weak_mods(MOD_MASK_GUI);
        send_keyboard_report();
    }
}

void redblade_controls_init(void) {
    stop_automation();
    game_mode = boot_armed = feedback_active = false;
    media_held = 0;
    turbo_half_period_ms = 10;
    physical_targets = trigger_held = 0;
}

void redblade_controls_suspend(void) {
    boot_armed = false;
    feedback_active = false;
    game_mode = false;
    stop_automation();
    physical_targets = trigger_held = 0;
    if (!media_held) return;
    for (uint8_t i = 0; i < 3; ++i) {
        if ((media_held & (1 << i)) && media[i].code) unregister_code(media[i].code);
    }
    media_held = 0;
}

void redblade_controls_task(bool fn_active) {
    if (!boot_armed && !turbo_key) return;
    const uint32_t now = timer_read32();
    if (boot_armed && (!fn_active || !(get_mods() & MOD_MASK_SHIFT))) boot_armed = false;
    if (boot_armed && now - boot_started_at >= REDBLADE_BOOT_HOLD_MS) {
        boot_armed = false;
        reset_keyboard();
        return;
    }
    if (turbo_key && now - turbo_edge_at >= turbo_half_period_ms) {
        // At most one edge per housekeeping pass. Never catch up in bursts.
        turbo_edge_at = now;
        if (turbo_down) unregister_code(turbo_key);
        else register_code(turbo_key);
        turbo_down = !turbo_down;
    }
}

bool redblade_controls_process(uint16_t keycode, keyrecord_t *record) {
    if (!record->event.pressed) {
        for (uint8_t i = 0; trigger_held && i < 2; ++i) {
            if ((trigger_held & (1 << i)) && record->event.key.row == trigger[i].row && record->event.key.col == trigger[i].col) {
                trigger_held &= ~(1 << i);
                return false;
            }
        }
        if (boot_armed && record->event.key.row == boot_key.row && record->event.key.col == boot_key.col) boot_armed = false;
        // Match physical release, even if Fn/Shift was released first or the
        // release resolves to another VIA keycode. No consumer key stays held.
        for (uint8_t i = 0; media_held && i < 3; ++i) {
            if ((media_held & (1 << i)) && record->event.key.row == media[i].row && record->event.key.col == media[i].col) {
                if (media[i].code) unregister_code(media[i].code);
                media_held &= ~(1 << i);
                return false;
            }
        }
    }
    if (keycode == KC_ESC && record->event.pressed) stop_automation();
    if (keycode == KC_E || keycode == KC_F || keycode == KC_SPC || keycode == KC_W) {
        const uint8_t bit = physical_bit(keycode);
        if (record->event.pressed) {
            if (keycode == turbo_key) stop_turbo();
            if (keycode == KC_W) stop_w();
            physical_targets |= bit;
        } else physical_targets &= ~bit;
    }
    if (game_mode && (keycode == KC_LGUI || keycode == KC_RGUI ||
        (keycode >= QK_MODS && keycode <= QK_MODS_MAX && ((QK_MODS_GET_MODS(keycode) & 8) ||
        QK_MODS_GET_BASIC_KEYCODE(keycode) == KC_LGUI || QK_MODS_GET_BASIC_KEYCODE(keycode) == KC_RGUI)))) return false;
    if (keycode < RB_GAME || keycode > RB_TURBO_RATE) return true;
    if (!record->event.pressed) return false;
    const bool shift = (get_mods() & MOD_MASK_SHIFT) != 0;
    if (keycode >= RB_PLAY && keycode <= RB_NEXT) {
        const uint8_t i = keycode - RB_PLAY;
        if (media_held & (1 << i)) return false;
        media_held |= 1 << i;
        media[i].row = record->event.key.row; media[i].col = record->event.key.col;
        media[i].code = 0;
        if (shift && i != 0) redblade_rgb_brightness_step(i == 1 ? -1 : 1);
        else {
            media[i].code = i == 0 ? (shift ? KC_MSTP : KC_MPLY) : i == 1 ? KC_MPRV : KC_MNXT;
            register_code(media[i].code);
        }
        return false;
    }
    switch (keycode) {
        case RB_GAME: game_set(shift ? false : !game_mode); break;
        case RB_HOST: redblade_rgb_profile(RB_PROFILE_HOST); break;
        case RB_WHITE: redblade_rgb_profile(RB_PROFILE_WHITE); break;
        case RB_RED: redblade_rgb_profile(RB_PROFILE_RED); break;
        case RB_DARK: redblade_rgb_profile(RB_PROFILE_OFF); break;
        case RB_LIGHT_TOGGLE: redblade_rgb_toggle(); break;
        case RB_BRIGHTER: redblade_rgb_brightness_step(1); break;
        case RB_DIMMER: redblade_rgb_brightness_step(-1); break;
        case RB_WAVE: redblade_rgb_replay(); break;
        case RB_ANIMATION: redblade_rgb_toggle_setting(RB_SETTING_ANIMATION); break;
        case RB_MEDIA_FALLBACK: redblade_rgb_toggle_setting(RB_SETTING_MEDIA); break;
        case RB_IDLE_FALLBACK: redblade_rgb_toggle_setting(RB_SETTING_IDLE_WHITE); break;
        case RB_DEFAULTS: redblade_rgb_defaults(); game_set(false); stop_automation(); turbo_half_period_ms = 10; break;
        case RB_PROFILE_NEXT: redblade_rgb_cycle(1); break;
        case RB_PROFILE_PREV: redblade_rgb_cycle(-1); break;
        case RB_SAVE:
            if (redblade_rgb_save()) { feedback_started_at = timer_read32(); feedback_active = true; }
            break;
        case RB_BOOT:
            // Fn+Esc is the immediate stop/unlock key. Boot needs Shift too.
            stop_automation(); game_set(false);
            if (shift && !boot_armed) { boot_key = record->event.key; boot_started_at = timer_read32(); boot_armed = true; }
            break;
        case RB_TURBO_E: toggle_turbo(KC_E,record->event.key); break;
        case RB_TURBO_F: toggle_turbo(KC_F,record->event.key); break;
        case RB_TURBO_SPACE: toggle_turbo(KC_SPC,record->event.key); break;
        case RB_AUTO_W:
            trigger[1] = record->event.key; trigger_held |= 2;
            if (auto_w) stop_w();
            else if (!(physical_targets & physical_bit(KC_W))) { auto_w = true; register_code(KC_W); }
            break;
        case RB_TURBO_RATE:
            turbo_half_period_ms = shift ? (turbo_half_period_ms == 20 ? 5 : turbo_half_period_ms == 10 ? 20 : 10) : (turbo_half_period_ms == 20 ? 10 : turbo_half_period_ms == 10 ? 5 : 20);
            break;
    }
    return false;
}

bool redblade_controls_indicators_needed(bool fn_active) {
    return fn_active || game_mode || feedback_active || turbo_key || auto_w;
}

bool redblade_controls_indicator(uint8_t index, bool fn_active, hsv_t *color) {
    if (!redblade_rgb_output_active()) return false;
    const uint8_t turbo_led = turbo_key == KC_E ? RB_LED_E : turbo_key == KC_F ? RB_LED_F : turbo_key == KC_SPC ? RB_LED_SPACE : 255;
    if (index == turbo_led) { *color = (hsv_t){0,255,96}; return true; }
    if (auto_w && index == RB_LED_W) { *color = (hsv_t){85,255,96}; return true; }
    if (fn_active && index == RB_LED_R) { *color = (hsv_t){turbo_half_period_ms == 20 ? 85 : turbo_half_period_ms == 10 ? 43 : 0,255,64}; return true; }
    if (index < 16 || index > 19) return false;
    if (index == 16 && feedback_active) {
        if (timer_read32() - feedback_started_at < 600) { *color = (hsv_t){85, 255, 64}; return true; }
        feedback_active = false;
    }
    if (index == 16 && game_mode) { *color = (hsv_t){0, 255, 64}; return true; }
    if (!fn_active) return false;
    const bool shift = (get_mods() & MOD_MASK_SHIFT) != 0;
    *color = (hsv_t){index == 16 ? 85 : index == 17 ? (shift ? 0 : 140) : (shift ? 43 : 170), 230, 64};
    return true;
}
