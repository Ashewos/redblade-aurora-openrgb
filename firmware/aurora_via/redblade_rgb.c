// SPDX-License-Identifier: GPL-2.0-or-later
// VialRGB wire compatibility based on vial-kb/vial-qmk/quantum/vialrgb.c
// and CalcProgrammer1/OpenRGB/Controllers/QMKController/QMKVialRGBController.
#include "redblade_rgb.h"
#include "raw_hid.h"
#include <string.h>

_Static_assert(RGB_MATRIX_LED_COUNT == 109, "Red Blade Full Size only");
enum { REDBLADE_REPORT_SIZE = 32 }; // Fixed QMK Raw HID API report size.

// Fixed allocation. Incoming packets only copy HSV; conversion is deferred
// to the cooperative RGB task (at most eight LEDs per task iteration).
static hsv_t incoming[RGB_MATRIX_LED_COUNT];
static uint32_t boot_started_at, last_packet_at, received_packets;
static bool boot_active, have_stream, suspended;
enum render_kind { RENDER_OFF, RENDER_DIRECT, RENDER_BOOT };
static enum render_kind render_kind;
static uint16_t boot_position;
static uint8_t boot_brightness;

_Static_assert(REDBLADE_RGB_TIMEOUT_MS > 0, "A positive RGB watchdog timeout is required");
_Static_assert(REDBLADE_BOOT_ANIMATION_MS > 0, "A positive boot animation duration is required");
_Static_assert(REDBLADE_BOOT_BRIGHTNESS <= RGB_MATRIX_MAXIMUM_BRIGHTNESS, "Boot brightness exceeds board limit");

// Generated inverse of the manufacturer's LED matrix, in logical LED order.
// No matrix search is performed in the USB request handler.
#include "led_positions.inc"

static void forget_stream(void) {
    have_stream = false;
    render_kind = RENDER_OFF;
    memset(incoming, 0, sizeof(incoming));
}

void redblade_rgb_init(void) {
    forget_stream();
    received_packets = 0;
    suspended = false;
    boot_started_at = timer_read32();
    // Respect an explicitly disabled light or a saved standalone effect.
    boot_active = rgb_matrix_is_enabled() && rgb_matrix_get_mode() == RGB_MATRIX_CUSTOM_REDBLADE_DIRECT;
}

void redblade_rgb_suspend(void) {
    if (suspended) return;
    suspended = true;
    boot_active = false;
    forget_stream();
    // RGB_MATRIX_SLEEP in QMK flushes black without stopping the shared
    // matrix/PWM timer. Stopping that timer would also stop key scanning.
}

void redblade_rgb_wake(void) {
    suspended = false;
    boot_active = false;
    forget_stream(); // Never replay a pre-sleep frame or the boot animation.
}

void redblade_rgb_begin_frame(void) {
    // Once per RGB frame, outside the keyboard scan and USB packet handler.
    const uint32_t now = timer_read32();
    render_kind = RENDER_OFF;
    if (suspended) return;
    if (boot_active) {
        const uint32_t elapsed = now - boot_started_at;
        if (elapsed < REDBLADE_BOOT_ANIMATION_MS) {
            render_kind = RENDER_BOOT;
            // A 32-unit soft-edged sweep across QMK's 0..224 X coordinates.
            boot_position = elapsed * 288UL / REDBLADE_BOOT_ANIMATION_MS;
            boot_brightness = rgb_matrix_get_val();
            if (boot_brightness > REDBLADE_BOOT_BRIGHTNESS) boot_brightness = REDBLADE_BOOT_BRIGHTNESS;
            return;
        }
        boot_active = false;
    }
    if (have_stream && now - last_packet_at >= REDBLADE_RGB_TIMEOUT_MS) forget_stream();
    if (have_stream) render_kind = RENDER_DIRECT;
}

hsv_t redblade_rgb_color(uint8_t index) {
    if (render_kind == RENDER_DIRECT) return incoming[index];
    hsv_t color = {0, 0, 0};
    if (render_kind == RENDER_BOOT) {
        int16_t distance = (int16_t)g_led_config.point[index].x + 32 - (int16_t)boot_position;
        if (distance < 0) distance = -distance;
        if (distance < 32) {
            color.h = 128; // restrained cyan sweep, integer arithmetic only
            color.s = 220;
            color.v = ((32 - distance) * boot_brightness) >> 5;
        }
    }
    return color;
}

