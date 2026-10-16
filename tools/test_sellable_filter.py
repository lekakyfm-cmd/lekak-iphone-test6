"""Compile actual collection functions with controlled inventory and test sales."""
from pathlib import Path
import subprocess,tempfile,sys,json
r=Path(__file__).resolve().parents[1]
p=Path(sys.argv[1]) if len(sys.argv)>1 else r/'SourceMod/Lekak/Collection/collection.c'
s=p.read_text()
parts=[s[s.index('static unsigned code('):s.index('static unsigned buy_price(')],s[s.index('static void rebuild('):s.index('static void load_names(')],s[s.index('static void message('):s.index('static void begin_sale(')]]
prefix=r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define MAX_CARDS 6201
static unsigned char data[0x400],stock[MAX_CARDS];
static unsigned char *save=data;
static unsigned currency,*stars=&currency,serial,opened_code;
static int cursor,result_count,quantity,confirm,open,editing,rarity_test,english,gCard_nCount=6200;
static int results[MAX_CARDS];
static char names[MAX_CARDS][128],query[40],notice[128];
static unsigned char *Cards_ChestSlot(unsigned char *s,int id){(void)s;return stock+id;}
static int Cards_Valid(int id){return id>0&&id<MAX_CARDS;}
static unsigned Tables_StarchipCap(void){return 999999;}
static unsigned sale_price(int id){(void)id;return 50;}
'''
suffix=r'''
int main(void){
 stock[1]=3;stock[2]=4;stock[3]=1;stock[1241]=4;stock[2481]=3;stock[3721]=5;stock[4961]=4;
 unsigned short deck[]={3,3,3,4};memcpy(save,deck,sizeof(deck));
 rebuild();int expected[]={2,3,1241,3721,4961};assert(result_count==5);assert(!memcmp(results,expected,sizeof(expected)));
 strcpy(names[1241],"Dragon Rare");strcpy(names[2481],"Dragon Epic");strcpy(query,"dragon");rebuild();assert(result_count==1&&results[0]==1241);
 query[0]=0;rebuild();cursor=0;quantity=1;opened_code=code();sell();assert(stock[2]==3&&currency==50);assert(result_count==4&&results[0]==3);
 for(int i=1;i<MAX_CARDS;i++)stock[i]=0;rebuild();assert(result_count==0&&cursor==0);sell();assert(currency==50);
 puts("PASS: sellable-only list, protected reserve, deck, variants, search, sale removal, empty list");
}
'''
with tempfile.TemporaryDirectory() as d:
 c=Path(d)/'filter.c';exe=Path(d)/'filter-test';c.write_text(prefix+'\n'.join(parts)+suffix)
 subprocess.run(['cc','-std=c11','-O2',str(c),'-o',str(exe)],check=True);subprocess.run([str(exe)],check=True)
