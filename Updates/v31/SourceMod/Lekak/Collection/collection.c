#include "pc/mods/modapi.h"
#include "pc/cards/cards.h"
#include "pc/cards/tables.h"
#include "pc/cards/art.h"
#include "pc/platform/title_screen.h"
#include "overlays/main_menu/frontend.h"
#include "game/display_object.h"
#include "game/input.h"
#include "game/duel_get_base_card_stat.h"
#include <string.h>
#include <stdio.h>
#include "art-assets.h"
#define MAX_CARDS CARD_TABLE_ID_END
#define NAME_SIZE 128
#define QUERY_SIZE 40
static const MemoriesModHost *host;
#include "ios_canvas.h"
#ifdef MEMORIES_IOS
#include "ios_text.h"
#endif
static unsigned char *save;
static unsigned *stars;
static int (*old_title)(void);
static int open,editing,confirm,cursor,keycursor,quantity,all_cards,english;
static int results[MAX_CARDS],result_count;
static char names[MAX_CARDS][NAME_SIZE],query[QUERY_SIZE],notice[128];
static unsigned serial=1,opened_code;
static int preview_id=-1;
static int rarity_test,rarity_level;
static const char alphabet[]="ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 -";
static unsigned code(void){unsigned n;memcpy(&n,save+0x334,4);return n;}
static int lower(int c){return c>='A'&&c<='Z'?c+32:c;}
static int matches(const char *name,const char *needle){
 if(!*needle)return 1;
 for(;*name;name++){
  const char *a=name,*b=needle;
  while(*a&&*b&&lower((unsigned char)*a)==lower((unsigned char)*b)){a++;b++;}
  if(!*b)return 1;
 }
 return 0;
}
static int in_deck(int id){
 int count=0;for(int i=0;i<40;i++){unsigned short card;memcpy(&card,save+2*i,2);if(card==id)count++;}return count;
}
static unsigned owned(int id){return *Cards_ChestSlot(save,id)+in_deck(id);}
/* Kept deck slots never use more than three copies of a card. Retaining
 * three in the combined deck + trunk makes every currently usable slot
 * remain usable without accessing or rewriting its private draft. */