static uint16_t read16(const uint8_t *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static void write16(uint8_t *p, uint16_t n) {
    p[0] = n;
    p[1] = n >> 8;
}

static void write32(uint8_t *p, uint32_t n) {
    write16(p, n);
    write16(p + 2, n >> 16);
}

static uint16_t current_mode(void) {
    if (!rgb_matrix_is_enabled()) return 0;
    return rgb_matrix_get_mode() == RGB_MATRIX_CUSTOM_REDBLADE_DIRECT ? 1 : 2;
}

// This intercepts only the VialRGB lighting extension and its discovery
// request. All standard VIA commands continue through upstream QMK.
bool via_command_kb(uint8_t *data, uint8_t length) {
    if (length != REDBLADE_REPORT_SIZE) return false;

    if (data[0] == 0xFE && data[1] == 0x00) {
        // Vial discovery envelope required by OpenRGB, RGB capability only.
        // Full Vial remapping / unlock protocol is deliberately not advertised
        // as a supported UI: this firmware retains the standard VIA UI.
        memset(data, 0, length);
        data[0] = 4; // little-endian discovery protocol version
        memcpy(data + 4, "RDBLFS01", 8);
        data[12] = 1; // VialRGB capability
        raw_hid_send(data, length);
        return true;
    }

    if (data[0] == 0xFE && data[1] == 0x80) {
        // Private read-only diagnostics, separate from VialRGB commands.
        const uint32_t now = timer_read32();
        const uint32_t age = have_stream ? now - last_packet_at : UINT32_MAX;
        memset(data + 2, 0, length - 2);
        data[2] = 1; // diagnostics format
        data[3] = 0; data[4] = 2; data[5] = 0; // firmware 0.2.0
        data[6] = (have_stream && age < REDBLADE_RGB_TIMEOUT_MS && !suspended ? 1 : 0) |
                  (boot_active && now - boot_started_at < REDBLADE_BOOT_ANIMATION_MS ? 2 : 0) |
                  (suspended ? 4 : 0) | (rgb_matrix_is_enabled() ? 8 : 0);
        data[7] = current_mode();
        write32(data + 8, age);
        write32(data + 12, REDBLADE_RGB_TIMEOUT_MS);
        write32(data + 16, received_packets);
        write32(data + 20, REDBLADE_BOOT_ANIMATION_MS);
        memcpy(data + 24, "RDBLFS01", 8);
        raw_hid_send(data, length);
        return true;
    }

    const uint8_t command = data[0];
    const uint8_t subcommand = data[1];
    if ((command != 0x07 && command != 0x08) || subcommand < 0x40 || subcommand > 0x44) return false;
    uint8_t *args = data + 2;

    if (command == 0x08) {
        const uint16_t requested = read16(args);
        memset(args, 0, length - 2);
        switch (subcommand) {
            case 0x40: // RGB protocol and maximum brightness
                write16(args, 1);
                args[2] = RGB_MATRIX_MAXIMUM_BRIGHTNESS;
                break;
            case 0x41:
                write16(args, current_mode());
                args[2] = rgb_matrix_get_speed();
                args[3] = rgb_matrix_get_hue();
                args[4] = rgb_matrix_get_sat();
                args[5] = rgb_matrix_get_val();
                break;
            case 0x42: { // supported modes, strictly greater than request
                memset(args, 0xFF, length - 2);
                uint8_t offset = 0;
                for (uint16_t mode = 1; mode <= 2; ++mode) {
                    if (mode > requested) {
                        write16(args + offset, mode);
                        offset += 2;
                    }
                }
                break;
            }
            case 0x43:
                write16(args, RGB_MATRIX_LED_COUNT);
                break;
            case 0x44:
                if (requested < RGB_MATRIX_LED_COUNT) {
                    args[0] = g_led_config.point[requested].x;
                    args[1] = g_led_config.point[requested].y;
                    args[2] = g_led_config.flags[requested];
                    args[3] = led_positions[requested][0];
                    args[4] = led_positions[requested][1];
                } else {
                    data[0] = 0xFF;
                }
                break;
        }
    } else if (subcommand == 0x41) {
        const uint16_t mode = read16(args);
        if (mode > 2) {
            data[0] = 0xFF;
        } else {
            boot_active = false; // An explicit host mode takes priority.
            if (mode == 0) {
                rgb_matrix_disable_noeeprom();
            } else {
                rgb_matrix_enable_noeeprom();
                rgb_matrix_mode_noeeprom(mode == 1 ? RGB_MATRIX_CUSTOM_REDBLADE_DIRECT : RGB_MATRIX_SOLID_COLOR);
            }
            rgb_matrix_set_speed_noeeprom(args[2]);
            rgb_matrix_sethsv_noeeprom(args[3], args[4], args[5]);
        }
    } else if (subcommand == 0x42) {
        const uint16_t first = read16(args);
        const uint8_t count = args[2];
        // Validate the complete request before touching either color buffer.
        if (count == 0 || count > 9 || first >= RGB_MATRIX_LED_COUNT || count > RGB_MATRIX_LED_COUNT - first) {
            data[0] = 0xFF;
        } else {
            for (uint8_t i = 0; i < count; ++i) {
                incoming[first + i].h = args[3 + i * 3];
                incoming[first + i].s = args[4 + i * 3];
                uint8_t value = args[5 + i * 3];
#if RGB_MATRIX_MAXIMUM_BRIGHTNESS < 255
                if (value > RGB_MATRIX_MAXIMUM_BRIGHTNESS) value = RGB_MATRIX_MAXIMUM_BRIGHTNESS;
#endif
                incoming[first + i].v = value;
            }
            last_packet_at = timer_read32();
            have_stream = true;
            boot_active = false; // Latest host colors take over next frame.
            ++received_packets;
        }
    } else {
        data[0] = 0xFF;
    }
    raw_hid_send(data, length);
    return true;
}
