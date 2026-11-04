# SPDX-License-Identifier: GPL-2.0-or-later
VIA_ENABLE = yes
CONSOLE_ENABLE = no
COMMAND_ENABLE = no
LTO_ENABLE = yes
# Minimal button-only HID output; no cursor/acceleration engine or extra task.
MOUSE_ENABLE = yes
MOUSE_SHARED_EP = yes
SRC += redblade_rgb.c redblade_controls.c redblade_recorder.c