static unsigned sellable(int id){
 unsigned trunk=*Cards_ChestSlot(save,id),total=owned(id);
 unsigned spare=total>3?total-3:0;return spare<trunk?spare:trunk;
}
static unsigned buy_price(int id){
 int (*base)(int)=host->find(host,"lekak-rarities-test:resale-base");
 int original=base?base(id):id;
 unsigned p=500;int attack=Duel_GetBaseCardStat(original,0);
 for(int ceiling=1500;attack>ceiling;ceiling+=500){if(p>999999U/2)return 999999;p*=2;}
 return p;
}
static int card_rarity(int id){
 int (*fn)(int)=host->find(host,"lekak-rarities-test:rarity");return fn?fn(id):0;
}
static unsigned card_colour(int id){
 unsigned (*fn)(int)=host->find(host,"lekak-rarities-test:frame-color");return fn?fn(id):0;
}
static unsigned sale_price(int id){
 int (*fn)(int)=host->find(host,"lekak-rarities-test:resale-multiplier");
 unsigned multiplier=fn?(unsigned)fn(id):1;
 return (buy_price(id)/10)*multiplier;
}
static void rebuild(void){
 int old=cursor<result_count?results[cursor]:0;result_count=0;
 int count=gCard_nCount;if(count>=MAX_CARDS)count=MAX_CARDS-1;
 for(int id=1;id<=count;id++)if(Cards_Valid(id)&&(all_cards||owned(id))&&matches(names[id],query))results[result_count++]=id;
 cursor=0;for(int i=0;i<result_count;i++)if(results[i]==old){cursor=i;break;}
 serial++;
}
static void load_names(void){
 memset(names,0,sizeof(names));int count=gCard_nCount;if(count>=MAX_CARDS)count=MAX_CARDS-1;
 for(int id=1;id<=count;id++)if(Cards_Valid(id)){
  static const char *const tiers[5]={"","Rare","Epique","Legendaire","Mythique"};
  int tier=card_rarity(id);char original[NAME_SIZE];Cards_NameUtf8(id,original,sizeof(original));
  if(tier>0&&tier<=4)snprintf(names[id],NAME_SIZE,"%.100s - %s",original,tiers[tier]);
  else snprintf(names[id],NAME_SIZE,"%s",original);
 }
 rebuild();
}
static int stable(void){
 if(gMain_bMenuID<5||gMain_bMenuID>10||D_80184599||D_80184598||D_8018459A||D_8018459B||D_8018459C||D_8018459D)return 0;
 int visible=0;for(int i=5;i<11;i++){DisplayObject *p=(DisplayObject *)gMain_apMenuEntries[i];if(p&&(p->flags&DISPLAY_OBJECT_FLAG_RENDERABLE))visible++;}
 return visible>=3;
}
static void message(const char *fr,const char *en){snprintf(notice,sizeof(notice),"%s",english?en:fr);serial++;}
static void close_panel(void){open=editing=confirm=rarity_test=0;notice[0]=0;serial++;}
static void sell(void){
 if(cursor<0||cursor>=result_count){confirm=0;return;}
 int id=results[cursor];unsigned cap=Tables_StarchipCap();
 if(code()!=opened_code){close_panel();return;}
 if(!Cards_Valid(id)||quantity<1||(unsigned)quantity>sellable(id)){confirm=0;message("Stock modifie : vente annulee.","Stock changed: sale cancelled.");return;}
 unsigned price=sale_price(id);
 if(*stars>cap||price>(cap-*stars)/(unsigned)quantity){message("Limite d'etoiles : reduis la quantite.","Star-chip limit: reduce quantity.");return;}
 unsigned gain=price*(unsigned)quantity;
 *Cards_ChestSlot(save,id)-=(unsigned char)quantity;*stars+=gain;confirm=0;
 snprintf(notice,sizeof(notice),english?"Sold %d copies: +%u stars. Save your game.":"%d vendues : +%u etoiles. Sauvegarde ta partie.",quantity,gain);
 rebuild();
}
static void begin_sale(void){
 if(!result_count)return;int id=results[cursor];
 if(!sellable(id)){message("Reserve de 3 exemplaires protegee.","Three-copy reserve is protected.");return;}
 quantity=1;confirm=1;notice[0]=0;serial++;
}
static int title(void){
 unsigned p=gInput_wPad1Pressed,n=p|gInput_wPad1Repeat;
 if(!open){
  if((p&PAD_BUTTON_SELECT)&&stable()){
   open=1;editing=confirm=cursor=keycursor=0;query[0]=notice[0]=0;opened_code=code();preview_id=-1;load_names();
  }else return old_title();
 }else if(code()!=opened_code)close_panel();
 else if(rarity_test){
  if(p&(PAD_BUTTON_CIRCLE|PAD_BUTTON_START|PAD_BUTTON_SELECT)){rarity_test=0;serial++;}
  else if(n&(PAD_DIRECTION_LEFT|PAD_BUTTON_L1)){rarity_level=(rarity_level+4)%5;serial++;}
  else if(n&(PAD_DIRECTION_RIGHT|PAD_BUTTON_R1)){rarity_level=(rarity_level+1)%5;serial++;}
 }else if(confirm){
  if(p&PAD_BUTTON_CIRCLE){confirm=0;serial++;}
  else if(n&PAD_DIRECTION_LEFT){if(quantity>1)quantity--;serial++;}
  else if(n&PAD_DIRECTION_RIGHT){if((unsigned)quantity<sellable(results[cursor]))quantity++;serial++;}
  else if(p&PAD_BUTTON_L2){quantity=1;serial++;}
  else if(p&PAD_BUTTON_R2){quantity=(int)sellable(results[cursor]);serial++;}
  else if(p&PAD_BUTTON_CROSS)sell();
 }else if(editing){
  int keys=(int)sizeof(alphabet)-1;
  if(p&(PAD_BUTTON_CIRCLE|PAD_BUTTON_START)){editing=0;rebuild();}
  else if(p&PAD_BUTTON_SQUARE){size_t len=strlen(query);if(len)query[len-1]=0;rebuild();}
  else if(p&PAD_BUTTON_TRIANGLE){query[0]=0;rebuild();}
  else if(p&PAD_BUTTON_CROSS){size_t len=strlen(query);if(len<QUERY_SIZE-1){query[len]=alphabet[keycursor];query[len+1]=0;rebuild();}}
  else if(n&PAD_DIRECTION_LEFT){keycursor=(keycursor+keys-1)%keys;serial++;}
  else if(n&PAD_DIRECTION_RIGHT){keycursor=(keycursor+1)%keys;serial++;}
  else if(n&PAD_DIRECTION_UP){keycursor=(keycursor+keys-10)%keys;serial++;}
  else if(n&PAD_DIRECTION_DOWN){keycursor=(keycursor+10)%keys;serial++;}
 }else{
  if(p&(PAD_BUTTON_CIRCLE|PAD_BUTTON_SELECT))close_panel();
  else if(p&PAD_BUTTON_START){rarity_test=1;rarity_level=0;preview_id=-1;serial++;}
  else if(p&PAD_BUTTON_TRIANGLE){editing=1;notice[0]=0;serial++;}
  else if(p&PAD_BUTTON_SQUARE){query[0]=0;rebuild();}
  else if(p&PAD_BUTTON_R1){all_cards=!all_cards;rebuild();}
  else if(p&PAD_BUTTON_L1){english=!english;host->set_setting(host,"language",english);serial++;}
  else if(p&PAD_BUTTON_CROSS)begin_sale();
  else if(result_count&&(n&(PAD_DIRECTION_UP|PAD_BUTTON_L2))){int jump=n&PAD_BUTTON_L2?6:1;cursor=(cursor+result_count-jump%result_count)%result_count;notice[0]=0;serial++;}
  else if(result_count&&(n&(PAD_DIRECTION_DOWN|PAD_BUTTON_R2))){cursor=(cursor+(n&PAD_BUTTON_R2?6:1))%result_count;notice[0]=0;serial++;}
 }
 gInput_wPad1Pressed=gInput_wPad1Repeat=gInput_wPad1Held=0;return -1;
}
static int right_edge;
static unsigned char preview_record[7*2048];
static int preview_ready;
static void load_preview(int id){
 if(preview_id==id)return;
 preview_id=id;preview_ready=0;
 memset(preview_record,0,sizeof(preview_record));
 int base=Cards_BaseId(id);
 int lba=host->disc_file_start(host,"\\DATA\\WA_MRG.MRG;1");
 if(base<1||lba<0||host->disc_read(host,lba+722+(base-1)*7,7,preview_record)<=0)return;
 Cards_PatchArtRecord(id,preview_record);
 preview_ready=1;
}

