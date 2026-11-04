// SPDX-License-Identifier: GPL-2.0-or-later
#include "redblade_controls.h"
#include "redblade_rgb.h"
#include "dynamic_keymap.h"
#include "report.h"

_Static_assert(RB_GAME == 0x7E40 && RB_BOOT == 0x7E53, "VIA custom keycode ABI changed");
_Static_assert(RB_TURBO_RATE == 0x7E58 && RB_TURBO_FASTER == 0x7E5B, "Game helper keycode ABI changed");
static bool game_mode, boot_armed, feedback_active;
static bool guide_active;
static uint32_t guide_started_at;
static uint32_t boot_started_at, feedback_started_at;
static keypos_t boot_key;
static uint8_t media_held;
static struct { uint8_t code, row, col; } media[3];
static uint8_t turbo_key, turbo_led = 255, turbo_rate = 50, turbo_interval = 10;
static uint8_t turbo_base_ms = 10, turbo_remainder, turbo_fraction, trigger_held;
static bool turbo_down, auto_w, selecting;
static uint8_t pending_key, consumed_releases[(MATRIX_ROWS * MATRIX_COLS + 7) / 8];
static uint32_t turbo_edge_at, selection_started_at;
static keypos_t trigger[3];

uint8_t redblade_turbo_key(void) { return turbo_key; }
uint8_t redblade_turbo_rate(void) { return turbo_rate; }
uint8_t redblade_turbo_led(void) { return turbo_led; }
bool redblade_turbo_selecting(void) { return selecting || pending_key; }
bool redblade_auto_w(void) { return auto_w; }
bool redblade_guide_active(void) { return guide_active; }

static void stop_turbo(void) {
    if (turbo_down) unregister_code(turbo_key);
    turbo_key = 0; turbo_led = 255; turbo_down = false;
}
static void stop_w(void) { if (auto_w) unregister_code(KC_W); auto_w = false; }
static void stop_automation(void) { stop_turbo(); stop_w(); selecting = false; pending_key = 0; guide_active = false; }

