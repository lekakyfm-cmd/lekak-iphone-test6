#include "../App/spu_config.h"
#include "../App/guest_exec.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
static uint32_t read_reg(LekakSpuConfig *s,uint32_t a){uint32_t v;assert(LekakSpu_Read(s,a,2,&v)==1);return v;}
static void write_reg(LekakSpuConfig *s,uint32_t a,uint16_t v){assert(LekakSpu_Write(s,a,2,v)==1);}
int main(void){
 LekakSpuConfig s={0};uint32_t unchanged=0xa5a5;
 write_reg(&s,0xbf801db0,0x8001);write_reg(&s,0x1f801db2,0x1234);
 assert(read_reg(&s,0x1f801db0)==0x8001&&read_reg(&s,0x1f801db2)==0x1234);
 assert(read_reg(&s,0x1f801db8)==0&&read_reg(&s,0x1f801dba)==0);
 write_reg(&s,0x1f801d80,0x3fff);write_reg(&s,0x1f801d82,0x4000);
 assert(read_reg(&s,0x1f801db8)==0);
 for(unsigned i=0;i<767;i++)LekakSpu_Cycle(&s);
 assert(read_reg(&s,0x1f801db8)==0&&s.ticks==0);
 LekakSpu_Cycle(&s);assert(s.ticks==1&&read_reg(&s,0x1f801db8)==0x7ffe&&read_reg(&s,0x1f801dba)==0x8000);
 assert(read_reg(&s,0x1f801db0)==0x8001); /* CD distinct from current/main gain */
 write_reg(&s,0x1f801db4,0xffff);assert(read_reg(&s,0x1f801db4)==0xffff);
 write_reg(&s,0x1f801daa,0xc001);assert(read_reg(&s,0x1f801daa)==0xc001&&read_reg(&s,0x1f801dae)==0);
 for(unsigned i=0;i<768;i++)LekakSpu_Cycle(&s);
 assert(read_reg(&s,0x1f801dae)==1);
 LekakSpuConfig before=s;
 assert(LekakSpu_Write(&s,0x1f801d80,2,0x8000)==-1);assert(!memcmp(&s,&before,sizeof(s)));
 for(uint32_t bit=4;bit<=0x2000;bit<<=1){
  if(bit==0x10||bit==0x20||bit==0x80)continue;
  assert(LekakSpu_Write(&s,0x1f801daa,2,bit)==-1);assert(!memcmp(&s,&before,sizeof(s)));
 }
 assert(LekakSpu_Write(&s,0x1f801db8,2,1)==-1);
 assert(LekakSpu_Write(&s,0x1f801dae,2,0xffff)==-1);
 assert(LekakSpu_Write(&s,0x1f801da8,2,1)==-1); /* no fake sound RAM FIFO */
 assert(LekakSpu_Write(&s,0x1f801d80,4,1)==-1);
 assert(LekakSpu_Read(&s,0x1f801d80,1,&unchanged)==-1&&unchanged==0xa5a5);
 assert(LekakSpu_Read(&s,0x1f801d81,2,&unchanged)==-1&&unchanged==0xa5a5);
 assert(LekakSpu_Read(&s,0x1f801db0,4,&unchanged)==-1&&unchanged==0xa5a5);
 write_reg(&s,0x1f801d84,0xffff);assert(read_reg(&s,0x1f801d84)==0xffff);
 assert(LekakSpu_Read(&s,0x1f801800,2,&unchanged)==0);
 /* Actual FIFO payload, bounded capacity, deferred mode and wrapping. */
 write_reg(&s,0x1f801daa,0);
 for(unsigned i=0;i<768;i++)LekakSpu_Cycle(&s);
 write_reg(&s,0x1f801dac,4);write_reg(&s,0x1f801da6,0xffff);
 for(unsigned i=0;i<32;i++)write_reg(&s,0x1f801da8,(uint16_t)(0x1200+i));
 before=s;assert(LekakSpu_Write(&s,0x1f801da8,2,0xdead)==-1);
 assert(!memcmp(&s,&before,sizeof(s)));
 assert(LekakSpu_Write(&s,0x1f801dac,2,6)==-1);
 assert(!memcmp(&s,&before,sizeof(s)));
 write_reg(&s,0x1f801daa,0x10);assert(s.fifo_count==32&&s.manual_halfwords==0);
 for(unsigned i=0;i<767;i++)LekakSpu_Cycle(&s);
 assert(s.fifo_count==32);LekakSpu_Cycle(&s);
 assert(s.fifo_count==0&&s.manual_halfwords==32&&s.transfer_address==56);
 assert(read_reg(&s,0x1f801da6)==0xffff&&read_reg(&s,0x1f801dae)==0x10);
 for(unsigned i=0;i<32;i++){unsigned a=(0x7fff8+i*2)&0x7ffff;
  assert(s.ram[a]==i&&s.ram[(a+1)&0x7ffff]==0x12);
 }
 write_reg(&s,0x1f801da8,0xabcd);assert(read_reg(&s,0x1f801dae)&0x400);
 for(unsigned i=0;i<768;i++)LekakSpu_Cycle(&s);
 assert(s.ram[56]==0xcd&&s.ram[57]==0xab&&!(read_reg(&s,0x1f801dae)&0x400));
 /* Real executor routes SDK-style gains and control accesses, preserves 16-bit
  * signed gains, exposes current gain only after the nominal sample boundary. */
 MemoriesMemory *m=calloc(1,sizeof(*m));assert(m);
 uint32_t code[]={0x3c081f80,0x24093fff,0xa5091d80,0xa5091db0,
  0x24090320,0x2529ffff,0x1520fffe,0,0x95021db8,0,0x95031db0,0,0x03e00008,0};
 for(unsigned i=0;i<sizeof(code)/sizeof(*code);i++)Memories_WriteLE32(m->ram+0x10000+i*4,code[i]);
 LekakExecResult r;assert(LekakExec_Run(m,0x80010000,0,0x801fff00,3000,&r));
 assert(r.registers[2]==0x7ffe&&r.registers[3]==0x3fff&&r.spu_writes==2&&r.spu_reads==2);
 assert(r.spu_cd_gain[0]==0x3fff&&r.spu_main_gain[0]==0x3fff);
 Memories_WriteLE32(m->ram+0x1000c,0xa5091da8);
 assert(!LekakExec_Run(m,0x80010000,0,0x801fff00,3000,&r));assert(r.detail==0x1f801da8&&strstr(r.reason,"SPU write"));
 free(m);puts("SPU fixed gains, distinct current/CD addresses, nominal sample boundary, control flags and unsupported mode/width rejection passed");return 0;
}
