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
static int finish(LekakExecSession *s,unsigned quota,LekakExecResult *r){
 for(unsigned i=0;i<2000;i++){int code=LekakExec_Slice(s,quota,r);if(code!=LEKAK_EXEC_YIELDED)return code;}
 assert(!"session failed to make progress");return -1;
}
int main(void){
 MemoriesMemory *a=calloc(1,sizeof(*a)),*b=calloc(1,sizeof(*b));assert(a&&b);
 const uint32_t code[]={I(15,0,8,0x8000),I(0x23,8,9,0x300),I(9,9,10,1),I(9,9,11,2),
 I(9,0,12,10),I(9,12,12,-1),I(5,12,0,-2),I(9,13,13,1),I(0x2b,8,13,0x304),RET,I(9,14,14,7)};
 put(a,0x1000,code,sizeof(code)/4);Memories_WriteLE32(a->ram+0x300,20);*b=*a;
 LekakExecResult ref,out;
 assert(LekakExec_Run(a,0x80001000,0x1234,0x801fff00,1000,&ref));
 for(unsigned quota=1;quota<=13;quota++){
 put(b,0x1000,code,sizeof(code)/4);Memories_WriteLE32(b->ram+0x304,0);
 LekakExecSession *s=LekakExec_Create(b,0x80001000,0x1234,0x801fff00,NULL,NULL);assert(s);
 assert(finish(s,quota,&out)==LEKAK_EXEC_FINISHED);
 assert(!memcmp(ref.registers,out.registers,sizeof(ref.registers)));
 assert(ref.nominal_cycles==out.nominal_cycles&&ref.total_steps==out.total_steps);
 assert(out.registers[10]==1&&out.registers[11]==22&&out.registers[13]==10&&out.registers[14]==7);
 assert(Memories_ReadLE32(b->ram+0x304)==10);
 LekakExecResult again;assert(LekakExec_Slice(s,1,&again)==LEKAK_EXEC_FINISHED);
 assert(again.total_steps==out.total_steps);LekakExec_Destroy(s);
 }
 memset(a,0,sizeof(*a));
 const uint32_t dma[]={I(15,0,8,0x1f80),I(15,0,9,0x400),I(13,9,9,2),I(0x2b,8,9,0x1814),
 I(9,0,9,0x800),I(0x2b,8,9,0x10f0),I(9,0,9,0x300),I(0x2b,8,9,0x10a0),I(15,0,9,0x100),
 I(13,9,9,0x401),I(0x2b,8,9,0x10a8),RET,0};
 const uint32_t packet[]={0x04ffffff,0xe407ffff,0x020000ff,0x00140010,0x00010010};
 put(a,0x1000,dma,sizeof(dma)/4);put(a,0x300,packet,5);*b=*a;
 assert(LekakExec_Run(a,0x80001000,0,0x801fff00,1000,&ref));
 LekakExecSession *s=LekakExec_Create(b,0x80001000,0,0x801fff00,NULL,NULL);assert(s);
 assert(finish(s,1,&out)==LEKAK_EXEC_FINISHED&&out.gpu_dma_transfers==1&&out.gpu_dma_words==4);
 assert(SoftGpu_Vram()[20*1024+16]==31&&out.total_steps==ref.total_steps&&out.nominal_cycles==ref.nominal_cycles);
 LekakExec_Destroy(s);
 /* Actual BIOS receives changed live inputs on later VBlanks. */
 memset(b,0,sizeof(*b));
 const uint32_t pad_code[]={I(9,0,8,0xb0),I(9,0,9,0x18),R(8,0,31,9),0,
 I(15,0,4,0x8000),I(13,4,4,0x700),I(9,0,5,34),I(15,0,6,0x8000),I(13,6,6,0x780),I(9,0,7,34),
 I(9,0,8,0xb0),I(9,0,9,0x12),R(8,0,31,9),0,
 I(9,0,8,0xb0),I(9,0,9,0x13),R(8,0,31,9),0,I(4,0,0,-1),0};
 put(b,0x1000,pad_code,sizeof(pad_code)/4);
 LekakExecPads live={{1,0},{8,0},0,0,0};out.pad_polls=0;
 s=LekakExec_Create(b,0x80001000,0,0x801fff00,&live,NULL);assert(s);
 for(unsigned phase=0;phase<3;phase++){
 if(phase==1)live.pressed[0]=0;
 if(phase==2)live.connected[0]=0;
 LekakExec_SetPads(s,&live);unsigned polls=out.pad_polls;
 for(unsigned i=0;i<2000;i++){assert(LekakExec_Slice(s,512,&out)==LEKAK_EXEC_YIELDED);if(out.pad_polls>polls)break;}
 assert(out.pad_polls>polls);
 if(phase<2)assert(b->ram[0x700]==0&&b->ram[0x701]==0x41&&b->ram[0x702]==(phase?0xff:0xf7));
 else assert(b->ram[0x700]==0xff);
 }
 assert(out.irq_entries>=3);LekakExec_Destroy(s);
 memset(b,0,sizeof(*b));Memories_WriteLE32(b->ram+0x1000,0xffffffff);
 s=LekakExec_Create(b,0x80001000,0,0x801fff00,NULL,NULL);assert(s);
 assert(LekakExec_Slice(s,0,&out)==LEKAK_EXEC_FAULT);
 assert(LekakExec_Slice(s,1000001,&out)==LEKAK_EXEC_FAULT);
 assert(LekakExec_Slice(s,1,&out)==LEKAK_EXEC_FAULT);uint64_t stopped=out.total_steps;assert(stopped==1);
 assert(LekakExec_Slice(s,1,&out)==LEKAK_EXEC_FAULT&&out.total_steps==stopped);
 LekakExec_Destroy(s);LekakExec_Destroy(NULL);free(a);free(b);
 puts("Persistent session: load/branch-delay parity, atomic DMA, live pad press/release/disconnect, sliced IRQ and terminal faults passed");return 0;
}
