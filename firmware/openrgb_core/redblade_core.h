// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "quantum.h"
#define RB_CORE_BOOT 0x7E53
void redblade_core_init(bool configured);
void redblade_core_usb(bool configured);
bool redblade_core_key(uint16_t keycode, keyrecord_t *record);
void redblade_core_task(bool fn_active);
void redblade_core_begin_frame(void);
hsv_t redblade_core_color(uint8_t index);
