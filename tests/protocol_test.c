// Compile the actual firmware protocol handler against a small QMK fixture.
#include "redblade_rgb.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

bool via_command_kb(uint8_t *, uint8_t);
led_config_t g_led_config;
static uint8_t mode = RGB_MATRIX_SOLID_COLOR, speed, hue, saturation, value;
static bool enabled = true;
static unsigned responses;
static uint32_t now;
uint32_t timer_read32(void) { return now; }
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
void raw_hid_send(uint8_t *data, uint8_t length) { (void)data; assert(length==32); ++responses; }

static void set_led(uint8_t index, uint8_t val) {
    uint8_t packet[32] = {7, 0x42, index, 0, 1, 17, 200, val};
    assert(via_command_kb(packet, 32) && packet[0] == 7);
}

static void assert_black(void) {
    redblade_rgb_begin_frame();
    for (unsigned i = 0; i < 109; ++i) assert(redblade_rgb_color(i).v == 0);
}

static void test_lifecycle(void) {
    mode = RGB_MATRIX_CUSTOM_REDBLADE_DIRECT; enabled = true; value = 255;
    now = 0; redblade_rgb_init();
    for (unsigned i = 0; i < 109; ++i) g_led_config.point[i].x = i * 224 / 108;
    assert_black(); // Sweep starts outside the keyboard.
    now = REDBLADE_BOOT_ANIMATION_MS / 2;
    redblade_rgb_begin_frame();
    unsigned lit = 0;
    for (unsigned i = 0; i < 109; ++i) {
        hsv_t color = redblade_rgb_color(i);
        assert(color.v <= REDBLADE_BOOT_BRIGHTNESS);
        if (color.v) { assert(color.h == 128 && color.s == 220); ++lit; }
    }
    assert(lit > 0 && lit < 109);
    // Valid host data preempts the animation at the next rendered frame.
    set_led(0, 180); redblade_rgb_begin_frame();
    assert(redblade_rgb_color(0).h == 17 && redblade_rgb_color(0).v == 180);
    assert(redblade_rgb_color(54).v == 0);
    now += REDBLADE_RGB_TIMEOUT_MS - 1; redblade_rgb_begin_frame();
    assert(redblade_rgb_color(0).v == 180);
    // Discovery, diagnostics, mode selection and malformed packets are not RGB heartbeats.
    uint8_t packet[32] = {0xFE, 0x80};
    assert(via_command_kb(packet, 32));
    assert(packet[2] == 1 && packet[4] == 2 && packet[6] == 9);
    assert(!memcmp(packet + 24, "RDBLFS01", 8));
    memset(packet, 0, 32); packet[0] = 7; packet[1] = 0x41; packet[2] = 1;
    assert(via_command_kb(packet, 32));
    memset(packet, 0, 32); packet[0] = 7; packet[1] = 0x42; // zero count
    assert(via_command_kb(packet, 32) && packet[0] == 0xFF);
    ++now; assert_black(); // Exact 5000 ms boundary.
    set_led(1, 140); redblade_rgb_begin_frame();
    assert(redblade_rgb_color(1).v == 140 && redblade_rgb_color(0).v == 0);
    // Long idle and the 32-bit millisecond wrap cannot revive an old frame.
    now += REDBLADE_RGB_TIMEOUT_MS; assert_black();
    now = 500; assert_black();
    now = UINT32_MAX - 1000; set_led(2, 123);
    now += REDBLADE_RGB_TIMEOUT_MS - 1; redblade_rgb_begin_frame();
    assert(redblade_rgb_color(2).v == 123);
    ++now; assert_black();

    now = 0; redblade_rgb_init();
    now = REDBLADE_BOOT_ANIMATION_MS; assert_black(); // One-shot boot, then darkness.
    now += 2000; assert_black();
    // Brightness-off and saved standalone effects never start the sweep.
    now = 0; enabled = false; redblade_rgb_init();
    now = REDBLADE_BOOT_ANIMATION_MS / 2; assert_black();
    enabled = true; mode = RGB_MATRIX_SOLID_COLOR; now = 0; redblade_rgb_init();
    now = REDBLADE_BOOT_ANIMATION_MS / 2; assert_black();
    mode = RGB_MATRIX_CUSTOM_REDBLADE_DIRECT; value = 10; now = 0; redblade_rgb_init();
    now = REDBLADE_BOOT_ANIMATION_MS / 2; redblade_rgb_begin_frame();
    for (unsigned i = 0; i < 109; ++i) assert(redblade_rgb_color(i).v <= 10);

    // Suspend invalidates colors; wake requires new host data, without a new animation.
    value = 255; set_led(3, 100); redblade_rgb_begin_frame();
    assert(redblade_rgb_color(3).v == 100);
    redblade_rgb_suspend(); redblade_rgb_suspend(); assert_black();
    redblade_rgb_wake(); assert_black();
    set_led(4, 80); redblade_rgb_begin_frame();
    assert(redblade_rgb_color(4).v == 80 && redblade_rgb_color(3).v == 0);
    // Even a very early suspend cancels the still-running boot sweep.
    now = 0; redblade_rgb_init(); redblade_rgb_suspend(); redblade_rgb_wake();
    now = REDBLADE_BOOT_ANIMATION_MS / 2; assert_black();
    puts("PASS: boot sweep, host priority, exact RGB timeout, no query heartbeat, recovery, timer wrap, suspend/wake");
}

