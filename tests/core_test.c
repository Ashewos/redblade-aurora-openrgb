#include "redblade_core.h"
#include "qmk_fixture.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static hsv_t color(uint8_t i) { redblade_core_begin_frame();return redblade_core_color(i); }
int main(void) {
    redblade_core_init(true);assert(color(0).v==0);
    uint8_t packet[32]={7,0x42,108,0,1,20,255,160};
    assert(via_command_kb(packet,32) && color(108).v==160);
    for(unsigned first=0;first<300;++first) for(unsigned count=0;count<256;++count) {
        uint8_t p[32]={7,0x42,first,first>>8,count};
        assert(via_command_kb(p,32));assert((p[0]!=255)==(count>=1 && count<=9 && first<109 && count<=109-first));
    }
    assert(via_command_kb(packet,32));now+=4999;assert(color(108).v==160);
    now+=1;assert(color(108).v==0);
    // VIA standard effect writes and remapped RGB keys must not leave a local
    // animation outside Core's Direct watchdog. Simulate the upstream setter.
    uint8_t effect[32]={7,3,2,RGB_MATRIX_SOLID_COLOR};
    assert(via_command_kb(effect,32));assert(mode==RGB_MATRIX_CUSTOM_REDBLADE_DIRECT);
    mode=RGB_MATRIX_SOLID_COLOR; // A remapped native RGB mode key.
    redblade_core_task(false);assert(mode==RGB_MATRIX_CUSTOM_REDBLADE_DIRECT);
    assert(color(108).v==0);
    assert(via_command_kb(packet,32));redblade_core_usb(false);assert(color(108).v==0);
    redblade_core_usb(true);assert(color(108).v==0); // No stale pre-sleep frame.
    uint8_t positions[6][21]={{0}};
    for(unsigned i=0;i<109;++i) {
        uint8_t p[32]={8,0x44,i};assert(via_command_kb(p,32));
        assert(p[5]<6 && p[6]<21 && !positions[p[5]][p[6]]++);
    }
    uint8_t d[32]={0xFE,0x81};assert(via_command_kb(d,32));
    assert(d[19]==128 && !d[16] && !memcmp(d+24,"RDBLFS01",8));
    keyrecord_t r={.event={.key={.row=0,.col=0},.pressed=true}};
    assert(redblade_core_key(4,&r));assert(!redblade_core_key(RB_CORE_BOOT,&r));
    now+=1999;redblade_core_task(true);assert(!bootloader_calls);
    ++now;redblade_core_task(true);assert(bootloader_calls==1);
    assert(!redblade_core_key(RB_CORE_BOOT,&r));redblade_core_task(false);
    now+=5000;redblade_core_task(true);assert(bootloader_calls==1);
    assert(!eeprom_writes && !key_down_calls && !mouse_reports);
    puts("PASS: Core per-key RGB, packet bounds, 109 positions, watchdog/suspend off, guarded boot, no automation");
}