static int art_width(const char *v,int scale,int title_font){
#ifdef MEMORIES_IOS
 return collection_text_width(v,scale,title_font);
#else
 int width=0;const ArtGlyph *glyphs=title_font?title_glyphs:body_glyphs;
 for(;*v;v++){unsigned c=(unsigned char)*v;if(c<32||c>126)c='?';width+=glyphs[c-32].advance*scale;}return width;
#endif
}
static void art_text(int x,int y,const char *v,unsigned rgb,int scale,int title_font){
#ifdef MEMORIES_IOS
 collection_text(x,y,v,rgb,scale,title_font);
#else
 const ArtGlyph *glyphs=title_font?title_glyphs:body_glyphs;const unsigned char *coverage=title_font?title_coverage:body_coverage;
 for(;*v;v++){
  unsigned c=(unsigned char)*v;if(c<32||c>126)c='?';const ArtGlyph *g=&glyphs[c-32];
  for(unsigned yy=0;yy<g->height;yy++)for(unsigned xx=0;xx<g->width;xx++){
   unsigned alpha=coverage[g->offset+yy*g->width+xx];if(!alpha)continue;
   unsigned color=rgb;
   if(title_font){unsigned t=yy*255/g->height;unsigned red=255-(t*73/255),green=224-(t*110/255),blue=154-(t*106/255);color=(red<<16)|(green<<8)|blue;}
   collection_fill(x+(int)xx*scale,y-(int)g->height*scale/2+(int)yy*scale,scale,scale,color,alpha);
  }
  x+=g->advance*scale;
 }
#endif
}
static void backdrop(int x,int y,int w,int h){
 for(int row=0;row<240;row++){
  int sy=y+row*h/240,ey=y+(row+1)*h/240,at=0;if(ey<=sy)continue;
  for(unsigned i=art_rows[row];i<art_rows[row+1];i++){
   int left=x+at*w/320;at+=art_runs[i].count;int end=x+at*w/320;
   if(end>left)collection_fill(left,sy,end-left,ey-sy,art_palette[art_runs[i].color],255);
  }
 }
}
static void line(int x,int y,const char *v,unsigned rgb,int scale){
 char text[256];snprintf(text,sizeof(text),"%s",v);int len=strlen(text),cut=0;
 while(len&&art_width(text,scale,0)>right_edge-x){text[--len]=0;cut=1;}
 if(cut&&len>=3)text[len-1]=text[len-2]=text[len-3]='.';
 #ifndef MEMORIES_IOS
 art_text(x+scale,y+scale,text,0x060408,scale,0);
#endif
 art_text(x,y,text,rgb,scale,0);
}
static void draw_preview_art(int ax,int ay,int artw,int arth,int s){
 if(preview_ready){
  for(int row=0;row<CARD_ART_HEIGHT;row++){
   int top=ay+row*arth/CARD_ART_HEIGHT,end=ay+(row+1)*arth/CARD_ART_HEIGHT;
   for(int col=0;col<CARD_ART_WIDTH;){
    unsigned index=preview_record[row*CARD_ART_WIDTH+col];int next=col+1;
    while(next<CARD_ART_WIDTH&&preview_record[row*CARD_ART_WIDTH+next]==index)next++;
    unsigned lo=preview_record[CARD_ART_CLUT+index*2],hi=preview_record[CARD_ART_CLUT+index*2+1];
    unsigned c=lo|(hi<<8),r=c&31,g=(c>>5)&31,b=(c>>10)&31;
    unsigned rgb=(((r<<3)|(r>>2))<<16)|(((g<<3)|(g>>2))<<8)|((b<<3)|(b>>2));
    int left=ax+col*artw/CARD_ART_WIDTH,right=ax+next*artw/CARD_ART_WIDTH;
    if(right>left&&end>top)collection_fill(left,top,right-left,end-top,rgb,255);
    col=next;
   }
  }
 }else line(ax+4*s,ay+arth/2,english?"Art unavailable":"Image indisponible",0xFFFFFF,s);
}
/* Render the same indexed art record as the in-game card viewer, including
 * replacements and added cards. Private buffer: no VRAM or game load changes. */
