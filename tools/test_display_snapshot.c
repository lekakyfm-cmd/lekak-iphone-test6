#include "../App/display_snapshot.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
int main(void){
 uint16_t *vram=calloc(1024*512,sizeof(*vram));uint8_t rgba[256*2*4];assert(vram);unsigned w=7,h=9;
 uint32_t origin=(511u<<10)|1023u,vertical=(18u<<10)|16u;
 vram[511*1024+1023]=31;vram[511*1024]=31<<5;vram[1023]=31<<10;
 assert(LekakDisplay_CopyRGBA(vram,0,origin,vertical,rgba,sizeof(rgba),&w,&h)&&w==256&&h==2);
 assert(rgba[0]==255&&rgba[1]==0&&rgba[2]==0&&rgba[3]==255);
 assert(rgba[4]==0&&rgba[5]==255&&rgba[6]==0&&rgba[7]==255);
 assert(rgba[256*4+2]==255);
 memset(rgba,0xa5,sizeof(rgba));w=7;h=9;
 assert(!LekakDisplay_CopyRGBA(vram,0,origin,vertical,rgba,sizeof(rgba)-1,&w,&h)&&w==7&&h==9&&rgba[0]==0xa5);
 assert(!LekakDisplay_CopyRGBA(vram,0x400000u,origin,vertical,rgba,sizeof(rgba),&w,&h));
 vram[511*1024+1023]=0x1122;vram[511*1024]=0x4433;
 assert(LekakDisplay_CopyRGBA(vram,0x200000u,origin,vertical,rgba,sizeof(rgba),&w,&h));
 assert(rgba[0]==0x22&&rgba[1]==0x11&&rgba[2]==0x33&&rgba[3]==255);
 assert(LekakDisplay_CopyRGBA(vram,0x800000u,origin,vertical,rgba,sizeof(rgba),&w,&h));
 for(unsigned i=0;i<256*2;i++)assert(!rgba[i*4]&&!rgba[i*4+1]&&!rgba[i*4+2]&&rgba[i*4+3]==255);
 free(vram);puts("Display snapshot: RGB555/RGB24, origins/axis/byte wrapping, display disable and atomic capacity/mode validation passed");return 0;
}
