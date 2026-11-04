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
static bool boot_active, have_stream, suspended, idle_fallback;
enum render_kind { RENDER_OFF, RENDER_WAITING, RENDER_DIRECT, RENDER_WHITE, RENDER_BOOT };
static uint8_t render_kind, render_target;
static uint16_t boot_position;
static uint8_t boot_brightness, fallback_brightness, media_brightness, boot_blend;

_Static_assert(REDBLADE_RGB_TIMEOUT_MS > 0, "A positive RGB watchdog timeout is required");
_Static_assert(REDBLADE_BOOT_ANIMATION_MS > 0, "A positive boot animation duration is required");
_Static_assert(REDBLADE_BOOT_BRIGHTNESS <= RGB_MATRIX_MAXIMUM_BRIGHTNESS, "Boot brightness exceeds board limit");
_Static_assert(REDBLADE_BOOT_ANIMATION_MS >= 10, "Boot animation must allow an entrance and handover phase");
_Static_assert(REDBLADE_FALLBACK_BRIGHTNESS <= RGB_MATRIX_MAXIMUM_BRIGHTNESS, "Fallback brightness exceeds board limit");
_Static_assert(REDBLADE_MEDIA_BRIGHTNESS <= RGB_MATRIX_MAXIMUM_BRIGHTNESS, "Media brightness exceeds board limit");

// Generated inverse of the manufacturer's LED matrix, in logical LED order.
// No matrix search is performed in the USB request handler.
#include "led_positions.inc"

static void forget_stream(void) {
    have_stream = false;
    render_kind = RENDER_OFF;
    memset(incoming, 0, sizeof(incoming));
}

void redblade_rgb_init(bool usb_active) {
    forget_stream();
    received_packets = 0;
    idle_fallback = false;
    suspended = !usb_active;
    boot_started_at = last_packet_at = timer_read32();
    // Respect an explicitly disabled light or a saved standalone effect.
    boot_active = usb_active && rgb_matrix_is_enabled() && rgb_matrix_get_mode() == RGB_MATRIX_CUSTOM_REDBLADE_DIRECT;
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
    if (!suspended) return; // USB LED/protocol notifications are not new boots.
    suspended = false;
    forget_stream(); // Never replay a pre-sleep frame.
    idle_fallback = false;
    boot_started_at = last_packet_at = timer_read32();
    boot_active = rgb_matrix_is_enabled() && rgb_matrix_get_mode() == RGB_MATRIX_CUSTOM_REDBLADE_DIRECT;
}

void redblade_rgb_begin_frame(void) {
    // Once per RGB frame, outside the keyboard scan and USB packet handler.
    const uint32_t now = timer_read32();
    render_kind = RENDER_OFF;
    if (suspended || !rgb_matrix_is_enabled() || rgb_matrix_get_mode() != RGB_MATRIX_CUSTOM_REDBLADE_DIRECT) return;
    const uint8_t brightness = rgb_matrix_get_val();
    boot_brightness = brightness < REDBLADE_BOOT_BRIGHTNESS ? brightness : REDBLADE_BOOT_BRIGHTNESS;
    fallback_brightness = brightness < REDBLADE_FALLBACK_BRIGHTNESS ? brightness : REDBLADE_FALLBACK_BRIGHTNESS;
    media_brightness = brightness < REDBLADE_MEDIA_BRIGHTNESS ? brightness : REDBLADE_MEDIA_BRIGHTNESS;
    if (!idle_fallback && now - last_packet_at >= REDBLADE_RGB_TIMEOUT_MS) {
        forget_stream();
        idle_fallback = true;
    }
    render_target = have_stream ? RENDER_DIRECT : idle_fallback ? RENDER_WHITE : RENDER_WAITING;
    if (boot_active) {
        const uint32_t elapsed = now - boot_started_at;
        if (elapsed < REDBLADE_BOOT_ANIMATION_MS) {
            render_kind = RENDER_BOOT;
            const uint32_t sweep_ms = REDBLADE_BOOT_ANIMATION_MS * 4 / 5;
            boot_position = elapsed * 384UL / sweep_ms;
            // Soft entrance, then HSV fade to the latest host colors in the final 20%.
            if (elapsed < REDBLADE_BOOT_ANIMATION_MS / 10) boot_brightness = boot_brightness * elapsed / (REDBLADE_BOOT_ANIMATION_MS / 10);
            boot_blend = elapsed < sweep_ms ? 0 : (elapsed - sweep_ms) * 255UL / (REDBLADE_BOOT_ANIMATION_MS - sweep_ms);
            return;
        }
        boot_active = false;
    }
    render_kind = render_target;
}

static hsv_t target_color(uint8_t index, uint8_t kind) {
    hsv_t color = {0, 0, 0};
    if (kind == RENDER_OFF) return color;
    if (kind == RENDER_WHITE) { color.v = fallback_brightness; return color; }
    if (kind == RENDER_DIRECT) color = incoming[index];
    // Manufacturer LED indices 16..19: Calc, Mute, Volume-, Volume+.
    // A black host color is the fallback sentinel for these four keys only.
    if (color.v == 0 && index >= 16 && index <= 19) {
        color.h = color.s = 0;
        color.v = media_brightness;
    }
    return color;
}

hsv_t redblade_rgb_color(uint8_t index) {
    if (render_kind != RENDER_BOOT) return target_color(index, render_kind);
    hsv_t color = {0, 0, 0};
    if (render_kind == RENDER_BOOT) {
        // Curved wavefront: each row offsets the crest; trailing edge fades
        // over 112 units while the front edge is crisp (32 units).
        const int16_t crest = (int16_t)boot_position - 64 - ((int16_t)g_led_config.point[index].y - 32) * ((int16_t)g_led_config.point[index].y - 32) / 96;
        const int16_t behind = crest - g_led_config.point[index].x;
        color.h = 166; color.s = 245; // deep blue bed
        color.v = boot_brightness / 12;
        if (behind >= -32 && behind < 112) {
            const uint8_t strength = behind < 0 ? (behind + 32) * 4 : (112 - behind) * 128 / 112;
            color.h = 166 - strength * 24 / 128; // blue -> icy blue crest
            color.s = 245 - strength * 85 / 128;
            color.v = color.v + strength * (boot_brightness - color.v) / 128;
        }
        if (boot_blend) {
            const hsv_t target = target_color(index, render_target);
            int16_t hue_delta = (int16_t)target.h - color.h;
            if (hue_delta > 127) hue_delta -= 256;
            if (hue_delta < -128) hue_delta += 256;
            color.h += hue_delta * boot_blend / 255;
            color.s += ((int16_t)target.s - color.s) * boot_blend / 255;
            color.v += ((int16_t)target.v - color.v) * boot_blend / 255;
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
        data[3] = 0; data[4] = 3; data[5] = 0; // firmware 0.3.0
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
            if (mode != 1) boot_active = false; // Explicit Off/standalone takes priority.
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
            idle_fallback = false;
            // Keep accepting frames during the nonblocking boot wave, so its
            // final fade hands over to fresh colors rather than an old image.
            ++received_packets;
        }
    } else {
        data[0] = 0xFF;
    }
    raw_hid_send(data, length);
    return true;
}
