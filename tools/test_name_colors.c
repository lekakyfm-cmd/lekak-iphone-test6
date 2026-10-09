#include <assert.h>
#include <string.h>
#include <stdio.h>
/* This test uses ordinary host buffers, not the game's pinned 32-bit
 * pointer storage. Keep its pointer fixture consistent on GCC and Clang. */
#include "../EngineSDK/src/port_ptr.h"
#undef G32
#define G32
#include "../SourceMod/Lekak/name-colors.c"
static u16 vram[512][1024], initial[7][16];
static int attack, disc_reads, fail_store;
s16 gDuel_wSelectedCardID=1;
int Cards_Valid(int id){return id>0;}
int Cards_Type(int id){(void)id;return 0;}
int Duel_GetBaseCardStat(int id,int stat){(void)id;(void)stat;return attack;}
int DrawSync(int mode){(void)mode;return 0;}
int LoadImage(Rect *r,u32 *data){int y;for(y=0;y<r->h;y++)memcpy(&vram[r->y+y][r->x],(u16 *)data+y*r->w,r->w*2);return 0;}
int StoreImage(Rect *r,u32 *data){int y;if(fail_store)return -1;for(y=0;y<r->h;y++)memcpy((u16 *)data+y*r->w,&vram[r->y+y][r->x],r->w*2);return 0;}
static int disc_start(const Host *h,const char *p){(void)h;(void)p;return 1;}
static int disc_read(const Host *h,int lba,int count,void *data){int i;(void)h;(void)lba;(void)count;memset(data,0,2048);for(i=0;i<16;i++)((u16 *)data)[i]=(i*2)|((i*2)<<5)|((i*2)<<10);disc_reads++;return 1;}
static void original(Box *b){(void)b;}
void func_80037DA4(Box *b){(void)b;}
Box *DuelEffect_InitEntry(int a,int b,int c){(void)a;(void)b;(void)c;return 0;}
void TextBox_Destroy(Box *b){(void)b;}
int LekakBeforeColorsInit(const Host *h,Mod *m){(void)h;(void)m;return 1;}
int main(void){
 Host h={0};Box boxes[8]={{0}};u8 commands[8];u16 snapshot[7][16];int i,j;
 h.disc_file_start=disc_start;h.disc_read=disc_read;host=&h;orig_text=(void *)original;
 for(i=0;i<7;i++)for(j=0;j<16;j++)vram[FIRST_ROW+i][640+j]=initial[i][j]=0x1234+i;
 vram[239][640]=0x4567;vram[240][640]=0x789A;
 /* All seven tiers coexist. Changing selection must not retint cached names. */
 for(i=0;i<7;i++){
  commands[i]=0x20;((u8 **)boxes[i].bytes)[0]=&commands[i];boxes[i].bytes[0x54]=4;
  attack=(int[]){1800,2200,2800,3300,3800,4200,5000}[i];text(&boxes[i]);
  assert(boxes[i].bytes[0x54]==FIRST_COLOR+i);
  for(j=0;j<16;j++)assert(vram[FIRST_ROW+i][640+j]==(j?((j*2*rgb[i][2]/255)<<10)|((j*2*rgb[i][1]/255)<<5)|(j*2*rgb[i][0]/255):0));
 }
 for(i=0;i<7;i++)memcpy(snapshot[i],&vram[FIRST_ROW+i][640],32);
 for(i=0;i<20;i++){
  attack=i%2?1800:2800;text(&boxes[0]);assert(boxes[0].bytes[0x54]==FIRST_COLOR+(i%2?0:2));
  for(j=0;j<7;j++)assert(!memcmp(snapshot[j],&vram[FIRST_ROW+j][640],32));
 }
 assert(disc_reads==1);assert(vram[239][640]==0x4567 && vram[240][640]==0x789A);
 commands[0]=0x40;text(&boxes[0]);assert(boxes[0].bytes[0x54]==4);
 commands[0]=0x20;attack=1500;text(&boxes[0]);assert(boxes[0].bytes[0x54]==4);
 cleanup();for(i=0;i<7;i++){assert(boxes[i].bytes[0x54]==4);assert(!memcmp(initial[i],&vram[FIRST_ROW+i][640],32));}
 fail_store=1;attack=5000;text(&boxes[0]);assert(boxes[0].bytes[0x54]==4);
 assert(tier(1500)==-1 && tier(1501)==0 && tier(2000)==0 && tier(2001)==1 && tier(4500)==5 && tier(4501)==6);
 puts("Independent name palettes: mixed tiers, repeated selection, vanilla/detail text, restore and failed upload passed.");return 0;
}
