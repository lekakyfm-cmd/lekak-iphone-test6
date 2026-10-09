#include "pc/mods/modapi.h"
#include "game/display_object.h"
#include "game/display_object_render_sprite_sheet.h"
#include "overlays/main_menu/frontend.h"
#include "psyq/libgte.h"
#include "psyq/libgpu.h"
#include "button-images.h"
#include "pc/render/texture_pack.h"
#include <stdio.h>
extern int LekakObjectivesButtonVisible(void),LekakObjectivesButtonSelected(void);
static void *old_draw;
static const MemoriesModHost *button_host;
static u16 supplied_pixels[2][128*32];
static int uploaded,ready[2];
/* Read the original menu texture rather than inventing another border. */
static u16 vram[512][1024],atlas[32][256];
static u32 saved_pixels[4096];
static struct {u8 count,flags,page,clut;SpriteSheetPart parts[1];} sheets[2];
static int widths[2],heights[2];
void LekakButton_Restore(void){RECT r={640,256,256,32};if(!uploaded)return;DrawSync(0);LoadImage(&r,saved_pixels);DrawSync(0);uploaded=ready[0]=ready[1]=0;}
void LekakButton_Reset(void){uploaded=ready[0]=ready[1]=0;}
static int coord(int a,u16 bits,int extended){if(!extended)return a;a=(u8)a|((bits&0xc000)>>6);return a&512?a-1024:a;}
static int capture(DisplayObject *o,int state){
 SpriteSheetHeader *s=(SpriteSheetHeader *)o->field_4C;
 SpriteSheetPart *p=(SpriteSheetPart *)(s+1);
 int minx=1024,miny=1024,maxx=-1024,maxy=-1024,i,x,y;
 RECT all={0,0,1024,512},target={640,256,256,32};
 if(!s||!s->count||s->count>32)return 0;
 for(i=0;i<s->count;i++){
  int dx=coord(p[i].dx,p[i].cell,s->flags&16),dy=coord(p[i].dy,p[i].size,s->flags&16);
  int w=((p[i].size>>2)&120)+8,h=((p[i].size>>6)&120)+8;
  if(dx<minx)minx=dx;if(dy<miny)miny=dy;if(dx+w>maxx)maxx=dx+w;if(dy+h>maxy)maxy=dy+h;
 }
 int w=maxx-minx,h=maxy-miny;
 if(w<32||w>128||h<8||h>32||w%8||h%8)return 0;
 DrawSync(0);if(StoreImage(&all,(u32 *)vram)<0)return 0;DrawSync(0);
 if(!uploaded){for(y=0;y<32;y++)for(x=0;x<256;x++)((u16 *)saved_pixels)[y*256+x]=vram[y+256][x+640];}
 for(y=0;y<32;y++)for(x=0;x<128;x++)atlas[y][state*128+x]=0;
 for(i=0;i<s->count;i++){
  int dx=coord(p[i].dx,p[i].cell,s->flags&16)-minx,dy=coord(p[i].dy,p[i].size,s->flags&16)-miny;
  int pw=((p[i].size>>2)&120)+8,ph=((p[i].size>>6)&120)+8;
  int wide=!!(o->attribute&0x01000000),page=o->field_66+s->tpage*(wide?2:1);
  int cx=o->field_40.h.field_40,cy=o->field_40.h.field_42;
  int u=((p[i].cell&31)<<3)+(o->field_5E&255),v=((p[i].cell&0x3e0)>>2)+(o->field_5E>>8);
  if(o->flags&DISPLAY_OBJECT_FLAG_TEXTURE_CELL_OFFSET){cx+=(s->clut&15)*16;cy+=s->clut>>4;}
  if(s->flags&0xe0){
   if(wide){page+=(p[i].cell>>9)&14;cy+=p[i].size&31;}
   else{int c=(cx&255)+(p[i].size&15)*16;cx=(cx&0x300)|(c&255);cy+=c>>8;page+=(p[i].cell>>10)&7;}
  }
  page+=u>>8;u&=255;
  int mode=(o->attribute>>24)&3,tx=(page&15)*64,ty=(page&16)?256:0;
  if(mode>2)return 0;
  for(y=0;y<ph;y++)for(x=0;x<pw;x++){
   int px=u+(((s->flags&0xe0)&&(p[i].cell&0x2000))?pw-1-x:x),py=(ty+((v+y)&255))&511;
   u16 value;
   if(mode==2)value=vram[py][(tx+px)&1023];
   else{int bits=mode==1?8:4,per=16/bits;u16 word=vram[py][(tx+px/per)&1023];int index=(word>>((px%per)*bits))&((1<<bits)-1);value=vram[cy&511][(cx+index)&1023];}
   atlas[dy+y][state*128+dx+x]=value;
  }
 }
 /* Use the supplied complete button pictures inside the native footprint.
    Preserve transparent padding, pivots and the native sprite geometry. */
 int bx=w,by=h,ex=0,ey=0;
 for(y=0;y<h;y++)for(x=0;x<w;x++)if(atlas[y][state*128+x]){if(x<bx)bx=x;if(y<by)by=y;if(x+1>ex)ex=x+1;if(y+1>ey)ey=y+1;}
 /* Transparent padding belongs to the native sprite too. Do not turn it
    into an opaque extension of the frame. */
 if(ex-bx<24||ey-by<8)return 0;
 /* Both images are ready immediately: selecting OBJECTIVES does not need
    LIBRARY to have been drawn in its highlighted state first. */
 for(int st=0;st<2;st++){
  char path[2048];int bw=ex-bx,bh=ey-by;
  for(y=0;y<32;y++)for(x=0;x<128;x++)atlas[y][st*128+x]=0;
  for(y=0;y<bh;y++)for(x=0;x<bw;x++){
   u16 value=supplied_buttons[st][y*32/bh][x*128/bw];
   atlas[by+y][st*128+bx+x]=value;
   supplied_pixels[st][y*bw+x]=value;
  }
  snprintf(path,sizeof(path),"%s/buttons/objectives-%s.png",button_host->directory,st?"selected":"normal");
  TexturePack_AddMade(supplied_pixels[st],bw,bh,16,0,0,path,0,0,supplied_width[st],supplied_height[st]);
  sheets[st].count=1;sheets[st].flags=0;sheets[st].page=st*2;sheets[st].clut=0;
  sheets[st].parts[0].dx=minx;sheets[st].parts[0].dy=miny;sheets[st].parts[0].cell=0;sheets[st].parts[0].size=((w-8)<<2)|((h-8)<<6);
  widths[st]=w;heights[st]=h;ready[st]=1;
 }
 LoadImage(&target,(u32 *)atlas);
 /* Individual uploads identify each complete picture to the HD sampler. */
 for(int st=0;st<2;st++){
  RECT body={640+st*128+bx,256+by,ex-bx,ey-by};
  LoadImage(&body,(u32 *)supplied_pixels[st]);
 }
 DrawSync(0);uploaded=1;return 1;
}
static void draw(DisplayObject *o,s32 ot,s32 depth){
 ((void (*)(DisplayObject *,s32,s32))old_draw)(o,ot,depth);
 if((void *)o!=(void *)gMain_apMenuEntries[8]||!LekakObjectivesButtonVisible())return;
 int native_selected=o->field_69==16;
 if(!ready[native_selected]&&!capture(o,native_selected))return;
 int state=!!LekakObjectivesButtonSelected();if(!ready[state])state=native_selected;
 DisplayObject button=*o;
 /* Keep position X, scale, pivot, tint, ordering and dimensions from LIBRARY. */
 button.attribute=(button.attribute&~0x03000000)|0x02000000;
 button.flags&=(u16)~0x24;
 button.field_30.h.field_32=142;
 button.field_4C=(s32)&sheets[state];button.field_5E=0;button.field_66=0x1a;
 ((void (*)(DisplayObject *,s32,s32))old_draw)(&button,ot,depth);
}
int LekakButton_Init(const MemoriesModHost *h){button_host=h;return h->hook(h,(void *)DisplayObject_RenderSpriteSheet,(void *)draw,&old_draw);}
