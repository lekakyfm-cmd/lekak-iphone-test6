#include "../App/spu_config.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
static void write16(uint8_t *ram,unsigned a,int16_t v){ram[a]=(uint8_t)v;ram[a+1]=(uint8_t)((uint16_t)v>>8);}
static int16_t read16(uint8_t *ram,unsigned a){return (int16_t)((uint16_t)ram[a]|((uint16_t)ram[a+1]<<8));}
int main(void){
 uint8_t *ram=calloc(1,0x80000),*before=malloc(0x80000);assert(ram&&before);
 LekakSpuReverb s={0};s.base=s.current=0x7ff00;
 /* Known stored echo: with APF2 coefficient zero, output is its delayed tap,
  * even with master writes disabled. Signed EVOL at half gain. */
 s.registers[0]=1;s.registers[1]=1;s.registers[28]=4;s.registers[29]=8;
 s.gain[0]=0x4000;s.gain[1]=0xc000;
 write16(ram,0x7ff18,2000);write16(ram,0x7ff38,1000);
 memcpy(before,ram,0x80000);LekakSpuReverb_Step(&s,ram,0,0,0);
 assert(s.output[0]==1000&&s.output[1]==-500&&s.current==0x7ff02&&s.writes==0);
 assert(!memcmp(before,ram,0x80000));
 /* Reflection-only impulse: IIR/input half gain, no wall feedback. 10000*
  * .5*.5=2500; opposite channel uses its own input, not the left impulse. */
 memset(&s,0,sizeof(s));memset(ram,0,0x80000);s.base=s.current=0x7ff00;
 s.registers[2]=0x4000;s.registers[30]=s.registers[31]=0x4000;
 s.registers[10]=1;s.registers[11]=2;s.registers[18]=3;s.registers[19]=4;
 s.registers[26]=5;s.registers[27]=6;s.registers[28]=7;s.registers[29]=8;
 LekakSpuReverb_Step(&s,ram,10000,-10000,1);
 assert(read16(ram,0x7ff08)==2500&&read16(ram,0x7ff10)==-2500);
 assert(read16(ram,0x7ff18)==2500&&read16(ram,0x7ff20)==-2500&&s.writes==8);
 /* Circular offsets and base writes preserve unrelated sample RAM. */
 s.current=0x7fffe;s.registers[10]=0x1001;ram[0x1000]=0xa5;
 LekakSpuReverb_Step(&s,ram,32767,32767,1);
 assert(s.current==s.base&&ram[0x1000]==0xa5);
 /* Extreme signed coefficient and smallest buffer stay bounded. */
 s.base=s.current=0x7fff8;s.registers[2]=0x8000;
 for(unsigned i=0;i<32;i++)s.registers[i]=0xffff;
 for(unsigned i=0;i<128;i++)LekakSpuReverb_Step(&s,ram,-32768,32767,1);
 assert(s.current>=s.base&&s.current<0x80000);
 LekakSpuConfig *c=calloc(1,sizeof(*c));assert(c);unsigned v;
 assert(LekakSpu_Write(c,0x1f801da2,2,0xffe0)==1);
 assert(c->dsp.reverb.base==0x7ff00&&c->dsp.reverb.current==0x7ff00);
 assert(LekakSpu_Write(c,0x1f801d98,2,0x1234)==1&&LekakSpu_Write(c,0x1f801d9a,2,0xabcd)==1);
 assert(c->dsp.reverb.voice_mask==0xcd1234);
 assert(LekakSpu_Write(c,0x1f801dc4,2,0x8000)==1&&LekakSpu_Read(c,0x1f801dc4,2,&v)==1&&v==0x8000);
 assert(LekakSpu_Write(c,0x1f801daa,2,0xc081)==1);
 for(unsigned i=0;i<1536;i++)LekakSpu_Cycle(c);
 assert(c->dsp.reverb.ticks==1&&c->dsp.reverb.writes==8);
 free(c);free(before);free(ram);puts("Reverb stored-tail output, independent signed impulses, gain, write-disable and circular RAM tests passed");
}
