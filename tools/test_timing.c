#include "../App/display_timing.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned cpu(LekakTimerRegisters *t,unsigned n){unsigned irq=0;while(n--)irq|=LekakTimer_CpuCycle(t);return irq;}
int main(void){
 LekakTimerRegisters t={0};uint32_t v;
 assert(LekakTimer_Write(&t,0x1f801108,4,3));
 assert(LekakTimer_Write(&t,0x1f801104,2,0x18));
 assert(cpu(&t,4)==0&&t.count[0]==2);
 assert(cpu(&t,1)==0x10&&t.count[0]==3&&t.irq_events[0]==1);
 assert(LekakTimer_Read(&t,0xbf801104,4,&v)&&(v&0x800)&&!(t.mode[0]&0x800));
 cpu(&t,40);assert(t.irq_events[0]==1&&t.count[0]<=3);
 assert(LekakTimer_Write(&t,0x1f801104,4,0x20));
 assert(LekakTimer_Write(&t,0x1f801100,2,0xfffe));
 assert(cpu(&t,3)==0x10&&t.count[0]==0xffff&&(t.mode[0]&0x1000));
 cpu(&t,1);assert(t.count[0]==0);
 memset(&t,0,sizeof(t));
 LekakTimer_Write(&t,0x1f801108,4,2);LekakTimer_Write(&t,0x1f801104,4,0xd8);
 assert(cpu(&t,4)==0x10&&!(t.mode[0]&0x400));
 assert(cpu(&t,4)==0&&(t.mode[0]&0x400));
 assert(cpu(&t,4)==0x10&&t.irq_events[0]==2);
 memset(&t,0,sizeof(t));LekakTimer_Write(&t,0x1f801124,4,0x200);
 cpu(&t,16);assert(t.count[2]==0);cpu(&t,8);assert(t.count[2]==1);
 LekakTimer_Write(&t,0x1f801124,4,0x201);cpu(&t,80);assert(t.count[2]==0);
 /* Sync 0 pauses while blank; sync 3 releases only on a rising edge. */
 memset(&t,0,sizeof(t));LekakTimer_Write(&t,0x1f801104,4,1);
 LekakTimer_Blank(&t,0,1);cpu(&t,10);assert(t.count[0]==0);
 LekakTimer_Blank(&t,0,0);cpu(&t,5);assert(t.count[0]==3);
 LekakTimer_Write(&t,0x1f801104,4,7);cpu(&t,8);assert(t.count[0]==0);
 LekakTimer_Blank(&t,0,1);cpu(&t,5);assert(t.count[0]==3);
 /* HBlank source does not advance on CPU clocks. */
 LekakTimer_Write(&t,0x1f801114,4,0x100);cpu(&t,100);assert(t.count[1]==0);
 for(unsigned i=0;i<5;i++)LekakTimer_Tick(&t,1);
 assert(t.count[1]==3);
 LekakGpuIo g;LekakGpuIo_Init(&g);LekakDisplayTiming d={0};
 memset(&t,0,sizeof(t));d.hblank=d.vblank=1;t.blank[0]=t.blank[1]=1;
 t.mode[1]=0x100;unsigned irq=0;
 while(!d.frames)irq|=LekakDisplay_Cycle(&d,&t,&g);
 assert(d.line==0&&d.hblank_edges==263&&t.count[1]==263);
 assert(d.vblank_edges==1&&(irq&1)); /* One rising edge per field, no boot edge. */
 assert(d.cycles>560000&&d.cycles<570000);
 LekakGpuIo_Write(&g,0x1f801814,0x08000008);memset(&d,0,sizeof(d));memset(&t,0,sizeof(t));
 d.hblank=d.vblank=1;t.blank[0]=t.blank[1]=1;
 while(!d.frames)LekakDisplay_Cycle(&d,&t,&g);
 assert(d.hblank_edges==314&&d.cycles>670000&&d.cycles<680000);
 puts("Functional timing: target/overflow flags, IRQ one-shot/toggle, divider, blank sync, live counts and PAL/NTSC fields passed");return 0;
}