int main(void) {
    uint8_t packet[32] = {0};
    // Standard VIA reads/writes must be left to upstream, without an ACK.
    for (unsigned command=1; command<=0x15; ++command) {
        memset(packet,0,32); packet[0]=command;
        unsigned before=responses;
        assert(!via_command_kb(packet,32)); assert(responses==before);
    }
    for (unsigned length=0; length<32; ++length) assert(!via_command_kb(packet,length));
    packet[0]=0xFE; packet[1]=0;
    assert(via_command_kb(packet,32));
    assert(packet[0]==4 && !memcmp(packet+4,"RDBLFS01",8) && packet[12]==1);

    memset(packet,0,32); packet[0]=8; packet[1]=0x43;
    assert(via_command_kb(packet,32)); assert(packet[2]==109 && packet[3]==0);
    bool seen[6][21]={{false}};
    for (unsigned i=0;i<109;++i) {
        memset(packet,0,32); packet[0]=8; packet[1]=0x44; packet[2]=i;
        assert(via_command_kb(packet,32));
        unsigned row=packet[5],col=packet[6];
        assert(row<6 && col<21 && !seen[row][col]); seen[row][col]=true;
    }
    memset(packet,0,32); packet[0]=8; packet[1]=0x44; packet[2]=109;
    assert(via_command_kb(packet,32) && packet[0]==0xFF);
    memset(packet,0,32); packet[0]=8; packet[1]=0x44; packet[3]=1;
    assert(via_command_kb(packet,32) && packet[0]==0xFF);

    // Fill all LEDs exactly as OpenRGB does: 9 HSV entries per report.
    for (unsigned first=0;first<109;first+=9) {
        memset(packet,0,32); packet[0]=7; packet[1]=0x42; packet[2]=first;
        packet[4]=109-first<9 ? 109-first : 9;
        for(unsigned i=0;i<packet[4];++i) { packet[5+i*3]=first+i; packet[6+i*3]=200; packet[7+i*3]=150; }
        assert(via_command_kb(packet,32) && packet[0]==7);
    }
    redblade_rgb_begin_frame();
    // Latest colors replace old state; no frame queue or dynamic allocation.
    for(unsigned i=0;i<109;++i) { hsv_t c=redblade_rgb_color(i); assert(c.h==i && c.s==200 && c.v==150); }
    // Exhaustive start/count boundaries must never alter the last good frame.
    for (unsigned first=0;first<300;++first) for(unsigned count=0;count<256;++count) {
        bool valid=count>0 && count<=9 && first<109 && count<=109-first;
        if(valid) continue;
        memset(packet,255,32); packet[0]=7; packet[1]=0x42; packet[2]=first; packet[3]=first>>8; packet[4]=count;
        assert(via_command_kb(packet,32) && packet[0]==255);
    }
    for(unsigned i=0;i<109;++i) assert(redblade_rgb_color(i).h==i);
    for(unsigned id=0;id<=2;++id) {
        memset(packet,0,32); packet[0]=7; packet[1]=0x41; packet[2]=id;
        assert(via_command_kb(packet,32) && packet[0]==7);
        assert(enabled==(id!=0));
        if(id==1) assert(mode==RGB_MATRIX_CUSTOM_REDBLADE_DIRECT);
    }
    memset(packet,0,32); packet[0]=8; packet[1]=0x42;
    assert(via_command_kb(packet,32) && packet[2]==1 && packet[4]==2 && packet[6]==255);
    memset(packet,0,32); packet[0]=8; packet[1]=0x42; packet[2]=2;
    assert(via_command_kb(packet,32) && packet[2]==255 && packet[3]==255);
    puts("PASS: VIA passthrough, discovery, all 109 LED positions, colors, 76,800 packet boundaries, modes");
    test_lifecycle();
}