static void preview(int id,int x,int y,int width,int bottom,int s){
 load_preview(id);
 int edge=right_edge;right_edge=x+width-12*s;
 int artw=width-24*s,arth=artw*CARD_ART_HEIGHT/CARD_ART_WIDTH;
#ifdef MEMORIES_IOS
 int maxart=bottom-y-144*s;
#else
 int maxart=bottom-y-120*s;
#endif
 if(arth>maxart){arth=maxart;artw=arth*CARD_ART_WIDTH/CARD_ART_HEIGHT;}
 if(arth<16){right_edge=edge;return;}
 int ax=x+(width-artw)/2,ay=y+48*s;
 unsigned tint=card_colour(id);
 collection_fill(x-2*s,y-2*s,width+4*s,arth+132*s,tint?tint:0xBCA075,255);
 collection_fill(x,y,width,arth+128*s,tint?((tint>>2)&0x3F3F3F):0x17121F,255);
 line(x+12*s,y+20*s,names[id],0xFFFFFF,s);
 collection_fill(ax-2*s,ay-2*s,artw+4*s,arth+4*s,tint?tint:0xD9C4A0,255);
 collection_fill(ax,ay,artw,arth,0x08080C,255);
 draw_preview_art(ax,ay,artw,arth,s);
 char text[100];
#ifdef MEMORIES_IOS
 snprintf(text,sizeof(text),"ATK %d",Duel_GetBaseCardStat(id,0));
 line(x+12*s,ay+arth+19*s,text,0xFFFFFF,s);
 snprintf(text,sizeof(text),"DEF %d",Duel_GetBaseCardStat(id,1));
 line(x+12*s,ay+arth+43*s,text,0xFFFFFF,s);
 snprintf(text,sizeof(text),"%u *",sale_price(id));
 line(x+12*s,ay+arth+67*s,text,0xFFE1A0,s);
#else
 snprintf(text,sizeof(text),"#%d  |  ATK %d / DEF %d",id,Duel_GetBaseCardStat(id,0),Duel_GetBaseCardStat(id,1));
 line(x+12*s,ay+arth+22*s,text,0xFFFFFF,s);
 snprintf(text,sizeof(text),english?"%u stars each":"%u etoiles / exemplaire",sale_price(id));
 line(x+12*s,ay+arth+44*s,text,0xFFE1A0,s);
#endif
 right_edge=edge;
}
static void help_line(int x,int y,const char *text,int width,int s){
 int scale=s+1;
#ifdef MEMORIES_IOS
 while(scale>1&&art_width(text,scale,0)>width)scale--;
 art_text(x,y,text,0xFFFFFF,scale,0);
#else
 while(scale>1&&host->text_width(host,text,scale)>width)scale--;
 host->draw_text(host,x,y,text,0xFFFFFF,scale);
#endif
}
/* Isolated visual preview: no inventory or save writes. Card 1 is the
 * retail Blue-Eyes White Dragon; all five treatments use its current art. */
