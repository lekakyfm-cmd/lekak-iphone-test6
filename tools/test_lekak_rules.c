#include "../App/lekak_rules.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct {
 unsigned win,owned;
 int unlocked,seen;
 const char *missing_win,*missing_unlock;
} Save;
static uint64_t wins(void *context,const char *id) {
 Save *s=context;
 return s->missing_win&&!strcmp(s->missing_win,id)?0:s->win;
}
static int unlocked(void *context,const char *id) {
 Save *s=context;
 return s->missing_unlock&&!strcmp(s->missing_unlock,id)?0:s->unlocked;
}
static int seen(void *context,const char *id) {(void)id;return ((Save *)context)->seen;}
static unsigned owned(void *context,const char *id) {(void)id;return ((Save *)context)->owned;}
static unsigned index_of(const char *name) {
 for(unsigned i=0;i<40;i++)if(!strcmp(LekakRules_Duelist(i),name))return i;
 abort();
}
int main(void) {
 Save empty={0},complete={1000,1,1,1,NULL,NULL};
 LekakProgressSnapshot a={&empty,0,0,wins,unlocked,seen,owned};
 LekakProgressSnapshot b={&complete,1000,1240,wins,unlocked,seen,owned};
 for(unsigned i=0;i<40;i++) {
  assert(!LekakRules_Ready(i,&a));assert(LekakRules_Ready(i,&b));
  /* Switching back to another save must not retain any unlock state. */
  assert(!LekakRules_Ready(i,&a));
 }
 assert(!LekakRules_Ready(40,&b));assert(!LekakRules_Ready(0,NULL));
 assert(!LekakRules_Duelist(40));
 unsigned rex=index_of("Lekakmod:primal-rex");a.total_wins=99;assert(!LekakRules_Ready(rex,&a));
 a.total_wins=100;assert(LekakRules_Ready(rex,&a));
 unsigned nerex=index_of("Lekakmod:nerex");b.distinct_seen=199;assert(!LekakRules_Ready(nerex,&b));
 b.distinct_seen=200;assert(LekakRules_Ready(nerex,&b));
 complete.win=9;assert(!LekakRules_Ready(nerex,&b));complete.win=10;assert(LekakRules_Ready(nerex,&b));
 complete.win=1000;
 unsigned oryx=index_of("Lekakmod:oryx");complete.seen=0;assert(!LekakRules_Ready(oryx,&b));
 complete.seen=1;assert(LekakRules_Ready(oryx,&b));
 complete.missing_unlock="Lekakmod:tharn";assert(!LekakRules_Ready(oryx,&b));complete.missing_unlock=NULL;
 unsigned velkor=index_of("Lekakmod:velkor");
 complete.missing_win="39";assert(!LekakRules_Ready(velkor,&b));
 complete.missing_win=NULL;assert(LekakRules_Ready(velkor,&b));
 for(unsigned i=37;i<40;i++) {
  complete.missing_win="Lekakmod:dark-simon";assert(!LekakRules_Ready(i,&b));
  complete.missing_win=NULL;complete.missing_unlock="Lekakmod:vezra";assert(!LekakRules_Ready(i,&b));
  complete.missing_unlock=NULL;assert(LekakRules_Ready(i,&b));
 }
 unsigned kaiba=index_of("Lekakmod:tyrant-kaiba");
 complete.missing_unlock="Lekakmod:eclipse-isis";assert(!LekakRules_Ready(kaiba,&b));
 complete.missing_unlock=NULL;assert(LekakRules_Ready(kaiba,&b));
 assert(LekakRules_Price(1500,1)==500);assert(LekakRules_Price(1501,1)==1000);
 assert(LekakRules_Price(2000,1)==1000);assert(LekakRules_Price(2001,1)==2000);
 assert(LekakRules_Price(5000,1)==64000);assert(LekakRules_Price(INT_MAX,1)==999999);
 assert(LekakRules_Price(5000,0)==500);assert(LekakRules_Price(INT_MIN,1)==500);
 assert(!LekakRules_NameRGB(1500,1));assert(!LekakRules_NameRGB(5000,0));
 assert(LekakRules_NameRGB(1501,1)==0xFFEB3C);
 assert(LekakRules_NameRGB(3999,1)==0x4687FF);
 assert(LekakRules_NameRGB(4000,1)==0x50FF1E);
 assert(LekakRules_NameRGB(4500,1)==0x50FF1E);
 assert(LekakRules_NameRGB(4501,1)==0x2DFFF5);
 assert(LekakRules_NameRGB(5000,1)==0x2DFFF5);
 puts("Lekak progression, save isolation, price and colour tests passed");
 return 0;
}
