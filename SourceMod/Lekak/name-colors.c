/* Lekak name-only ATK colours. ABI prefix verified against modapi.h API 5.
 * Each ATK tier has its own CLUT on rows 241..247. Retail font ramps
 * (232..239) and the page-two background palette (240) remain untouched.
 * Scope: the game's selected-card detail textbox (F8 card-name command).
 */
#include "pc/mods/modapi.h"
#include "types.h"
typedef MemoriesModHost Host;
typedef MemoriesMod Mod;
typedef struct { u8 bytes[0x64]; } Box;
typedef struct { s16 x,y,w,h; } Rect;
#if __SIZEOF_POINTER__ == 4
_Static_assert(__builtin_offsetof(Host,hook)==0x50,"host hook ABI");
#endif
extern int LekakBeforeColorsInit(const Host *,Mod *);
extern void func_80037DA4(Box *);
extern Box *DuelEffect_InitEntry(int,int,int);
extern void TextBox_Destroy(Box *);
extern s16 gDuel_wSelectedCardID;
extern int Cards_Valid(int),Cards_Type(int),Duel_GetBaseCardStat(int,int);
extern int LoadImage(Rect *,u32 *),StoreImage(Rect *,u32 *),DrawSync(int);
static const Host *host;
static void *orig_text,*orig_init,*orig_destroy;
static void (*previous_reset)(void),(*previous_applied)(int),(*previous_shutdown)(void);
static u32 disc_ramps[64];
static int have_ramps;
#define FIRST_COLOR 9
#define FIRST_ROW (232+FIRST_COLOR)
typedef struct { Box *box; u8 color; } Saved;
static Saved saved[16];
/* Yellow, orange, red, violet, electric blue, neon green, cyan. */
static const u8 rgb[7][3]={{255,235,60},{255,145,35},{255,65,65},{200,90,255},
 {70,135,255},{80,255,30},{45,255,245}};
static int tier(int attack) {
 if (attack<=1500) return -1;
 if (attack<=2000) return 0;
 if (attack<=2500) return 1;
 if (attack<=3000) return 2;
 if (attack<=3500) return 3;
 if (attack<4000) return 4;
 if (attack<=4500) return 5;
 return 6;
}
static void restore(Box *box) {
 int i; for(i=0;i<16;i++) if(saved[i].box==box) {
  if(box->bytes[0x54]>=FIRST_COLOR && box->bytes[0x54]<FIRST_COLOR+7)
   box->bytes[0x54]=saved[i].color;
  saved[i].box=0;
 }
}
static int remember(Box *box) {
 int i; for(i=0;i<16;i++) if(!saved[i].box) {
  saved[i].box=box;saved[i].color=box->bytes[0x54];return 1;
 } return 0;
}
static int upload(int t) {
 u32 aligned[56]; u16 *out=(u16 *)aligned,*base=(u16 *)disc_ramps;
 Rect rect={640,FIRST_ROW,16,7}; int i,lba,color;
 (void)t;
 if(!(have_ramps&1)) {
  u32 sector[512];
  lba=host->disc_file_start(host,"\\DATA\\WA_MRG.MRG;1");
  if(lba<0 || host->disc_read(host,lba+0x16C2,1,sector)<=0) return 0;
  for(i=0;i<8;i++) disc_ramps[i]=sector[i];
  have_ramps|=1;
 }
 if(!(have_ramps&2)) {
  DrawSync(0);
  if(StoreImage(&rect,disc_ramps+8)<0) return 0;
  DrawSync(0); have_ramps|=2;
 }
 for(color=0;color<7;color++) for(i=0;i<16;i++) {
  unsigned lum=base[i]&31;
  out[color*16+i]=base[i] ? (u16)(((lum*rgb[color][2]/255)<<10)|((lum*rgb[color][1]/255)<<5)|(lum*rgb[color][0]/255)):0;
  if(base[i] && !out[color*16+i]) out[color*16+i]=0x8000;
 }
 /* Always upload: boot/overlays/save states may have overwritten VRAM. */
 DrawSync(0); LoadImage(&rect,aligned); DrawSync(0); return 1;
}
static void text(Box *box) {
 int stream=(s8)box->bytes[0x58],command=0,id,t;
 if(stream>=0 && stream<22) {u8 *p=((u8 *G32 *)box)[stream];if(p) command=*p;}
 restore(box);
 ((void (*)(Box *))orig_text)(box);
 id=gDuel_wSelectedCardID;
 if((command&0x10) || !(command&0x20) || !Cards_Valid(id) || Cards_Type(id)>=20) return;
 t=tier(Duel_GetBaseCardStat(id,0));
 if(t>=0 && upload(t) && remember(box)) box->bytes[0x54]=FIRST_COLOR+t;
}
static Box *init(int a,int b,int c) {
 Box *box=((Box *(*)(int,int,int))orig_init)(a,b,c);
 int i;for(i=0;i<16;i++) if(saved[i].box==box) saved[i].box=0;
 return box;
}
static void destroy(Box *box) {restore(box);((void (*)(Box *))orig_destroy)(box);}
static void cleanup(void) {
 int i; Rect rect={640,FIRST_ROW,16,7};
 for(i=0;i<16;i++) if(saved[i].box) restore(saved[i].box);
 if(have_ramps&2) {DrawSync(0);LoadImage(&rect,disc_ramps+8);DrawSync(0);have_ramps&=1;}
}
static void reset(void) {cleanup();if(previous_reset) previous_reset();}
static void applied(int on) {if(!on) cleanup();if(previous_applied) previous_applied(on);}
static void shutdown(void) {cleanup();if(previous_shutdown) previous_shutdown();}
int MemoriesModInit(const Host *h,Mod *m) {
 if(h->api<5 || !h->hook || !h->disc_file_start || !h->disc_read) return 0;
 if(!LekakBeforeColorsInit(h,m)) return 0;
 host=h;
 if(!h->hook(h,(void *)func_80037DA4,(void *)text,&orig_text)
 || !h->hook(h,(void *)DuelEffect_InitEntry,(void *)init,&orig_init)
 || !h->hook(h,(void *)TextBox_Destroy,(void *)destroy,&orig_destroy)) return 0;
 previous_reset=m->reset;previous_applied=m->applied;previous_shutdown=m->shutdown;
 m->reset=reset;m->applied=applied;m->shutdown=shutdown;return 1;
}
