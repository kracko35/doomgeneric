/* GPL-2.0-or-later. NHDT SH-4A guest display and time adapter. */
#include <stdint.h>
#include <string.h>
#include "doomgeneric.h"
#include "doomstat.h"

volatile struct {
    uint32_t magic,frames,ticks,gamestate,demoplayback,gametic,stop,error;
} navi_state;
unsigned navi_wad_size=1; /* Loaded by the runner; retained across BSS clearing. */
#define DU ((volatile uint32_t *)0xfff80000u)
#define REG8(a) (*(volatile uint8_t *)(a))
#define REG16(a) (*(volatile uint16_t *)(a))
#define REG32(a) (*(volatile uint32_t *)(a))
static uint32_t previous_timer, elapsed_ms, remainder;

void navi_exit(int status) {
    navi_state.error=status ? (unsigned)status : 0x100;
    for (;;) __asm__ volatile("nop");
}
void DG_Init(void) {
    navi_state.magic=0x444f4f4d;
    DU[0x40/4]=0;DU[0x44/4]=416;DU[0x48/4]=0;DU[0x4c/4]=240;
    DU[0x18/4]=8;DU[0x100/4]=0x4001;DU[0x104/4]=416;DU[0x108/4]=255;
    DU[0x110/4]=416;DU[0x114/4]=240;DU[0x118/4]=0;DU[0x11c/4]=0;
    DU[0x130/4]=0;DU[0x134/4]=0;DU[0x00/4]=0x100100;
    REG8(0xffd81004)=0;REG16(0xffd81010)=0;
    REG32(0xffd81008)=0xffffffff;REG32(0xffd8100c)=0xffffffff;
    previous_timer=0xffffffff;REG8(0xffd81004)=1;
}
uint32_t DG_GetTicksMs(void) {
    uint32_t now=REG32(0xffd8100c);
    uint32_t delta=previous_timer-now;previous_timer=now;
    uint32_t count=delta+remainder;
    elapsed_ms+=count/12500;remainder=count%12500;
    navi_state.ticks=elapsed_ms;
    return elapsed_ms;
}
void DG_SleepMs(uint32_t ms) {
    uint32_t begin=DG_GetTicksMs();
    while(DG_GetTicksMs()-begin<ms && !navi_state.stop) { }
}
void DG_DrawFrame(void) {
    uint32_t physical=(navi_state.frames&1)?0x0a940000:0x0a900000;
    volatile uint16_t *fb=(volatile uint16_t *)(physical|0xa0000000u);
    /* Doomgeneric's framebuffer is byte-ordered RGBA on big endian too. */
    const uint8_t *pixels=(const uint8_t *)DG_ScreenBuffer;
    for(unsigned y=0;y<240;y++) {
        unsigned sy=y*200/240;
        for(unsigned x=0;x<416;x++) {
            unsigned sx=x*320/416;
            const uint8_t *p=pixels+4*(sy*320+sx);
            fb[y*416+x]=(uint16_t)(((p[2]&248)<<8)|((p[1]&252)<<3)|(p[0]>>3));
        }
    }
    DU[0x120/4]=physical;DU[0x124/4]=physical;
    navi_state.frames++;navi_state.gamestate=gamestate;
    navi_state.demoplayback=demoplayback;navi_state.gametic=gametic;
}
int DG_GetKey(int *pressed,unsigned char *key) { (void)pressed;(void)key;return 0; }
void DG_SetWindowTitle(const char *title) { (void)title; }
void navi_main(void) {
    char *args[]={"doom-navi","-iwad","doom1.wad","-nosound","-nomouse","-mb","16",0};
    doomgeneric_Create(7,args);
    while(!navi_state.stop) doomgeneric_Tick();
    for(;;) __asm__ volatile("nop");
}
