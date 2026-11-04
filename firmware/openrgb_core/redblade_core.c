// SPDX-License-Identifier: GPL-2.0-or-later
#include "redblade_core.h"
#include "raw_hid.h"
#include <string.h>
#include "led_positions.inc"
static hsv_t incoming[RGB_MATRIX_LED_COUNT];
static bool configured,have_stream,frame_active,boot_armed;
static uint32_t last_packet,packets,boot_started;
static keypos_t boot_key;
_Static_assert(RGB_MATRIX_LED_COUNT == 109,"Red Blade Full Size only");
static uint16_t read16(const uint8_t *p) { return p[0] | ((uint16_t)p[1]<<8); }
static void write16(uint8_t *p,uint16_t n) { p[0]=n;p[1]=n>>8; }
static void write32(uint8_t *p,uint32_t n) { write16(p,n);write16(p+2,n>>16); }
static void clear_stream(void) { have_stream=frame_active=false;memset(incoming,0,sizeof(incoming)); }
void redblade_core_init(bool active) {
    configured=active;packets=0;boot_armed=false;clear_stream();
    rgb_matrix_mode_noeeprom(RGB_MATRIX_CUSTOM_REDBLADE_DIRECT);
}
void redblade_core_usb(bool active) {
    if(configured!=active) clear_stream();
    configured=active;if(!active) boot_armed=false;
}
bool redblade_core_key(uint16_t code,keyrecord_t *r) {
    if(!r->event.pressed && r->event.key.row==boot_key.row && r->event.key.col==boot_key.col) boot_armed=false;
    if(code!=RB_CORE_BOOT) return code<SAFE_RANGE; // Retired RedBlade+ cells never run helpers.
    if(r->event.pressed && !boot_armed) { boot_key=r->event.key;boot_started=timer_read32();boot_armed=true; }
    return false;
}
void redblade_core_task(bool fn_active) {
    // VIA's standard RGB channel and remapped RGB mode keys bypass VialRGB.
    // Core owns the renderer in every case; keep Off, but no local effect.
    if(rgb_matrix_get_mode()!=RGB_MATRIX_CUSTOM_REDBLADE_DIRECT) rgb_matrix_mode_noeeprom(RGB_MATRIX_CUSTOM_REDBLADE_DIRECT);
    redblade_core_begin_frame();
    if(!fn_active) boot_armed=false;
    if(boot_armed && timer_read32()-boot_started>=2000UL) { boot_armed=false;reset_keyboard(); }
}
void redblade_core_begin_frame(void) {
    if(have_stream && timer_read32()-last_packet>=REDBLADE_RGB_TIMEOUT_MS) clear_stream();
    frame_active=configured && have_stream && rgb_matrix_is_enabled() && rgb_matrix_get_mode()==RGB_MATRIX_CUSTOM_REDBLADE_DIRECT;
}
hsv_t redblade_core_color(uint8_t index) { return frame_active && index<109 ? incoming[index] : (hsv_t){0}; }
bool via_command_kb(uint8_t *data,uint8_t length) {
    if(length!=32) return false;
    if((data[0]==7 || data[0]==8) && data[1]==3 && data[2]==2) {
        // Standard VIA RGB-matrix effect channel, separate from VialRGB.
        if(data[0]==7) {
            if(data[3]==0) rgb_matrix_disable_noeeprom();
            else { rgb_matrix_enable_noeeprom();rgb_matrix_mode_noeeprom(RGB_MATRIX_CUSTOM_REDBLADE_DIRECT); }
        }
        data[3]=rgb_matrix_is_enabled()?RGB_MATRIX_CUSTOM_REDBLADE_DIRECT:0;
        raw_hid_send(data,32);return true;
    }
    if(data[0]==0xFE && data[1]==0) {
        memset(data,0,32);data[0]=4;memcpy(data+4,"RDBLFS01",8);data[12]=1;
    } else if(data[0]==0xFE && (data[1]==0x80 || data[1]==0x81)) {
        const uint8_t kind=data[1];memset(data+2,0,30);data[2]=1;
        data[3]=REDBLADE_VERSION_MAJOR;data[4]=REDBLADE_VERSION_MINOR;data[5]=REDBLADE_VERSION_PATCH;
        if(kind==0x80) {
            const uint32_t age=have_stream ? timer_read32()-last_packet : UINT32_MAX;
            data[6]=(have_stream && age<REDBLADE_RGB_TIMEOUT_MS && configured ? 1 : 0) | (!configured ? 4 : 0) | (rgb_matrix_is_enabled() ? 8 : 0);
            data[7]=rgb_matrix_is_enabled() && rgb_matrix_get_mode()==RGB_MATRIX_CUSTOM_REDBLADE_DIRECT;write32(data+8,age);write32(data+12,REDBLADE_RGB_TIMEOUT_MS);write32(data+16,packets);
        } else { data[8]=255;write16(data+10,2000);write32(data+12,REDBLADE_RGB_TIMEOUT_MS);data[19]=128; }
        memcpy(data+24,"RDBLFS01",8);
    } else {
        const uint8_t command=data[0],sub=data[1];uint8_t *args=data+2;
        if((command!=7 && command!=8) || sub<0x40 || sub>0x44) return false;
        if(command==8) {
            const uint16_t requested=read16(args);memset(args,0,30);
            switch(sub) {
                case 0x40:write16(args,1);args[2]=255;break;
                case 0x41:write16(args,rgb_matrix_is_enabled()?1:0);args[5]=255;break;
                case 0x42:memset(args,255,30);if(requested<1) write16(args,1);break;
                case 0x43:write16(args,109);break;
                case 0x44:
                    if(requested>=109) data[0]=255;
                    else { args[0]=g_led_config.point[requested].x;args[1]=g_led_config.point[requested].y;args[2]=g_led_config.flags[requested];args[3]=led_positions[requested][0];args[4]=led_positions[requested][1]; }
                    break;
            }
        } else if(sub==0x41) {
            const uint16_t mode=read16(args);
            if(mode>1) data[0]=255;
            else if(!mode) rgb_matrix_disable_noeeprom();
            else { rgb_matrix_enable_noeeprom();rgb_matrix_mode_noeeprom(RGB_MATRIX_CUSTOM_REDBLADE_DIRECT); }
        } else if(sub==0x42) {
            const uint16_t first=read16(args);const uint8_t count=args[2];
            if(!count || count>9 || first>=109 || count>109-first) data[0]=255;
            else {
                for(uint8_t i=0;i<count;++i) incoming[first+i]=(hsv_t){args[3+i*3],args[4+i*3],args[5+i*3]};
                have_stream=true;last_packet=timer_read32();++packets;
            }
        } else data[0]=255;
    }
    raw_hid_send(data,32);return true;
}
