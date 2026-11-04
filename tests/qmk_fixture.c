#include "qmk_fixture.h"
#include <assert.h>
led_config_t g_led_config = {.point = {
#include "led_points.inc"
}};
uint8_t mode = RGB_MATRIX_CUSTOM_REDBLADE_DIRECT, speed, hue, saturation, value = 255, mods, weak_mods;
bool enabled = true, key_down[256];
uint32_t now, eeprom_user = UINT32_MAX;
unsigned responses, eeprom_writes, keyboard_reports, bootloader_calls, key_down_calls, key_up_calls;
uint32_t timer_read32(void) { return now; }
uint32_t eeconfig_read_user(void) { return eeprom_user; }
void eeconfig_update_user(uint32_t v) { eeprom_user = v; ++eeprom_writes; }
uint8_t get_mods(void) { return mods; }
void del_mods(uint8_t m) { mods &= ~m; }
void del_weak_mods(uint8_t m) { weak_mods &= ~m; }
void send_keyboard_report(void) { ++keyboard_reports; }
void register_code(uint8_t code) { key_down[code] = true; ++key_down_calls; }
void unregister_code(uint8_t code) { key_down[code] = false; ++key_up_calls; }
void reset_keyboard(void) { ++bootloader_calls; }
bool rgb_matrix_is_enabled(void) { return enabled; }
uint8_t rgb_matrix_get_mode(void) { return mode; }
uint8_t rgb_matrix_get_speed(void) { return speed; }
uint8_t rgb_matrix_get_hue(void) { return hue; }
uint8_t rgb_matrix_get_sat(void) { return saturation; }
uint8_t rgb_matrix_get_val(void) { return value; }
void rgb_matrix_enable_noeeprom(void) { enabled = true; }
void rgb_matrix_disable_noeeprom(void) { enabled = false; }
void rgb_matrix_mode_noeeprom(uint8_t m) { mode = m; }
void rgb_matrix_set_speed_noeeprom(uint8_t s) { speed = s; }
void rgb_matrix_sethsv_noeeprom(uint8_t h, uint8_t s, uint8_t v) { hue=h; saturation=s; value=v; }
void raw_hid_send(uint8_t *data, uint8_t length) { (void)data; assert(length == 32); ++responses; }
