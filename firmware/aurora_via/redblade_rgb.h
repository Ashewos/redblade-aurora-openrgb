// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "quantum.h"

void redblade_rgb_init(void);
void redblade_rgb_suspend(void);
void redblade_rgb_wake(void);
void redblade_rgb_begin_frame(void);
hsv_t redblade_rgb_color(uint8_t index);

