// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "quantum.h"

// Stable VIA custom keycodes. Keep in sync with fn-shortcuts.json.
enum redblade_keycodes {
    RB_GAME = SAFE_RANGE, RB_PLAY, RB_PREV, RB_NEXT,
    RB_HOST, RB_WHITE, RB_RED, RB_DARK, RB_LIGHT_TOGGLE,
    RB_BRIGHTER, RB_DIMMER, RB_WAVE, RB_ANIMATION,
    RB_MEDIA_FALLBACK, RB_IDLE_FALLBACK, RB_DEFAULTS,
    RB_PROFILE_NEXT, RB_PROFILE_PREV, RB_SAVE, RB_BOOT,
    RB_TURBO_E, RB_TURBO_F, RB_TURBO_SPACE, RB_AUTO_W, RB_TURBO_RATE
};
enum redblade_game_leds { RB_LED_W = 43, RB_LED_E = 44, RB_LED_R = 45, RB_LED_F = 65, RB_LED_SPACE = 99 };

void redblade_controls_init(void);
bool redblade_controls_process(uint16_t keycode, keyrecord_t *record);
void redblade_controls_task(bool fn_active);
void redblade_controls_suspend(void);
bool redblade_game_mode(void);
uint8_t redblade_turbo_key(void);
uint8_t redblade_turbo_rate(void);
bool redblade_auto_w(void);
bool redblade_controls_indicators_needed(bool fn_active);
bool redblade_controls_indicator(uint8_t index, bool fn_active, hsv_t *color);
