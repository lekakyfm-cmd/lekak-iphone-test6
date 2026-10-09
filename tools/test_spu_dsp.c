#include "../App/spu_config.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
static void w(LekakSpuConfig *s,unsigned a,unsigned v){assert(LekakSpu_Write(s,a,2,v)==1);}
static unsigned r(LekakSpuConfig *s,unsigned a){unsigned v=0;assert(LekakSpu_Read(s,a,2,&v)==1);return v;}
static void tick(LekakSpuConfig *s){for(unsigned i=0;i<768;i++)LekakSpu_Cycle(s);}
int main(void){
 LekakSpuConfig *s=calloc(1,sizeof(*s)),*t=calloc(1,sizeof(*t));assert(s&&t);
 /* Known ADPCM block: filter 0, shift 0, positive constant 4096. No CD or
  * replacement hooks. Full direct gains, fast attack then decay/sustain. */
 s->ram[0x1000]=0;s->ram[0x1001]=1;
 for(unsigned i=2;i<16;i++)s->ram[0x1000+i]=0x11;
 w(s,0x1f801c00,0x3fff);w(s,0x1f801c02,0x3fff);w(s,0x1f801c04,0x1000);
 w(s,0x1f801c06,0x200);w(s,0x1f801c08,0);w(s,0x1f801c0a,0);
 w(s,0x1f801d80,0x3fff);w(s,0x1f801d82,0x3fff);w(s,0x1f801d88,1);
 assert(r(s,0x1f801c0c)==0);tick(s);assert(s->dsp.frames==0);
 w(s,0x1f801daa,0xc000);tick(s);
 assert(r(s,0x1f801c0c)==14336&&r(s,0x1f801d9c)==1);
 for(unsigned i=0;i<30;i++)tick(s);
 assert(s->dsp.nonzero_frames>0&&r(s,0x1f801c0c)==0);
 assert(s->dsp.last[0]==0&&s->dsp.last[1]==0);
 /* Re-key clears ENDX, release responds to actual envelope state. */
 s->ram[0x1001]=3;w(s,0x1f801c0e,0x200);w(s,0x1f801d88,1);
 assert(r(s,0x1f801d9c)==0);tick(s);assert(r(s,0x1f801d9c)==1);
 w(s,0x1f801d8c,1);for(unsigned i=0;i<16;i++)tick(s);
 assert(r(s,0x1f801c0c)==0);
 /* Wrapped, eight-byte aligned sample start: 16-byte decode crosses RAM end.
  * Independent private engine state: activating one does not affect another. */
 t->ram[0x7fff8]=0;t->ram[0x7fff9]=3;
 for(unsigned i=2;i<16;i++)t->ram[(0x7fff8+i)&0x7ffff]=0x22;
 w(t,0x1f801c06,0xffff);w(t,0x1f801c0e,0xffff);w(t,0x1f801c04,0x1000);
 w(t,0x1f801d88,1);w(t,0x1f801daa,0xc000);tick(t);
 assert(t->dsp.frames==1&&r(t,0x1f801c0c)==14336&&r(s,0x1f801c0c)==0);
 w(t,0x1f801c00,0x3fff);w(t,0x1f801c02,0x3fff);
 w(t,0x1f801d80,0x3fff);w(t,0x1f801d82,0x3fff);
 w(t,0x1f801daa,0x8000);tick(t);
 assert(r(t,0x1f801c0c)>0&&t->dsp.last[0]==0&&t->dsp.last[1]==0);
 assert(LekakSpu_Write(t,0x1f801daa,2,0xc040)==-1); /* no fake SPU IRQ */
 assert(LekakSpu_Write(t,0x1f801d94,2,1)==-1); /* no fake noise */
 assert(LekakSpu_Write(t,0x1f801c00,2,0x8000)==-1); /* no fake sweep */
 free(s);free(t);puts("SPU ADPCM waveform, ADSR/key/release, end flags, wrapped decode and private-state tests passed");
}
