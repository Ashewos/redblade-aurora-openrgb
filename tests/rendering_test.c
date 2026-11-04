// Exercise the actual custom-effect include and pinned QMK color conversion.
#include "redblade_rgb.h"
#include "redblade_controls.h"
#include "qmk_fixture.h"
#include <assert.h>
#include <stdio.h>
typedef struct { uint8_t iter; } effect_params_t;
#define pixels rendered
#define RGB_MATRIX_EFFECT(name)
#define RGB_MATRIX_CUSTOM_EFFECT_IMPLS
#define RGB_MATRIX_USE_LIMITS(min,max) const uint8_t min=params->iter*8; const uint8_t max=min+8<109?min+8:109
static bool rgb_matrix_check_finished_leds(uint8_t max) { return max<109; }
#include "rgb_matrix_user.inc"
static void render(void) {
    effect_params_t params={0};
    while(REDBLADE_DIRECT(&params)) ++params.iter;
}
static unsigned red_center(void) {
    unsigned weighted=0,total=0;
    for(unsigned i=0;i<109;++i) { assert(pixels[i].r>=pixels[i].g && pixels[i].r>=pixels[i].b); weighted+=pixels[i].r*g_led_config.point[i].x;total+=pixels[i].r; }
    assert(total>1000); return weighted/total;
}
int main(void) {
    redblade_rgb_init(true); redblade_controls_init();
    // Reproduce OpenRGB's Direct request with all unused scalar fields zero.
    uint8_t direct[32]={7,0x41,1};
    assert(via_command_kb(direct,32) && value==255);
    now=500; render(); const unsigned left=red_center();
    now=1000; render(); assert(red_center()>left+30);
    now=2000; render();
    for(unsigned i=16;i<=19;++i) assert(pixels[i].r==64 && pixels[i].g==64 && pixels[i].b==64);
    uint8_t packet[32]={7,0x42,44,0,1,0,255,80}; assert(via_command_kb(packet,32));
    render(); assert(pixels[44].r==80 && pixels[44].g<2 && pixels[44].b==0);
    redblade_rgb_replay(); now+=500; render(); red_center();
    redblade_rgb_suspend(); render();
    for(unsigned i=0;i<109;++i) assert(!pixels[i].r && !pixels[i].g && !pixels[i].b);
    redblade_rgb_wake(); now+=REDBLADE_RGB_TIMEOUT_MS; render();
    redblade_controls_render(0,109,false);
    unsigned dim_keys=0;
    for(unsigned i=0;i<109;++i) if(pixels[i].r==8 && pixels[i].g==8 && pixels[i].b==8) ++dim_keys;
    assert(dim_keys==17 && pixels[RB_LED_NUM].r==8); // Entire numpad very dim when Num Lock is off.
    host_locks=(led_t){.num_lock=true,.caps_lock=true,.scroll_lock=true};
    render(); redblade_controls_render(0,109,false);
    assert(pixels[RB_LED_CAPS].g==128 && !pixels[RB_LED_CAPS].r);
    assert(pixels[RB_LED_NUM].g==128 && !pixels[RB_LED_NUM].r);
    assert(pixels[57].r==64 && pixels[57].g==64 && pixels[RB_LED_SCROLL].r==128);
    assert(pixels[RB_LED_FN].b==24 && pixels[103].r==64 && pixels[103].b==64); // Actual Fn, not Right Ctrl.
    uint8_t stream[32]={7,0x42,RB_LED_CAPS,0,1,0,255,200}; assert(via_command_kb(stream,32));
    render(); redblade_controls_render(0,109,false);
    assert(pixels[RB_LED_CAPS].r==200 && !pixels[RB_LED_CAPS].g); // Fresh host takes priority over lock fallback.
    keyrecord_t game={.event={.key={.row=5,.col=1},.pressed=true}};
    redblade_controls_process(RB_GAME,&game); redblade_controls_render(0,109,false);
    assert(pixels[RB_LED_LGUI].r==128 && !pixels[RB_LED_LGUI].g && pixels[RB_LED_RGUI].r==128);
    redblade_rgb_profile(RB_PROFILE_OFF); render(); redblade_controls_render(0,109,true);
    for(unsigned i=0;i<109;++i) assert(!pixels[i].r && !pixels[i].g && !pixels[i].b);
    puts("PASS: actual 8-LED effect rendering, zero-field Direct requests, moving red wave, 4 visible media LEDs, linear host brightness, USB off");
}