static void set_rate(uint8_t rate) {
    turbo_rate = rate;
    // Divide only on a human control action; distribute fractional ms across
    // later edges so a requested 60/s does not become 62.5/s.
    turbo_base_ms = 500 / rate; turbo_remainder = 500 % rate;
    turbo_interval = turbo_base_ms; turbo_fraction = 0; turbo_edge_at = timer_read32();
}
static uint8_t led_at(keypos_t pos) {
    return pos.row < MATRIX_ROWS && pos.col < MATRIX_COLS ? g_led_config.matrix_co[pos.row][pos.col] : 255;
}
static void start_turbo(uint8_t key, uint8_t led) {
    if (key == KC_W) stop_w(); // Exactly one owner for a generated usage.
    if (is_key_pressed(key)) return; // Never steal a physically held target.
    stop_turbo();
    turbo_key = key; turbo_led = led; turbo_down = true;
    turbo_fraction = 0; turbo_interval = turbo_base_ms; turbo_edge_at = timer_read32();
    register_code(key);
}
static void toggle_turbo(uint8_t key, keypos_t pos) {
    trigger[0] = pos; trigger_held |= 1; selecting = false; pending_key = 0;
    if (turbo_key == key) { stop_turbo(); return; }
    start_turbo(key, led_at(pos));
}
bool redblade_game_mode(void) { return game_mode; }
static void game_set(bool enabled) {
    game_mode = enabled;
    if (enabled) { del_mods(MOD_MASK_GUI); del_weak_mods(MOD_MASK_GUI); send_keyboard_report(); }
}
void redblade_controls_init(void) {
    stop_automation(); game_mode = boot_armed = feedback_active = false;
    media_held = trigger_held = 0;
    for (uint8_t i = 0; i < sizeof(consumed_releases); ++i) consumed_releases[i] = 0;
    set_rate(50);
}
void redblade_controls_suspend(void) {
    boot_armed = feedback_active = game_mode = false; stop_automation(); trigger_held = 0;
    for (uint8_t i = 0; i < sizeof(consumed_releases); ++i) consumed_releases[i] = 0;
    for (uint8_t i = 0; media_held && i < 3; ++i) {
        if ((media_held & (1 << i)) && media[i].code) unregister_code(media[i].code);
    }
    media_held = 0;
}
void redblade_controls_task(bool fn_active) {
    if (!boot_armed && !turbo_key && !selecting && !pending_key && !guide_active) return;
    const uint32_t now = timer_read32();
    if ((selecting || pending_key) && now - selection_started_at >= REDBLADE_TURBO_SELECT_TIMEOUT_MS) {
        selecting = false; pending_key = 0;
    }
    if (guide_active && now - guide_started_at >= REDBLADE_GUIDE_TIMEOUT_MS) guide_active = false;
    if (boot_armed && !fn_active) boot_armed = false;
    if (boot_armed && now - boot_started_at >= REDBLADE_BOOT_HOLD_MS) {
        boot_armed = false; reset_keyboard(); return;
    }
    if (turbo_key && now - turbo_edge_at >= turbo_interval) {
        // At most one edge per pass. Drop missed deadlines, never catch up.
        turbo_edge_at = now;
        if (turbo_down) unregister_code(turbo_key); else register_code(turbo_key);
        turbo_down = !turbo_down; turbo_interval = turbo_base_ms;
        turbo_fraction += turbo_remainder;
        if (turbo_fraction >= turbo_rate) { ++turbo_interval; turbo_fraction -= turbo_rate; }
    }
}
bool redblade_controls_process(uint16_t keycode, keyrecord_t *record) {
    const keypos_t pos = record->event.key;
    const bool valid_pos = pos.row < MATRIX_ROWS && pos.col < MATRIX_COLS;
    const uint8_t position = valid_pos ? pos.row * MATRIX_COLS + pos.col : 0;
    if (!record->event.pressed) {
        if (boot_armed && pos.row == boot_key.row && pos.col == boot_key.col) boot_armed = false;
        if (valid_pos && (consumed_releases[position / 8] & (1 << (position % 8)))) {
            consumed_releases[position / 8] &= ~(1 << (position % 8));
            if (pending_key && pos.row == trigger[0].row && pos.col == trigger[0].col) {
                const uint8_t key = pending_key; pending_key = 0; start_turbo(key, led_at(pos));
            }
            return false;
        }
        for (uint8_t i = 0; trigger_held && i < 3; ++i) {
            if ((trigger_held & (1 << i)) && pos.row == trigger[i].row && pos.col == trigger[i].col) {
                trigger_held &= ~(1 << i); return false;
            }
        }
        if (boot_armed && pos.row == boot_key.row && pos.col == boot_key.col) boot_armed = false;
        for (uint8_t i = 0; media_held && i < 3; ++i) {
            if ((media_held & (1 << i)) && pos.row == media[i].row && pos.col == media[i].col) {
                if (media[i].code) unregister_code(media[i].code);
                media_held &= ~(1 << i); return false;
            }
        }
    }
    if (keycode == KC_ESC && record->event.pressed) stop_automation();
    if (selecting && record->event.pressed && keycode != RB_BOOT && keycode != RB_TURBO_SELECT && keycode != KC_ESC) {
        if (!valid_pos) return false;
        // Only resolve VIA's base layer while choosing; with Fn still held,
        // F1 selects F1 rather than changing the lighting profile.
        const uint16_t base = dynamic_keymap_get_keycode(0, pos.row, pos.col);
        if (base >= 4 && base <= 0xA4 && base != KC_ESC && base != 0x66 && !is_key_pressed(base)) {
            selecting = false; pending_key = base; selection_started_at = timer_read32(); trigger[0] = pos;
            consumed_releases[position / 8] |= 1 << (position % 8);
            return false; // Start on release; selection itself never types.
        }
        if (!((base >= 0xE0 && base <= 0xE7) || (base >= QK_MOMENTARY && base <= QK_MOMENTARY_MAX))) {
            consumed_releases[position / 8] |= 1 << (position % 8);
            return false; // Unsupported system/consumer/macro actions do not execute.
        }
    }
    if (record->event.pressed && keycode == turbo_key) stop_turbo();
    if (record->event.pressed && keycode == KC_W) stop_w();
    if (game_mode && (keycode == KC_LGUI || keycode == KC_RGUI ||
        (keycode >= QK_MODS && keycode <= QK_MODS_MAX && ((QK_MODS_GET_MODS(keycode) & 8) ||
        QK_MODS_GET_BASIC_KEYCODE(keycode) == KC_LGUI || QK_MODS_GET_BASIC_KEYCODE(keycode) == KC_RGUI)))) return false;
    if (keycode < RB_GAME || keycode > RB_GUIDE) return true;
    if (!record->event.pressed) return false;
    const bool shift = (get_mods() & MOD_MASK_SHIFT) != 0;
    if (keycode >= RB_PLAY && keycode <= RB_NEXT) {
        const uint8_t i = keycode - RB_PLAY;
        if (media_held & (1 << i)) return false;
        media_held |= 1 << i; media[i].row = pos.row; media[i].col = pos.col; media[i].code = 0;
        if (shift && i != 0) redblade_rgb_brightness_step(i == 1 ? -1 : 1);
        else {
            media[i].code = i == 0 ? (shift ? KC_MSTP : KC_MPLY) : i == 1 ? KC_MPRV : KC_MNXT;
            register_code(media[i].code);
        }
        return false;
    }
    // Every non-trigger Fn action owns its release too. In particular a live
    // rate adjustment must not release a repeated numpad key if Fn goes up first.
    if (valid_pos && keycode != RB_TURBO_E && keycode != RB_TURBO_F && keycode != RB_TURBO_SPACE && keycode != RB_AUTO_W && keycode != RB_TURBO_SELECT) {
        consumed_releases[position / 8] |= 1 << (position % 8);
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
        case RB_DEFAULTS: redblade_rgb_defaults(); game_set(false); stop_automation(); set_rate(50); break;
        case RB_PROFILE_NEXT: redblade_rgb_cycle(1); break;
        case RB_PROFILE_PREV: redblade_rgb_cycle(-1); break;
        case RB_SAVE:
            if (redblade_rgb_save()) { feedback_started_at = timer_read32(); feedback_active = true; }
            break;
        case RB_BOOT:
            stop_automation(); game_set(false);
            if (!boot_armed) { boot_key = pos; boot_started_at = timer_read32(); boot_armed = true; }
            break;
        case RB_GUIDE:
            guide_active = shift ? false : !guide_active;
            guide_started_at = timer_read32();
            break;
        case RB_TURBO_E: toggle_turbo(KC_E,pos); break;
        case RB_TURBO_F: toggle_turbo(KC_F,pos); break;
        case RB_TURBO_SPACE: toggle_turbo(KC_SPC,pos); break;
        case RB_TURBO_SELECT:
            trigger[2] = pos; trigger_held |= 4;
            if (turbo_key || selecting || pending_key) { stop_turbo(); selecting = false; pending_key = 0; }
            else { selecting = true; selection_started_at = timer_read32(); }
            break;
        case RB_AUTO_W:
            trigger[1] = pos; trigger_held |= 2;
            if (auto_w) stop_w();
            else { if (turbo_key == KC_W) stop_turbo(); if (!is_key_pressed(KC_W)) { auto_w = true; register_code(KC_W); } }
            break;
        case RB_TURBO_RATE:
            set_rate(shift ? (turbo_rate > 50 ? 50 : turbo_rate > 25 ? 25 : 100) : (turbo_rate < 25 ? 25 : turbo_rate < 50 ? 50 : turbo_rate < 100 ? 100 : 25));
            break;
        case RB_TURBO_SLOWER: set_rate(turbo_rate > 5 ? turbo_rate - 5 : 5); break;
        case RB_TURBO_FASTER: set_rate(turbo_rate < 100 ? turbo_rate + 5 : 100); break;
    }
    return false;
}
bool redblade_controls_indicators_needed(bool fn_active) { (void)fn_active; return true; }
bool redblade_controls_indicator(uint8_t index, bool fn_active, hsv_t *color) {
    if (!redblade_rgb_output_active()) return false;
    if (index == RB_LED_FN) {
        if (selecting || pending_key) *color = (hsv_t){32,255,(timer_read32() / 250) & 1 ? 128 : 40};
        else if (turbo_key) *color = (hsv_t){0,255,128};
        else if (auto_w) *color = (hsv_t){85,255,96};
        else if (fn_active || guide_active) *color = (hsv_t){128,255,96};
        else if (game_mode) *color = (hsv_t){200,255,80};
        else *color = (hsv_t){170,255,24};
        return true;
    }
    if (game_mode && (index == RB_LED_LGUI || index == RB_LED_RGUI)) { *color = (hsv_t){0,255,128}; return true; }
    if (index == turbo_led && turbo_key) { *color = (hsv_t){0,255,128}; return true; }
    if (auto_w && index == RB_LED_W) { *color = (hsv_t){85,255,96}; return true; }
    const bool hints = fn_active || guide_active;
    if (hints && index == RB_LED_T) { *color = (hsv_t){32,255,96}; return true; }
    if (hints && (index == RB_LED_R || index == RB_LED_MINUS || index == RB_LED_PLUS)) {
        *color = (hsv_t){turbo_rate <= 25 ? 85 : turbo_rate <= 50 ? 43 : 0,255,80}; return true;
    }
    if (redblade_rgb_fallback_active()) {
        const led_t locks = host_keyboard_led_state();
        if (index == RB_LED_CAPS || (REDBLADE_CAPS_SHOW_SHIFT && (index == RB_LED_LSHIFT || index == RB_LED_RSHIFT))) {
            *color = (hsv_t){85,locks.caps_lock ? 255 : 0,locks.caps_lock ? 128 : REDBLADE_FALLBACK_BRIGHTNESS}; return true;
        }
        if (index == RB_LED_SCROLL) { *color = (hsv_t){32,locks.scroll_lock ? 255 : 0,locks.scroll_lock ? 128 : REDBLADE_FALLBACK_BRIGHTNESS}; return true; }
        if ((index >= 37 && index <= 40) || (index >= 57 && index <= 60) || (index >= 75 && index <= 77) || (index >= 92 && index <= 95) || index >= 107) {
            *color = (hsv_t){85,index == RB_LED_NUM && locks.num_lock ? 255 : 0,locks.num_lock ? (index == RB_LED_NUM ? 128 : REDBLADE_FALLBACK_BRIGHTNESS) : REDBLADE_NUMPAD_DIM_BRIGHTNESS}; return true;
        }
    }
    if (hints) {
        if (index == RB_LED_LGUI || index == RB_LED_RGUI) { *color = (hsv_t){85,255,80}; return true; }
        if (index == RB_LED_E || index == RB_LED_F || index == RB_LED_SPACE || index == 0 || index == 74) { *color = (hsv_t){0,255,96}; return true; }
        if (index == RB_LED_W) { *color = (hsv_t){85,255,80}; return true; }
        if (index == 1) { *color = (hsv_t){170,255,80}; return true; }
        if (index == 2 || index == 33 || index == 36 || index == 56) { *color = (hsv_t){0,0,80}; return true; }
        if (index == 3) { *color = (hsv_t){0,255,80}; return true; }
        if (index == 4) { *color = (hsv_t){0,0,8}; return true; }
        if (index >= 5 && index <= 7) { *color = (hsv_t){85,(redblade_rgb_settings_get() & (1 << (index-5))) ? 255 : 0,64}; return true; }
        if (index == 8 || index == 9 || (index >= 104 && index <= 106) || index == 91) { *color = (hsv_t){index == 9 ? 85 : 200,255,80}; return true; }
    }
    if (index < 16 || index > 19) {
        if (guide_active) { *color = (hsv_t){0,0,8}; return true; }
        return false;
    }
    if (index == 16 && feedback_active) {
        if (timer_read32() - feedback_started_at < 600) { *color = (hsv_t){85,255,96}; return true; }
        feedback_active = false;
    }
    if (!hints) return false;
    const bool shift = (get_mods() & MOD_MASK_SHIFT) != 0;
    *color = (hsv_t){index == 16 ? 128 : index == 17 ? (shift ? 0 : 140) : (shift ? 43 : 170),230,80};
    return true;
}

void redblade_controls_render(uint8_t led_min, uint8_t led_max, bool fn_active) {
    if (!redblade_rgb_output_active()) return;
    for (uint8_t i = led_min; i < led_max; ++i) {
        hsv_t color;
        if (!redblade_controls_indicator(i, fn_active, &color)) continue;
        if (color.v > rgb_matrix_get_val()) color.v = rgb_matrix_get_val();
        const rgb_t rgb = redblade_rgb_convert(redblade_rgb_apply_brightness(color));
        rgb_matrix_set_color(i, rgb.r, rgb.g, rgb.b);
    }
}
