"""Render the production drawing functions at the minimum iPhone game size.

The host/save inputs are fixtures; all text, preview and panel drawing is real.
Assert every glyph survives intact, and save list/sale/search/menu renders.
"""
import os,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
source=(ROOT/'SourceMod/Lekak/Collection/collection.c').read_text()
drawing=source[source.index('static int art_width('):source.index('static unsigned signature(')]
prefix=r'''
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pc/mods/modapi.h"
#include "pc/cards/art.h"
#include "art-assets.h"
static const MemoriesModHost *host;
#include "ios_canvas.h"
#include "ios_text.h"
static int open=1,editing,confirm,cursor,keycursor,quantity=1,all_cards,english,rarity_test,rarity_level;
static int results[8]={1,2,3,4,5},result_count=5,right_edge,preview_ready=1;
static char names[8][128],query[40],notice[128];
static const char alphabet[]="ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 -";
static unsigned star_count=10000,*stars=&star_count;
static unsigned char preview_record[7*2048];
static int stable(void){return 1;}
static void load_preview(int id){(void)id;}
static unsigned sale_price(int id){return id==1?123456:50;}
static unsigned owned(int id){(void)id;return 99;}
static unsigned sellable(int id){(void)id;return 96;}
static unsigned card_colour(int id){(void)id;return 0x8E44D4;}
static int Duel_GetBaseCardStat(int id,int def){(void)id;return def?2500:3000;}
'''
suffix=r'''
static int w=320,h=240;
static unsigned char pixels[960*720*3];
static void size(const MemoriesModHost *host,int *x,int *y,int *s){(void)host;*x=w;*y=h;*s=1;}
static void fill(const MemoriesModHost *host,int x,int y,int width,int height,uint32_t rgb,unsigned alpha){
 (void)host;assert(x>=0&&y>=0&&width>0&&height>0&&x+width<=w&&y+height<=h);
 for(int yy=y;yy<y+height;yy++)for(int xx=x;xx<x+width;xx++)for(int c=0;c<3;c++){
  unsigned char *p=&pixels[(yy*w+xx)*3+c];unsigned n=(rgb>>((2-c)*8))&255;*p=(*p*(255-alpha)+n*alpha)/255;
 }
}
static const MemoriesModHost mock={.overlay_size=size,.fill=fill};
static void dump(const char *folder,const char *name){
 char path[1024];snprintf(path,sizeof(path),"%s/%s-%d.ppm",folder,name,w);
 FILE *f=fopen(path,"wb");assert(f);fprintf(f,"P6\n%d %d\n255\n",w,h);
 assert(fwrite(pixels,3,w*h,f)==(size_t)(w*h));fclose(f);
}
int main(int argc,char **argv){
 assert(argc==2);host=&mock;
 for(int i=1;i<=5;i++)snprintf(names[i],128,"Dragon blanc - Mythique");
 for(int resolution=1;resolution<=3;resolution++){
  w=320*resolution;h=240*resolution;canvas_width=w;canvas_height=h;
  /* All printable glyphs, all three alignments: every intended output pixel
   * must remain solid and every empty pixel must stay empty. */
  for(unsigned c=32;c<127;c++)for(int alignment=0;alignment<3;alignment++){
   memset(pixels,0,sizeof(pixels));char text[2]={(char)c,0};
   collection_text(30+alignment,30+alignment,text,0xFFFFFF,1,0);
   const unsigned char *rows=collection_glyph(c);
   int x=(30+alignment)*w/960,y=(30+alignment)*h/720-7*resolution/2;
   for(int yy=0;yy<7*resolution;yy++)for(int xx=0;xx<6*resolution;xx++){
    unsigned expected=xx<5*resolution&&(rows[yy/resolution]&(16>>(xx/resolution)))?255:0;
    for(int k=0;k<3;k++)assert(pixels[((y+yy)*w+x+xx)*3+k]==expected);
   }
  }
  for(int lang=0;lang<2;lang++){
   english=lang;
   for(int mode=0;mode<4;mode++){
    memset(pixels,0,sizeof(pixels));open=mode!=3;editing=mode==1;confirm=mode==2;
    overlay();char label[40];snprintf(label,sizeof(label),"%s-%s",lang?"en":"fr",mode==0?"list":mode==1?"search":mode==2?"sale":"menu");dump(argv[1],label);
   }
  }
 }
 puts("95 printable glyphs intact at 3 alignments x 3 resolutions; 24 production collection renders passed");
}
'''
output=ROOT/'build/readability';output.mkdir(parents=True,exist_ok=True)
with tempfile.TemporaryDirectory(prefix='lekak-readability-') as d:
 path=Path(d);c=path/'test.c';c.write_text(prefix+drawing+suffix);binary=path/'test'
 subprocess.run([os.environ.get('CC','cc'),'-std=gnu11','-O2','-DMEMORIES_IOS','-I'+str(ROOT/'EngineSDK/src'),'-I'+str(ROOT/'SourceMod/Lekak/Collection'),str(c),'-o',str(binary)],check=True)
 subprocess.run([str(binary),str(output)],check=True)
