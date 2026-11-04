#pragma once
#include "quantum.h"
extern uint8_t mode, speed, hue, saturation, value, mods, weak_mods;
extern bool enabled, key_down[256];
extern uint32_t now, eeprom_user;
extern unsigned responses, eeprom_writes, keyboard_reports, bootloader_calls, key_down_calls, key_up_calls;
bool via_command_kb(uint8_t *data, uint8_t length);
