#include "../App/guest_exec.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
static uint32_t code[500];static unsigned count;static int delayed_start;
static void emit(uint32_t w){assert(count<500);code[count++]=w;}
static void value(uint32_t v){emit(0x3c090000|(v>>16));emit(0x35290000|(v&65535));}
static void w32(unsigned a,uint32_t v){value(v);emit(0xad090000|a);}
static void w16(unsigned a,uint16_t v){value(v);emit(0xa5090000|a);}
static void mode(uint16_t m){w16(0x1daa,m);if(delayed_start&&m==0x20)return;emit(0x312b0030);unsigned loop=count;emit(0x950a1dae);emit(0);emit(0x314a0030);emit(0x154b0000|(uint16_t)((int)loop-(int)count-1));emit(0);}
static void install(MemoriesMemory *m,uint32_t source,uint32_t control,unsigned blocks){
 count=0;emit(0x3c081f80);w32(0x10f0,0x076dc321);w32(0x10f4,0x00900000);
 w32(0x1014,0x220931e1);w16(0x1dac,4);w16(0x1da6,0xffff);
 mode(0x20);w32(0x10c0,source);w32(0x10c4,(blocks<<16)|16);w32(0x10c8,control);
 if(delayed_start){
  emit(0x3c0c0100);unsigned loop=count;emit(0x8d0a10c8);emit(0);emit(0x014c5024);
  emit(0x15400000|(uint16_t)((int)loop-(int)count-1));emit(0);
 }
 mode(0);w16(0x1da6,0xffff);mode(0x30);w32(0x10c0,0x21000);w32(0x10c4,0x20010);w32(0x10c8,0x01000200);
 emit(0x03e00008);emit(0);
 memset(m,0,sizeof(*m));for(unsigned i=0;i<count;i++)Memories_WriteLE32(m->ram+0x10000+i*4,code[i]);
 for(unsigned i=0;i<128;i++)m->ram[0x20000+i]=(uint8_t)(i*13+7);
 memset(m->ram+0x21000,0xa5,128);
}
int main(void){
 MemoriesMemory *m=calloc(1,sizeof(*m));assert(m);LekakExecResult r;
 install(m,0x20000,0x01000201,2);
 int ok=LekakExec_Run(m,0x80010000,0,0x801fff00,5000,&r);if(!ok)fprintf(stderr,"DMA result: %s pc=%08x detail=%08x\n",r.reason,r.pc,r.detail);
 assert(ok);
 assert(!memcmp(m->ram+0x20000,m->ram+0x21000,128));
 assert(r.spu_dma_transfers==2&&r.spu_dma_words==64&&(r.irq_status&8));
 assert(r.dma_interrupt&0x10000000); /* actual completion flag */
 delayed_start=1;install(m,0x20000,0x01000201,2);
 assert(LekakExec_Run(m,0x80010000,0,0x801fff00,5000,&r));
 assert(r.spu_dma_transfers==2&&!memcmp(m->ram+0x20000,m->ram+0x21000,128));
 delayed_start=0;
 install(m,0x1ffffc,0x01000201,2);
 assert(!LekakExec_Run(m,0x80010000,0,0x801fff00,3000,&r));assert(r.spu_dma_transfers==0&&r.detail==0x1ffffc);
 for(unsigned i=0;i<128;i++)assert(m->ram[0x21000+i]==0xa5);
 install(m,0x20000,0x01000601,2);
 assert(!LekakExec_Run(m,0x80010000,0,0x801fff00,3000,&r)&&strstr(r.reason,"SPU DMA control"));
 install(m,0x20000,0x01000201,65535);
 assert(!LekakExec_Run(m,0x80010000,0,0x801fff00,3000,&r)&&strstr(r.reason,"SPU DMA exceeds diagnostic budget"));
 for(unsigned i=0;i<128;i++)assert(m->ram[0x21000+i]==0xa5);
 free(m);puts("SPU DMA4 write/read roundtrip, wrap, DICR completion, full range/budget and unsupported-mode tests passed");
}