static int card_x,card_y,card_w,card_h;
static void card_rect(int x,int y,int w,int h,unsigned c,unsigned a){
 int l=card_x+x*card_w/160,t=card_y+y*card_h/218;
 int r=card_x+(x+w)*card_w/160,b=card_y+(y+h)*card_h/218;
 if(r>l&&b>t)collection_fill(l,t,r-l,b-t,c,a);
}
static unsigned prism(unsigned t){
 static const unsigned colors[]={0x59DAFF,0xA878FF,0xFF70CC,0xFFD67D,0x7DFFE4,0x59DAFF};
 t%=500;unsigned k=t/100,f=t%100,c=0;
 for(int sh=0;sh<=16;sh+=8){unsigned a=(colors[k]>>sh)&255,b=(colors[k+1]>>sh)&255;c|=((a*(100-f)+b*f)/100)<<sh;}return c;
}
static void centered(int middle,const char *text,unsigned color,int maxscale,int left,int width){
 int scale=maxscale;
#ifdef MEMORIES_IOS
 while(scale>1&&art_width(text,scale,0)>width-12)scale--;
 art_text(left+(width-art_width(text,scale,0))/2,middle,text,color,scale,0);
#else
 while(scale>1&&host->text_width(host,text,scale)>width-12)scale--;
 host->draw_text(host,left+(width-host->text_width(host,text,scale))/2,middle,text,color,scale);
#endif
}
static void rarity_overlay(int w,int h,int s){
 static const char *fr[]={"NORMALE","RARE","EPIQUE","LEGENDAIRE","MYTHIQUE"};
 static const char *en[]={"NORMAL","RARE","EPIC","LEGENDARY","MYTHIC"};
 static const unsigned colors[]={0xA3A4AD,0x00C853,0xE53935,0x00D9ED,0x8E44D4};
 unsigned tick=host->now_us?(unsigned)(host->now_us(host)/50000):0;
 unsigned color=colors[rarity_level];
 collection_fill(0,0,w,h,0x100D18,255);right_edge=w-12;
 centered(28*s,english?"BLUE-EYES WHITE DRAGON - RARITY PREVIEW":"DRAGON BLANC - APERCU DES RARETES",0xF5EBDD,s+1,0,w);
 card_h=h-180*s;if(card_h<218)card_h=218;
 card_w=card_h*160/218;
 if(card_w>w*2/3){card_w=w*2/3;card_h=card_w*218/160;}
 card_x=(w-card_w)/2;card_y=65*s;
 if(rarity_level)for(int pad=12;pad>=2;pad-=2){
  collection_fill(card_x-pad,card_y-pad,card_w+pad*2,2,color,36);
  collection_fill(card_x-pad,card_y+card_h+pad-2,card_w+pad*2,2,color,36);
  collection_fill(card_x-pad,card_y-pad,2,card_h+pad*2,color,36);
  collection_fill(card_x+card_w+pad-2,card_y-pad,2,card_h+pad*2,color,36);
 }
 card_rect(0,0,160,218,color,255);card_rect(3,3,154,212,0x76612B,255);
 for(int row=5;row<213;row++){
  unsigned shade=(unsigned)(row<109?row:218-row)/5;unsigned c=((190+shade)<<16)|((152+shade)<<8)|66;
  card_rect(5,row,150,1,c,255);
 }
 card_rect(10,9,140,18,0xF2D282,255);
 centered(card_y+18*card_h/218,"BLUE-EYES WHITE DRAGON",0x211A0D,s+1,card_x+10*card_w/160,140*card_w/160);
 for(int i=0;i<8;i++){
  int x=46+i*12;
  static const unsigned char disk[]={60,126,255,255,255,255,126,60};
  static const unsigned char star[]={16,16,56,254,124,56,108,198};
  for(int yy=0;yy<8;yy++)for(int xx=0;xx<8;xx++)if(disk[yy]&(1U<<xx))card_rect(x+xx,32+yy,1,1,(star[yy]&(1U<<xx))?0xFFE15B:0xBB461B,255);
 }
 card_rect(10,44,140,132,0x433B42,255);card_rect(12,46,136,128,0xA69DA6,255);
 load_preview(1);
 int ax=card_x+13*card_w/160,ay=card_y+47*card_h/218;
 int aw=134*card_w/160,ah=126*card_h/218;
 draw_preview_art(ax,ay,aw,ah,s);
 /* Thin moving sheen; clipped strictly to the artwork. */
 if(rarity_level>=2){
  int sweep=(int)((tick*4)%(unsigned)(aw+ah+80))-ah-40;
  for(int row=0;row<ah;row+=2){
   int left=sweep+row/2;
   for(int band=0;band<3;band++){
    int l=left+band*8,r=l+8;if(l<0)l=0;if(r>aw)r=aw;
    if(r>l)collection_fill(ax+l,ay+row,r-l,row+2>ah?ah-row:2,rarity_level==4?0xBC8CFF:0xFFFFFF,band==1?36:14);
   }
  }
 }
 if(rarity_level==4){
  for(int row=0;row<218;row+=2){unsigned c=0x8E44D4;card_rect(0,row,3,2,c,255);card_rect(157,row,3,2,c,255);}
  for(int col=0;col<160;col+=2){unsigned c=0x8E44D4;card_rect(col,0,2,3,c,255);card_rect(col,215,2,3,c,255);}
 }
 card_rect(10,180,140,28,0xEFDAA0,255);
 centered(card_y+188*card_h/218,"ATK 3000   DEF 2500",0x241B13,s+1,card_x,card_w);
 centered(card_y+202*card_h/218,english?en[rarity_level]:fr[rarity_level],0x38263F,s+1,card_x,card_w);
 if(rarity_level){
  for(int d=-3;d<=3;d++){int span=3-(d<0?-d:d);card_rect(18-span,34+d,span*2+1,1,color,255);}
 }
 if(rarity_level>=3)for(int i=0;i<10;i++){
  int xx=card_x+((i*71+13)%card_w),yy=card_y+((i*113+7)%card_h);
  if(xx>card_x+card_w/8&&xx<card_x+card_w*7/8)xx=(i&1)?card_x-5:card_x+card_w+5;
  unsigned phase=(tick+i*11)%40,alpha=phase<20?phase*11:(40-phase)*11;
  unsigned c=rarity_level==4?0xBC8CFF:0xFFF1BB;
  collection_fill(xx-3,yy,7,1,c,alpha);collection_fill(xx,yy-3,1,7,c,alpha);
 }
 int fy=h-67*s;collection_fill(10,fy-18*s,w-20,74*s,0x08080D,255);
 centered(fy,english?"LEFT / RIGHT or L1 / R1: rarity":"GAUCHE / DROITE ou L1 / R1 : rarete",0xFFFFFF,s+1,0,w);
 centered(fy+25*s,english?"O / START: back | Visual test only":"O / START : retour | Test visuel uniquement",0xDAD0E5,s,0,w);
}

