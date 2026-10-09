#include "../App/guest_exec.h"
#include "pc/render/soft_gpu.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#define I(op,rs,rt,im) (((uint32_t)(op)<<26)|((rs)<<21)|((rt)<<16)|((im)&0xffffu))
#define R(rs,rt,rd,fn) (((rs)<<21)|((rt)<<16)|((rd)<<11)|(fn))
#define RET R(31,0,0,8)
static void put(MemoriesMemory *m,unsigned at,const uint32_t *w,size_t n){for(size_t i=0;i<n;i++)Memories_WriteLE32(m->ram+at+i*4,w[i]);}
int main(void){
 MemoriesMemory *m=calloc(1,sizeof(*m));assert(m);LekakExecResult r;
 const uint32_t main_irq[]={R(31,0,16,0x21),I(9,0,20,77),I(15,0,4,0x8000),I(13,4,4,0x300),
  I(9,0,8,0xb0),I(9,0,9,0x19),R(8,0,31,9),0,I(15,0,8,0x1f80),I(9,0,9,0x10),I(0x2b,8,9,0x1074),
  I(9,0,9,3),I(0x2b,8,9,0x1108),I(9,0,9,0x18),I(0x2b,8,9,0x1104),I(15,0,10,0x8000),I(13,10,10,0x200),
  I(0x23,10,11,0),0,I(4,11,0,0xfffd),0,R(16,0,31,0x21),RET,0};
 const uint32_t handler[]={I(15,0,8,0x1f80),I(0x2b,8,0,0x1070),I(15,0,8,0x8000),I(9,0,9,1),I(0x2b,8,9,0x200),
  I(9,0,20,99),I(9,0,8,0xb0),I(9,0,9,0x17),R(8,0,0,8),0};
 put(m,0x1000,main_irq,sizeof(main_irq)/4);put(m,0x1800,handler,sizeof(handler)/4);
 Memories_WriteLE32(m->ram+0x300,0x80001800);Memories_WriteLE32(m->ram+0x304,0x801ffe00);
 assert(LekakExec_Run(m,0x80001000,0x1234,0x801fff00,500,&r));
 assert(r.irq_entries==1&&r.irq_returns==1&&r.registers[20]==77&&r.registers[28]==0x1234);
 assert(r.registers[29]==0x801fff00&&r.registers[16]==LEKAK_EXEC_RETURN&&Memories_ReadLE32(m->ram+0x200)==1);
 memset(m,0,sizeof(*m));
 const uint32_t dma[]={I(15,0,8,0x1f80),I(15,0,9,0x400),I(13,9,9,2),I(0x2b,8,9,0x1814),
  I(9,0,9,0x800),I(0x2b,8,9,0x10f0),I(15,0,9,0x84),I(0x2b,8,9,0x10f4),
  I(9,0,9,0x300),I(0x2b,8,9,0x10a0),I(15,0,9,0x100),I(13,9,9,0x401),I(0x2b,8,9,0x10a8),
  I(0x23,8,10,0x10a8),0,RET,0};
 const uint32_t packet[]={0x03ffffff,0x020000ff,0x00140010,0x00010010};
 put(m,0x1000,dma,sizeof(dma)/4);put(m,0x300,packet,4);
 assert(LekakExec_Run(m,0x80001000,0,0x801fff00,100,&r));
 assert(r.gpu_dma_transfers==1&&r.gpu_dma_words==3&&r.gp0_commands==1&&r.registers[10]==0x401);
 assert(SoftGpu_Vram()[20*1024+16]==31&&r.dma_interrupt==0x84840000&&(r.irq_status&8));
 /* Polyline spans two linked-list payloads, then a distinct draw attribute.
  * Validate the entire stream before drawing or signaling completion. */
 const uint32_t poly_a[]={0x04000340,0xe407ffff,0x4800ff00,0x00050005,0x00050008};
 const uint32_t poly_b[]={0x03ffffff,0x00080008,0x55555555,0xe1000000};
 put(m,0x300,poly_a,5);put(m,0x340,poly_b,4);
 assert(LekakExec_Run(m,0x80001000,0,0x801fff00,100,&r));
 assert(r.gpu_dma_transfers==1&&r.gpu_dma_words==7&&r.gp0_commands==3);
 assert(SoftGpu_Vram()[5*1024+6]==0x3e0);
 Memories_WriteLE32(m->ram+0x340,0x01ffffff);
 assert(!LekakExec_Run(m,0x80001000,0,0x801fff00,100,&r)&&strstr(r.reason,"DMA packet"));
 assert(r.gpu_dma_transfers==0&&r.gp0_commands==0&&SoftGpu_Vram()[5*1024+6]==0);
 put(m,0x1000,dma,sizeof(dma)/4);Memories_WriteLE32(m->ram+0x300,0x00000300);
 assert(!LekakExec_Run(m,0x80001000,0,0x801fff00,100,&r)&&strstr(r.reason,"DMA list")&&r.gpu_dma_transfers==0);
 put(m,0x300,packet,4);Memories_WriteLE32(m->ram+0x304,0xa0000000);
 assert(!LekakExec_Run(m,0x80001000,0,0x801fff00,100,&r)&&strstr(r.reason,"DMA packet")&&r.gpu_dma_transfers==0);
 put(m,0x300,packet,4);Memories_WriteLE32(m->ram+0x1010,I(9,0,9,0));
 assert(LekakExec_Run(m,0x80001000,0,0x801fff00,100,&r)&&r.gpu_dma_transfers==0&&r.registers[10]==0x01000401);
 memset(m,0,sizeof(*m));
 const uint32_t otc[]={I(15,0,8,0x1f80),I(15,0,9,0x800),I(0x2b,8,9,0x10f0),I(15,0,9,0xc0),I(0x2b,8,9,0x10f4),
  I(9,0,9,0x30c),I(0x2b,8,9,0x10e0),I(9,0,9,4),I(0x2b,8,9,0x10e4),I(15,0,9,0x1100),I(13,9,9,2),I(0x2b,8,9,0x10e8),
  I(0x23,8,10,0x10e8),I(0x23,8,11,0x10e0),0,RET,0};
 put(m,0x1000,otc,sizeof(otc)/4);m->ram[0x2fc]=0xaa;m->ram[0x310]=0xbb;
 assert(LekakExec_Run(m,0x80001000,0,0x801fff00,100,&r)&&r.otc_dma_transfers==1&&r.otc_dma_words==4);
 assert(r.registers[10]==2&&r.registers[11]==0x2fc&&r.dma_interrupt==0xc0c00000&&(r.irq_status&8));
 assert(Memories_ReadLE32(m->ram+0x300)==0xffffff&&Memories_ReadLE32(m->ram+0x304)==0x300);
 assert(Memories_ReadLE32(m->ram+0x308)==0x304&&Memories_ReadLE32(m->ram+0x30c)==0x308);
 assert(m->ram[0x2fc]==0xaa&&m->ram[0x310]==0xbb);
 Memories_WriteLE32(m->ram+0x1014,I(9,0,9,8));
 assert(!LekakExec_Run(m,0x80001000,0,0x801fff00,100,&r)&&r.otc_dma_transfers==0&&strstr(r.reason,"memory"));
 put(m,0x1000,otc,sizeof(otc)/4);Memories_WriteLE32(m->ram+0x101c,I(9,0,9,0));
 assert(!LekakExec_Run(m,0x80001000,0,0x801fff00,100,&r)&&strstr(r.reason,"OTC DMA exceeds"));
 /* Request-mode DMA2 round trip through an odd, wrapped image. */
 memset(m,0,sizeof(*m));
 const uint32_t image_dma[]={
 I(15,0,8,0x1f80),I(15,0,9,0x400),I(13,9,9,2),I(0x2b,8,9,0x1814),
 I(9,0,9,0x800),I(0x2b,8,9,0x10f0),
 I(15,0,9,0xa000),I(0x2b,8,9,0x1810),
 I(15,0,9,0x1ff),I(13,9,9,0x3ff),I(0x2b,8,9,0x1810),
 I(15,0,9,1),I(13,9,9,3),I(0x2b,8,9,0x1810),
 I(9,0,9,0x300),I(0x2b,8,9,0x10a0),
 I(15,0,9,1),I(13,9,9,2),I(0x2b,8,9,0x10a4),
 I(15,0,9,0x100),I(13,9,9,0x201),I(0x2b,8,9,0x10a8),
 I(15,0,9,0xc000),I(0x2b,8,9,0x1810),
 I(15,0,9,0x1ff),I(13,9,9,0x3ff),I(0x2b,8,9,0x1810),
 I(15,0,9,1),I(13,9,9,3),I(0x2b,8,9,0x1810),
 I(15,0,9,0x400),I(13,9,9,3),I(0x2b,8,9,0x1814),
 I(9,0,9,0x400),I(0x2b,8,9,0x10a0),
 I(15,0,9,1),I(13,9,9,2),I(0x2b,8,9,0x10a4),
 I(15,0,9,0x100),I(13,9,9,0x200),I(0x2b,8,9,0x10a8),
 I(0x23,8,10,0x10a8),I(0x23,8,11,0x10a0),0,RET,0};
 put(m,0x1000,image_dma,sizeof(image_dma)/4);
 Memories_WriteLE32(m->ram+0x300,0x22221111);Memories_WriteLE32(m->ram+0x304,0xdead3333);
 assert(LekakExec_Run(m,0x80001000,0,0x801fff00,200,&r));
 assert(r.gpu_dma_transfers==2&&r.gpu_dma_words==4&&r.registers[10]==0x200&&r.registers[11]==0x408);
 assert(Memories_ReadLE32(m->ram+0x400)==0x22221111&&Memories_ReadLE32(m->ram+0x404)==0x3333);
 /* Entire malformed range/count/budget rejected before touching VRAM. */
 put(m,0x1000,image_dma,sizeof(image_dma)/4);Memories_WriteLE32(m->ram+0x1044,I(13,9,9,3));
 assert(!LekakExec_Run(m,0x80001000,0,0x801fff00,200,&r));
 assert(strstr(r.reason,"remaining image")&&r.gpu_dma_transfers==0&&SoftGpu_Vram()[511*1024+1023]==0);
 put(m,0x1000,image_dma,sizeof(image_dma)/4);Memories_WriteLE32(m->ram+0x1038,I(15,0,9,0x20));
 assert(!LekakExec_Run(m,0x80001000,0,0x801fff00,200,&r));
 assert(strstr(r.reason,"memory")&&r.gpu_dma_transfers==0&&SoftGpu_Vram()[511*1024+1023]==0);
 put(m,0x1000,image_dma,sizeof(image_dma)/4);
 assert(!LekakExec_Run(m,0x80001000,0,0x801fff00,23,&r));
 assert(strstr(r.reason,"GPU DMA exceeds diagnostic budget")&&r.gpu_dma_transfers==0&&SoftGpu_Vram()[511*1024+1023]==0);
 free(m);puts("IRQ context restoration, GPU/OTC DMA data and completion, bounds/budget and malformed list rejection passed");return 0;
}
