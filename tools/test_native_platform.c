#include "native_platform.h"
#include "pc/platform/platform.h"
#include <assert.h>
#include <string.h>
static unsigned shown,opened,pumped;
static uint32_t last[12];static int lw,lh;
void Menu_LoadSettings(void){}
static int open_window(void *p){assert(p==&shown);opened++;return 0;}
static void frame(void *p,const uint32_t *pixels,int w,int h){assert(p==&shown);assert(w*h<=12);memcpy(last,pixels,(size_t)w*h*4);lw=w;lh=h;shown++;}
static void error(void *p,const char *t,const char *m){(void)p;(void)t;(void)m;}
static int audio(void *p,void (*mix)(int16_t *,size_t)){(void)p;(void)mix;return 0;}
static void pump(void *p){assert(p==&shown);pumped++;}
int main(void){
    LekakNativeServices s={&shown,open_window,frame,error,audio,pump};
    assert(!LekakNative_Install(NULL));assert(LekakNative_Install(&s));
    assert(Platform_Open("Lekak")==0&&opened==1);
    assert(Platform_PadConnected(0)&&!Platform_PadConnected(1));
    LekakNative_SetPad(0,0x4000,1);assert(Platform_Pad(0)==0x4000);
    LekakNative_SetPad(1,0x1000,1);assert(Platform_PadConnected(1));
    LekakNative_ReleasePads();assert(!Platform_Pad(0)&&!Platform_Pad(1));
    uint16_t vram[1024*512]={0};
    vram[1025]=31;vram[1026]=31<<5;vram[2049]=31<<10;vram[2050]=0x7fff;
    Platform_Present(vram,1024,1,1,2,2,0);
    assert(shown==1&&lw==2&&lh==2);
    assert(last[0]==0xff0000&&last[1]==0x00ff00&&last[2]==0x0000ff&&last[3]==0xffffff);
    uint32_t crop;assert(Platform_ReadPicture(&crop,1,1,1,1)&&crop==0xffffff);
    assert(!Platform_ReadPicture(&crop,2,1,1,1));
    unsigned char *rgb=(unsigned char *)(vram+1024+2);
    rgb[0]=12;rgb[1]=34;rgb[2]=56;rgb[3]=78;rgb[4]=90;rgb[5]=123;
    Platform_Present(vram,1024,2,1,2,1,1);
    assert(shown==2&&last[0]==0x0c2238&&last[1]==0x4e5a7b);
    Platform_Present(vram,1024,1023,1,2,1,1);assert(shown==2);
    Platform_Present(vram,1024,0,511,1,2,0);assert(shown==2);
    uint32_t hd[]={1,2,3,4,5,6,7,8,9,10,11,12};
    assert(Platform_PresentPicture(hd,4,1,1,2,2,2));
    assert(last[0]==6&&last[1]==7&&last[2]==10&&last[3]==11);
    assert(!Platform_PresentPicture(hd,4,3,0,2,1,1));
    Platform_Frame(1);assert(pumped==1);
    Platform_RequestQuit();assert(Platform_ShouldQuit()&&!Platform_Pad(0));
    LekakNative_Clear();assert(LekakNative_Install(&s));assert(!Platform_ShouldQuit());
    LekakNative_Clear();return 0;
}
