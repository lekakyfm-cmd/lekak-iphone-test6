/* Lekak name-only ATK colours. ABI prefix verified against modapi.h API 5.
 * Uses only the retail unused CLUT 7, leaving all seven existing ramps intact.
 * Scope: the game's selected-card detail textbox (F8 card-name command).
 */
#include "pc/mods/modapi.h"
#include "types.h"
typedef MemoriesModHost Host;
typedef MemoriesMod Mod;
typedef struct { u8 bytes[0x70]; } Object;
typedef struct { s16 x,y,w,h; } Rect;
#if __SIZEOF_POINTER__ == 4
_Static_assert(__builtin_offsetof(Host,hook)==0x50,"host hook ABI");
#endif
/* Page 2 only: replace the rear background object's sheet during drawing.
 * Original object and original background textures are never overwritten.
 * Custom 8bpp picture occupies the far-right, lower VRAM bank; its contents
 * are backed up/restored on leaving this page. CLUT is below font ramps.
 */
extern int LekakBeforeSanctuaryInit(const Host *,Mod *);
extern void FreeDuel_Init(u8 *),FreeDuel_Entry(void);
extern void DisplayObject_RenderSpriteSheet(Object *,int,int);
extern int gFreeDuel_nPage;
extern u8 D_801AF000[];
extern int LoadImage(Rect *,u32 *),StoreImage(Rect *,u32 *),DrawSync(int);
#include "sanctuary-data.h"
static void *old_init,*old_entry,*old_draw;
static void (*old_frame)(void),(*old_reset)(void),(*old_applied)(int),(*old_shutdown)(void);
static int seen_frame,uploaded;
static u32 backup_pixels[19200],backup_palette[128];
typedef struct {s8 dx,dy;u16 cell,size;} Part;
static const struct {u8 count,flags,page,clut;Part parts[6];} sheet={
 6,0x30,0,0,{{0,0,0,0x1de0},{-128,0,16,0x1de0},{0,0,0x4400,0x1ce0},
 {0,120,15<<5,0x1de0},{-128,120,(15<<5)|16,0x1de0},{0,120,(15<<5)|0x4400,0x1ce0}}
};
_Static_assert(sizeof(Part)==6,"native sprite sheet part ABI");
static void restore_vram(void){
 Rect pixels={640,256,160,240},palette={640,240,256,1};
 if(!uploaded)return;
 DrawSync(0);LoadImage(&pixels,backup_pixels);LoadImage(&palette,backup_palette);DrawSync(0);uploaded=0;
}
static int upload(void){
 Rect pixels={640,256,160,240},palette={640,240,256,1};
 if(uploaded)return 1;
 DrawSync(0);
 if(StoreImage(&pixels,backup_pixels)<0 || StoreImage(&palette,backup_palette)<0)return 0;
 DrawSync(0);
 LoadImage(&palette,(u32 *)background_asset);LoadImage(&pixels,(u32 *)(background_asset+128));DrawSync(0);
 uploaded=1;return 1;
}
static void draw(Object *o,int ot,int depth){
 Object replacement;int i;
 int match=seen_frame && *(u8 *G32 *)(o->bytes+0x54)==D_801AF000
  && o->bytes[0x69]==1 && *(u16 *)(o->bytes+0x66)==16;
 if(match && gFreeDuel_nPage!=1)restore_vram();
 if(!match || gFreeDuel_nPage!=1 || !upload()){
  ((void (*)(Object *,int,int))old_draw)(o,ot,depth);return;
 }
 for(i=0;i<0x70;i++)replacement.bytes[i]=o->bytes[i];
 *(u32 *)(replacement.bytes+4)=0x09000000;
 *(u16 *)(replacement.bytes)|=8;
 *(u16 *)(replacement.bytes)&=(u16)~0x24;
 *(u32 *)(replacement.bytes+0x30)=0;
 *(u32 *)(replacement.bytes+0x40)=640u|(240u<<16);
 *(u32 *)(replacement.bytes+0x44)=0x10001000;
 *(u32 *)(replacement.bytes+0x48)=0;
 *(void *G32 *)(replacement.bytes+0x4c)=(void *)&sheet;
 *(u16 *)(replacement.bytes+0x5e)=0;
 *(u16 *)(replacement.bytes+0x66)=0x9a;
 ((void (*)(Object *,int,int))old_draw)(&replacement,ot,depth);
}
static void init(u8 *p){restore_vram();seen_frame=1;((void (*)(u8 *))old_init)(p);}
static void entry(void){seen_frame=1;((void (*)(void))old_entry)();}
static void frame(void){if(!seen_frame)restore_vram();seen_frame=0;if(old_frame)old_frame();}
static void reset(void){uploaded=0;seen_frame=0;if(old_reset)old_reset();}
static void applied(int on){if(!on){restore_vram();seen_frame=0;}if(old_applied)old_applied(on);}
static void shutdown(void){restore_vram();if(old_shutdown)old_shutdown();}
int MemoriesModInit(const Host *h,Mod *m){
 if(h->api<5 || !h->hook)return 0;
 if(!LekakBeforeSanctuaryInit(h,m))return 0;
 if(!h->hook(h,(void *)FreeDuel_Init,(void *)init,&old_init)
 || !h->hook(h,(void *)FreeDuel_Entry,(void *)entry,&old_entry)
 || !h->hook(h,(void *)DisplayObject_RenderSpriteSheet,(void *)draw,&old_draw))return 0;
 old_frame=m->frame;old_reset=m->reset;old_applied=m->applied;old_shutdown=m->shutdown;
 m->frame=frame;m->reset=reset;m->applied=applied;m->shutdown=shutdown;return 1;
}
