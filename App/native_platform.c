#include "native_platform.h"
#include "pc/platform/platform.h"
#include "pc/platform/menu.h"
#include "pc/debug/hud.h"
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
static LekakNativeServices services;
static atomic_uint pads[2],connected[2],quit;
static uint32_t *picture;
static size_t capacity;
static int width,height,state_slot,scale=1;
int LekakNative_Install(const LekakNativeServices *value) {
    if(!value||!value->open||!value->frame||!value->error||!value->audio||!value->pump)return 0;
    services=*value;atomic_store(&quit,0);LekakNative_ReleasePads();return 1;
}
void LekakNative_SetPad(unsigned port,uint16_t bits,int present) {
    if(port>=2)return;
    atomic_store(&pads[port],present?bits:0);atomic_store(&connected[port],!!present);
}
void LekakNative_ReleasePads(void) {
    for(unsigned i=0;i<2;i++){atomic_store(&pads[i],0);atomic_store(&connected[i],i==0);}
}
void LekakNative_Clear(void) {
    LekakNative_ReleasePads();memset(&services,0,sizeof(services));
    free(picture);picture=NULL;capacity=0;width=height=0;
}
static int reserve(int w,int h) {
    /* VRAM and HD software pictures are bounded to avoid corrupt geometry
     * turning into a huge allocation. 4096 includes scale 6 at 640 pixels. */
    if(w<=0||h<=0||w>4096||h>4096)return 0;
    size_t n=(size_t)w*h;
    if(n>capacity){void *p=realloc(picture,n*sizeof(*picture));if(!p)return 0;picture=p;capacity=n;}
    width=w;height=h;return 1;
}
int Platform_Open(const char *title) {
    (void)title;if(!services.open)return -1;
    /* Real upstream settings, asset/audio loaders and mod registry. */
    Menu_LoadSettings();return services.open(services.context);
}
void Platform_ShowError(const char *title,const char *message) {
    if(services.error)services.error(services.context,title,message);
}
int Platform_SelectDisc(char *p,size_t n,char *why,size_t wn) {
    (void)p;(void)n;
    const char *s="Import the USA disc through the iPhone document picker before launching.";
    if(why&&wn){size_t k=strlen(s);if(k>=wn)k=wn-1;memcpy(why,s,k);why[k]=0;}return -1;
}
static void present_overlay(void);
void Platform_Present(const uint16_t *vram,int stride,int x,int y,int w,int h,int rgb24) {
    if(!vram||stride<=0||x<0||y<0||y+h>512||
       (!rgb24&&x+w>stride)||(rgb24&&((size_t)x*2+(size_t)w*3>(size_t)stride*2))||!reserve(w,h))return;
    for(int row=0;row<h;row++)for(int col=0;col<w;col++) {
        uint32_t rgb;
        if(rgb24){const unsigned char *p=(const unsigned char *)(vram+(y+row)*stride+x)+col*3;rgb=(uint32_t)p[0]<<16|(uint32_t)p[1]<<8|p[2];}
        else{unsigned p=vram[(y+row)*stride+x+col],r=p&31,g=(p>>5)&31,b=(p>>10)&31;
            rgb=((r<<3)|(r>>2))<<16|((g<<3)|(g>>2))<<8|((b<<3)|(b>>2));}
        picture[(size_t)row*w+col]=rgb;
    }
    present_overlay();
}
int Platform_PresentPicture(const uint32_t *p,int stride,int x,int y,int w,int h,int at_scale) {
    if(!p||stride<=0||x<0||y<0||w>stride||x>stride-w||at_scale<1||!reserve(w,h))return 0;
    for(int row=0;row<h;row++)memcpy(picture+(size_t)row*w,p+(size_t)(y+row)*stride+x,(size_t)w*sizeof(*p));
    present_overlay();return 1;
}
static void present_overlay(void) {
    MenuCanvas canvas={picture,width,width,height,0};
    Menu_SetOverlayArea(0,0,-1);
    Hud_Draw(&canvas);
    if(services.frame)services.frame(services.context,picture,width,height);
}
int Platform_ReadPicture(uint32_t *out,int x,int y,int w,int h) {
    if(!out||!picture||x<0||y<0||w<=0||h<=0||w>width||h>height||x>width-w||y>height-h)return 0;
    for(int row=0;row<h;row++) { memcpy(out+(size_t)row*w,picture+(size_t)(y+row)*width+x,(size_t)w*sizeof(*out)); }
    return 1;
}
int Platform_PresentWidePicture(int x,int y,int w,int h,int wide,int s){(void)x;(void)y;(void)w;(void)h;(void)wide;(void)s;return 0;}
int Platform_ReadWidePicture(uint32_t *p,int x,int y,int w,int h,int wide,int s){(void)p;(void)x;(void)y;(void)w;(void)h;(void)wide;(void)s;return 0;}
int Platform_Widescreen(void){return 0;}
int Platform_ShouldQuit(void){return atomic_load(&quit);}
void Platform_RequestQuit(void){atomic_store(&quit,1);LekakNative_ReleasePads();}
int Platform_StateSlot(void){return state_slot;}
void Platform_SetStateSlot(int s){if(s>=0&&s<10)state_slot=s;}
int Platform_Scale(void){return scale;}
void Platform_SetScale(int s){if(s>=1&&s<=6)scale=s;}
void Platform_ApplyDisplaySettings(void){}
int Platform_HasWindowModes(void){return 0;}
void Platform_PumpEvents(void){if(services.pump)services.pump(services.context);}
void Platform_Frame(unsigned frame){(void)frame;Platform_PumpEvents();}
uint16_t Platform_Pad(int p){return p>=0&&p<2?(uint16_t)atomic_load(&pads[p]):0;}
int Platform_PadConnected(int p){return p>=0&&p<2?(int)atomic_load(&connected[p]):0;}
uint16_t Platform_PadFixedBits(int p){(void)p;return 0;}
void Gamepad_Poll(unsigned frame){(void)frame;}
uint16_t Gamepad_Bits(int p){return Platform_Pad(p);}
int Gamepad_Connected(int p){return Platform_PadConnected(p);}
int Platform_StartAudio(void (*mix)(int16_t *,size_t)){return services.audio?services.audio(services.context,mix):-1;}
void Platform_AudioStats(int *queued,unsigned *underruns){if(queued)*queued=0;if(underruns)*underruns=0;}
/* Desktop-only operations explicitly report unsupported. */
int Platform_OpenFolder(const char *p){(void)p;return -1;}
int Platform_OpenUrl(const char *p){(void)p;return -1;}
int Platform_CopyText(const char *p){(void)p;return -1;}
void Platform_OpenMods(void){Platform_ShowError("Mods","Lekak is included statically in this iPhone build.");}
void Platform_OpenControls(void){Platform_ShowError("Controls","Use the iPhone touch controls.");}
void Platform_Screenshot(int window){(void)window;Platform_ShowError("Screenshot","Use the iPhone screenshot buttons.");}
int Platform_RestartGame(void){return -1;}