static void overlay(void){
 int w,h,s;collection_size(&w,&h,&s);s=w/900;if(s>h/650)s=h/650;if(s<1)s=1;
 if(!open){if(stable()){
  right_edge=w-24;
#ifdef MEMORIES_IOS
  collection_fill(12,h-45,w-24,39,0x09090F,245);
  line(24,h-25,english?"SELECT: collection / sell cards":"SELECT : collection / revente",0xFFFFFF,s);
#else
  line(14,h-18,english?"Select: search / sell cards":"Select : rechercher / revendre des cartes",0xEEE4FF,s);
#endif
 }return;}
 if(rarity_test){rarity_overlay(w,h,s);return;}
 int pw=w,ph=h;
 int x=0,y=0;right_edge=x+pw-pw/9;int inset=pw/9;char b[256];
 backdrop(x,y,pw,ph);
 int header=ph/7;
 int full_edge=right_edge,footer=ph-ph/10-108*s;
 int preview_x=pw*61/100,preview_w=full_edge-preview_x;
 if(!editing&&result_count){
  preview(results[cursor],preview_x,header+106*s,preview_w,footer-8*s,s);
  right_edge=preview_x-22*s;
 }
 right_edge=full_edge;
 int title_x=x+(pw-art_width("COLLECTION",s,1))/2;art_text(title_x,y+header-12*s,"COLLECTION",0xFFDD99,s,1);
 const char *subtitle=english?"SEARCH & RESALE":"RECHERCHE ET REVENTE";
 line(x+(pw-art_width(subtitle,s,0))/2,y+header+28*s,subtitle,0xC9AADF,s);
 #ifdef MEMORIES_IOS
 snprintf(b,sizeof(b),english?"%d cards | %u stars | %s":"%d cartes | %u etoiles | %s",result_count,*stars,*query?query:"-");
#else
 snprintf(b,sizeof(b),english?"Filter: %s | %d cards | %u stars":"Recherche : %s | %d cartes | %u etoiles",*query?query:"-",result_count,*stars);
#endif
 line(x+inset,y+header+55*s,b,0xC8B6E2,s);
 line(x+inset,y+header+83*s,all_cards?(english?"All cards | 3 copies kept":"Toutes | 3 exemplaires gardes"):(english?"Owned | 3 copies kept":"Possedees | 3 exemplaires gardes"),0xE8D7C1,s);
 if(!editing&&result_count)right_edge=preview_x-22*s;
 if(confirm){
  int id=results[cursor];line(x+inset,y+header+120*s,names[id],0xD5D9C0,s);
  snprintf(b,sizeof(b),english?"Sell: %d / %u":"Vente : %d / %u",quantity,sellable(id));line(x+inset,y+header+155*s,b,0xEEE4FF,s);
#ifdef MEMORIES_IOS
  snprintf(b,sizeof(b),english?"Each: %u stars":"Unite : %u etoiles",sale_price(id));line(x+inset,y+header+190*s,b,0xFFFFFF,s);
  snprintf(b,sizeof(b),english?"TOTAL: %u stars":"TOTAL : %u etoiles",sale_price(id)*(unsigned)quantity);line(x+inset,y+header+225*s,b,0xFFE1A0,s);
  snprintf(b,sizeof(b),english?"Remaining: %u (deck incl.)":"Reste : %u (deck inclus)",owned(id)-(unsigned)quantity);line(x+inset,y+header+260*s,b,0xFFFFFF,s);
#else
  snprintf(b,sizeof(b),english?"Each: %u | Total: %u":"Unite : %u | Total : %u",sale_price(id),sale_price(id)*(unsigned)quantity);line(x+inset,y+header+190*s,b,0xE8D7C1,s);
  snprintf(b,sizeof(b),english?"Remaining: %u (deck incl.)":"Reste : %u (deck inclus)",owned(id)-(unsigned)quantity);line(x+inset,y+header+225*s,b,0xEEE4FF,s);
#endif
 }else if(editing){
  line(x+inset,y+header+115*s,english?"ARROWS: choose a letter":"FLECHES : choisir une lettre",0xEEE4FF,s);
  for(int i=0;i<(int)sizeof(alphabet)-1;i++){
   int xx=x+inset+(i%10)*36*s,yy=y+header+140*s+(i/10)*(ph<650?28:36)*s;
   if(i==keycursor)collection_fill(xx-4*s,yy-12*s,26*s,26*s,0x685083,255);
   char key[2]={alphabet[i],0};line(xx,yy,key,0xEEE4FF,s);
  }
 }else{
  int visible=(footer-header-120*s)/(48*s);if(visible<1)visible=1;if(visible>6)visible=6;
  int start=(cursor/visible)*visible;
  if(!result_count)line(x+inset,y+header+120*s,english?"No matching cards.":"Aucune carte correspondante.",0xEEE4FF,s);
  for(int k=0;k<visible&&start+k<result_count;k++){
   int index=start+k,id=results[index],yy=y+header+120*s+k*48*s;
   if(index==cursor)collection_fill(x+inset-6*s,yy-12*s,right_edge-inset+6*s,43*s,0x413051,175);
   snprintf(b,sizeof(b),"%s #%d  %s",index==cursor?">":" ",id,names[id]);line(x+inset,yy,b,0xEEE4FF,s);
   snprintf(b,sizeof(b),english?"%u own | S%u | %u*":"%u poss | V%u | %u*",owned(id),sellable(id),sale_price(id));line(x+inset,yy+23*s,b,0xD5D9C0,s);
  }
 }
 right_edge=full_edge;
 if(notice[0])line(inset,footer-22*s,notice,0xFFE1A0,s);
 collection_fill(inset-12*s,footer-14*s,pw-2*inset+24*s,108*s,0x09090F,245);
 collection_fill(inset-12*s,footer-14*s,pw-2*inset+24*s,2*s,0xBCA075,255);
#ifdef MEMORIES_IOS
 const char *first,*second,*third;
 if(confirm){
  first=english?"LEFT/RIGHT: quantity | L2: 1 | R2: max":"GAUCHE/DROITE : quantite | L2:1 | R2:max";
  second=english?"X: confirm sale | O: cancel":"X : confirmer | O : annuler";
  third=english?"Save your game after selling.":"Sauvegarde apres la vente.";
 }else if(editing){
  first=english?"ARROWS: move | X: add":"FLECHES : choisir | X : ajouter";
  second=english?"SQUARE: delete | TRIANGLE: clear":"CARRE : effacer | TRIANGLE : vider";
  third=english?"START / O: back to results":"START / O : retour aux resultats";
 }else{
  first=english?"UP/DOWN: choose | X: sell | O: back":"HAUT/BAS : choisir | X:vendre | O:retour";
  second=english?"TRIANGLE: search | R1: all | L1: FR":"TRIANGLE:chercher | R1:toutes | L1:EN";
  third=english?"L2/R2: page | SQUARE: clear | * = stars":"L2/R2:page | CARRE:vider | * = etoiles";
 }
 help_line(inset,footer+7*s,first,pw-2*inset,s);
 help_line(inset,footer+39*s,second,pw-2*inset,s);
 help_line(inset,footer+74*s,third,pw-2*inset,s);
#else
 const char *first,*second;
 if(confirm){
  first=english?"LEFT/RIGHT: quantity | L2: 1 | R2: maximum":"GAUCHE/DROITE : quantite | L2 : 1 | R2 : maximum";
  second=english?"X: confirm sale | O: cancel":"X : confirmer la vente | O : annuler";
 }else if(editing){
  first=english?"ARROWS: move | X: add | SQUARE: delete":"FLECHES : deplacer | X : ajouter | CARRE : effacer";
  second=english?"TRIANGLE: clear | START / O: results":"TRIANGLE : vider | START / O : resultats";
 }else{
  first=english?"UP/DOWN: select | L2/R2: page | X: sell | O: back":"HAUT/BAS : choisir | L2/R2 : page | X : vendre | O : retour";
  second=english?"TRIANGLE: search | R1: owned/all | L1: FR | SQUARE: clear":"TRIANGLE : chercher | R1 : toutes | L1 : EN | CARRE : vider";
 }
 help_line(inset,footer+7*s,first,pw-2*inset,s);
 help_line(inset,footer+39*s,second,pw-2*inset,s);
 line(inset,footer+74*s,english?"START: rarity preview | Save after selling.":"START : test raretes | Sauvegarde apres vente.",0xFFE1A0,s);
#endif
}
static unsigned signature(void){return serial+(rarity_test&&host->now_us?(unsigned)(host->now_us(host)/50000)*7919U:0);}
static void reset(void){close_panel();query[0]=0;preview_id=-1;}
static void applied(int on){(void)on;reset();}
int MemoriesModInit(const MemoriesModHost *h,MemoriesMod *m){
 if(h->api<6||!h->symbol||!h->hook||!h->overlay_size||!h->draw_text||!h->text_width||!h->fill||!h->disc_file_start||!h->disc_read)return 0;
 host=h;save=h->symbol(h,"gDuel_awPlayerDeck");stars=h->symbol(h,"gLibrary_dwStarchips");if(!save||!stars)return 0;
 if(!h->hook(h,TitleScreen_Update,title,(void **)&old_title))return 0;
 english=h->setting(h,"language",0)!=0;
 m->api=6;m->name="Lekak - Collection et revente (v1.6)";m->overlay=overlay;m->overlay_signature=signature;m->reset=reset;m->applied=applied;
 return 1;
}
