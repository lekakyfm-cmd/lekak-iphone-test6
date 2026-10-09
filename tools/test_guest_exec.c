#include "../App/guest_exec.h"
#include "../App/irq_registers.h"
#include "../App/timer_registers.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define I(op,rs,rt,imm) (((uint32_t)(op)<<26)|((rs)<<21)|((rt)<<16)|((imm)&0xffffu))
#define R(rs,rt,rd,fn) (((rs)<<21)|((rt)<<16)|((rd)<<11)|(fn))
#define RET R(31,0,0,8)
static void program(MemoriesMemory *m,const uint32_t *words,size_t count){memset(m,0,sizeof(*m));for(size_t i=0;i<count;i++)Memories_WriteLE32(m->ram+0x1000+i*4,words[i]);}
int main(void){
 MemoriesMemory *m=malloc(sizeof(*m));assert(m);LekakExecResult r;
 LekakIrqRegisters irq={0};uint32_t value;
 assert(LekakIrq_Read(&irq,0x1f801070,2,&value)&&value==0);
 irq.status=0x405;
 assert(LekakIrq_Write(&irq,0x9f801070,2,0xfffb)&&irq.status==0x401);
 assert(LekakIrq_Write(&irq,0xbf801070,4,0xffff)&&irq.status==0x401);
 assert(LekakIrq_Write(&irq,0x1f801070,4,0)&&irq.status==0);
 assert(LekakIrq_Write(&irq,0x1f801074,4,0xffffffff)&&irq.mask==0x7ff);
 assert(LekakIrq_Read(&irq,0xbf801074,4,&value)&&value==0x7ff);
 assert(!LekakIrq_Write(&irq,0x1f801072,2,0)&&!LekakIrq_Read(&irq,0x1f801074,1,&value));
 assert(LekakIrq_Write(&irq,0x1f8010f0,4,0x33333333)&&irq.dpcr==0x33333333);
 irq.dicr=0x01810000;
 assert(LekakIrq_Write(&irq,0x1f8010f4,4,0x00810000)&&irq.dicr==0x81810000&&(irq.status&8));
 assert(LekakIrq_Write(&irq,0x1f8010f4,4,0x01810000)&&irq.dicr==0x00810000);
 irq.dicr=0x01000000;
 assert(LekakIrq_Write(&irq,0x1f8010f4,4,0x00820000)&&irq.dicr==0x01820000); /* Flag 0 cannot fire enabled channel 1. */
 irq.dpcr=0x12345678;uint32_t half;
 assert(LekakIrq_Read(&irq,0x1f8010f2,2,&half)&&half==0x1234);
 assert(LekakIrq_Write(&irq,0x1f8010f1,1,0xab)&&irq.dpcr==0x1234ab78);
 irq.dicr=0x83830001;irq.status=8;
 assert(LekakIrq_Read(&irq,0x1f8010f6,2,&half)&&half==0x8383);
 assert(LekakIrq_Write(&irq,0x1f8010f6,2,0x0383)&&irq.dicr==0x00830001);
 irq.dicr=0x81810001;
 assert(LekakIrq_Write(&irq,0x1f8010f7,1,1)&&irq.dicr==0x00810001);
 assert(!LekakIrq_Write(&irq,0x1f8010f5,2,0xffff));
 irq.dicr=0;
 assert(LekakIrq_Write(&irq,0x1f8010f4,4,0x8000)&&irq.dicr==0x80008000);
 assert(LekakIrq_Write(&irq,0x1f8010f4,2,0)&&irq.dicr==0);
 LekakTimerRegisters timers={0};
 assert(LekakTimer_Write(&timers,0x1f801114,4,0x100));
 assert(LekakTimer_Read(&timers,0xbf801114,2,&value)&&value==0x500);
 assert(LekakTimer_Write(&timers,0x1f801128,4,0x12345678));
 assert(LekakTimer_Read(&timers,0x1f801128,4,&value)&&value==0x5678);
 timers.mode[0]=0x1c00;
 assert(LekakTimer_Read(&timers,0x1f801104,2,&value)&&value==0x1c00&&timers.mode[0]==0x400);
 assert(LekakTimer_Read(&timers,0x1f801100,2,&value)&&value==0);
 const uint32_t irq_access[]={I(15,0,8,0x1f80),I(13,8,8,0x1070),
  I(9,0,9,0xffff),I(0x29,8,9,4),I(0x25,8,10,4),0,
  I(0x2b,8,0,4),I(0x23,8,11,4),0,I(0x29,8,0,0),RET,0};
 program(m,irq_access,sizeof(irq_access)/4);
 assert(LekakExec_Run(m,0x80001000,0,0x801fff00,100,&r));
 assert(r.registers[10]==0x7ff&&r.registers[11]==0&&r.irq_status==0&&r.irq_mask==0);
 assert(r.irq_reads==2&&r.irq_writes==3);
 const uint32_t register_hooks[]={R(31,0,16,0x21),I(15,0,4,0x8000),I(13,4,4,0x200),
  I(9,0,10,0xb0),I(9,0,9,0x19),R(10,0,31,9),0,
  I(9,0,4,0),I(15,0,5,0x8000),I(13,5,5,0x300),I(9,0,10,0xc0),I(9,0,9,2),R(10,0,31,9),0,
  I(9,5,5,16),R(10,0,31,9),0,
  I(9,0,4,1),I(9,0,5,0),I(9,0,9,0xa),R(10,0,31,9),0,R(2,0,17,0x21),
  I(9,0,10,0xb0),I(9,0,9,0x5b),I(9,0,4,0),R(10,0,31,9),0,R(2,0,18,0x21),
  R(16,0,31,0x21),RET,0};
 program(m,register_hooks,sizeof(register_hooks)/4);
 assert(LekakExec_Run(m,0x80001000,0,0x801fff00,200,&r));
 assert(r.interrupt_hook==0x80000200&&r.interrupt_chains[0]==0x80000310&&r.bios_calls==5);
 assert(Memories_ReadLE32(m->ram+0x310)==0x80000300&&Memories_ReadLE32(m->ram+0x300)==0);
 assert(r.registers[17]==1&&r.registers[18]==1);
 const uint32_t basic[]={I(15,0,8,0x8000),I(13,8,8,0x200),I(9,0,9,1234),I(0x2b,8,9,0),I(0x23,8,10,0),I(9,10,11,1),I(9,10,12,1),RET,0};
 program(m,basic,sizeof(basic)/4);assert(LekakExec_Run(m,0x80001000,0,0x801fff00,100,&r));
 assert(r.registers[10]==1234&&r.registers[11]==1&&r.registers[12]==1235&&r.steps==9);assert(Memories_ReadLE32(m->ram+0x200)==1234);
 const uint32_t branch[]={I(9,0,8,1),I(4,8,8,2),I(9,0,9,5),I(9,0,9,99),RET,0};
 program(m,branch,sizeof(branch)/4);assert(LekakExec_Run(m,0x80001000,0,0x801fff00,100,&r)&&r.registers[9]==5);
 const uint32_t merge[]={I(15,0,8,0x8000),I(13,8,8,0x201),I(0x22,8,9,3),I(0x26,8,9,0),0,RET,0};
 program(m,merge,sizeof(merge)/4);for(unsigned i=0;i<8;i++)m->ram[0x200+i]=(uint8_t)(0x11*(i+1));
 assert(LekakExec_Run(m,0x80001000,0,0x801fff00,100,&r)&&r.registers[9]==0x55443322);
 const uint32_t divide[]={I(15,0,8,0x8000),I(9,0,9,0xffff),R(8,9,0,0x1a),R(0,0,10,0x12),RET,0};
 program(m,divide,sizeof(divide)/4);assert(LekakExec_Run(m,0x80001000,0,0x801fff00,100,&r)&&r.registers[10]==0x80000000);
 const uint32_t overflow[]={I(15,0,8,0x7fff),I(13,8,8,0xffff),I(8,8,9,1)};
 program(m,overflow,sizeof(overflow)/4);assert(!LekakExec_Run(m,0x80001000,0,0x801fff00,100,&r)&&strstr(r.reason,"overflow"));
 const uint32_t mmio[]={I(15,0,8,0x1f80),I(0x23,8,9,0x1800)};
 program(m,mmio,sizeof(mmio)/4);assert(!LekakExec_Run(m,0x80001000,0,0x801fff00,100,&r)&&r.detail==0x1f801800&&r.pc==0x80001004);
 const uint32_t startup[]={R(31,0,16,0x21),I(9,0,2,77),I(9,0,8,0xa0),I(9,0,9,0x72),R(8,0,31,9),0,
  I(9,0,9,0x56),R(8,0,31,9),0,R(2,0,17,0x21),I(9,0,9,0x44),R(8,0,31,9),0,
  I(9,0,9,0x9f),I(9,0,4,2),R(8,0,31,9),0,I(9,0,8,0xb0),I(9,0,9,0x18),R(8,0,31,9),0,
  R(2,0,18,0x21),I(9,0,9,0x35),I(9,0,4,1),I(15,0,5,0x8000),I(13,5,5,0x200),I(9,0,6,5),R(8,0,31,9),0,
  R(16,0,31,0x21),RET,0};
 program(m,startup,sizeof(startup)/4);memcpy(m->ram+0x200,"hello",5);
 assert(LekakExec_Run(m,0x80001000,0,0x801fff00,200,&r));
 assert(r.cd_driver_removals==2&&!r.cd_iso_driver_registered&&r.memory_megabytes==2);
 assert(r.registers[17]==77&&r.registers[18]==LEKAK_DEFAULT_IRQ_CONTEXT&&r.registers[2]==5);
 assert(Memories_ReadLE32(m->ram+0x400)==LEKAK_EXCEPTION_RETURN&&Memories_ReadLE32(m->ram+0x404)==0x8000fff0);
 assert(r.console_bytes==5&&!strcmp(r.console_output,"hello")&&!r.console_truncated&&r.bios_calls==6);
 const uint32_t stdout_bad[]={I(9,0,8,0xb0),I(9,0,9,0x35),I(9,0,4,2),R(8,0,0,8),0};
 program(m,stdout_bad,sizeof(stdout_bad)/4);assert(!LekakExec_Run(m,0x80001000,0,0x801fff00,100,&r)&&strstr(r.reason,"descriptor"));
 const uint32_t stdout_long[]={R(31,0,16,0x21),I(9,0,8,0xb0),I(9,0,9,0x35),I(9,0,4,1),I(15,0,5,0x8000),
  I(13,5,5,0x3000),I(9,0,6,5000),R(8,0,31,9),0,R(16,0,31,0x21),RET,0};
 program(m,stdout_long,sizeof(stdout_long)/4);memset(m->ram+0x3000,'X',5000);
 assert(LekakExec_Run(m,0x80001000,0,0x801fff00,100,&r)&&r.console_truncated);
 assert(r.console_bytes==LEKAK_EXEC_CONSOLE-1&&r.console_output[LEKAK_EXEC_CONSOLE-1]==0&&r.registers[2]==5000);
 Memories_WriteLE32(m->ram+0x1014,I(13,5,5,0xfffc));Memories_WriteLE32(m->ram+0x1010,I(15,0,5,0x801f));
 assert(!LekakExec_Run(m,0x80001000,0,0x801fff00,100,&r)&&r.bios_calls==0);
 const uint32_t setmem_bad[]={I(9,0,8,0xa0),I(9,0,9,0x9f),I(9,0,4,8),R(8,0,0,8),0};
 program(m,setmem_bad,sizeof(setmem_bad)/4);assert(!LekakExec_Run(m,0x80001000,0,0x801fff00,100,&r)&&strstr(r.reason,"RAM configuration"));
 const uint32_t dma_config[]={I(15,0,8,0x1f80),I(9,0,9,0x401),I(0x2b,8,9,0x10a8),I(0x23,8,10,0x10a8),0,RET,0};
 program(m,dma_config,7);assert(LekakExec_Run(m,0x80001000,0,0x801fff00,100,&r)&&r.registers[10]==0x401);
 const uint32_t dma_start[]={I(15,0,8,0x1f80),I(15,0,9,0x100),I(0x2b,8,9,0x10d8)};
 program(m,dma_start,3);assert(!LekakExec_Run(m,0x80001000,0,0x801fff00,100,&r)&&strstr(r.reason,"DMA transfer")&&r.detail==5);
 const uint32_t bios[]={I(9,0,9,0x4f),I(9,0,8,0xa0),R(8,0,0,8),0};
 program(m,bios,sizeof(bios)/4);assert(!LekakExec_Run(m,0x80001000,0,0x801fff00,100,&r)&&strstr(r.reason,"BIOS")&&r.detail==0x4f);
 const uint32_t loop[]={I(4,0,0,0xffff),0};program(m,loop,2);assert(!LekakExec_Run(m,0x80001000,0,0x801fff00,7,&r)&&r.steps==7&&strstr(r.reason,"budget"));
 const uint32_t bad_delay[]={I(4,0,0,1),RET};program(m,bad_delay,2);assert(!LekakExec_Run(m,0x80001000,0,0x801fff00,100,&r)&&strstr(r.reason,"delay slot"));
 const uint32_t cop0[]={0x40086800};program(m,cop0,1);assert(!LekakExec_Run(m,0x80001000,0,0x801fff00,100,&r)&&strstr(r.reason,"unsupported"));
 const uint32_t critical[]={I(9,0,4,1),0x0000000c,R(2,0,8,0x21),
  0x0000000c,R(2,0,9,0x21),I(9,0,2,77),I(9,0,4,2),0x0000000c,
  R(2,0,10,0x21),I(9,0,4,1),0x0000000c,R(2,0,11,0x21),RET,0};
 program(m,critical,sizeof(critical)/4);
 assert(LekakExec_Run(m,0x80001000,0,0x801fff00,100,&r));
 assert(r.registers[8]==1&&r.registers[9]==0&&r.registers[10]==77&&r.registers[11]==1);
 assert(r.critical_enters==3&&r.critical_exits==1&&!r.logical_interrupts_enabled);
 const uint32_t unknown_sys[]={I(9,0,4,3),0x0000000c};program(m,unknown_sys,2);
 assert(!LekakExec_Run(m,0x80001000,0,0x801fff00,100,&r)&&r.detail==3&&strstr(r.reason,"System call"));
 const uint32_t sys_delay[]={I(9,0,4,1),I(4,0,0,1),0x0000000c};program(m,sys_delay,3);
 assert(!LekakExec_Run(m,0x80001000,0,0x801fff00,100,&r)&&strstr(r.reason,"delay slot"));
 /* Retail entry's BSS clear range and exact five-instruction loop, followed
  * by our return sentinel. The old limit must stop; the corrected limit
  * must finish, clear the entire span and leave both neighbours intact. */
 const uint32_t clear_bss[]={I(15,0,2,0x8009),I(13,2,2,0xb090),
  I(15,0,3,0x800f),I(13,3,3,0xe728),I(0x2b,2,0,0),I(9,2,2,4),
  R(2,3,1,0x2b),I(5,1,0,0xfffc),0,RET,0};
 program(m,clear_bss,sizeof(clear_bss)/4);
 memset(m->ram+0x9b08f,0xa5,0xfe728-0x9b090+2);
 assert(!LekakExec_Run(m,0x80001000,0,0x801fff00,100000,&r)&&strstr(r.reason,"budget"));
 memset(m->ram+0x9b08f,0xa5,0xfe728-0x9b090+2);
 assert(LekakExec_Run(m,0x80001000,0,0x801fff00,1000000,&r));
 assert(r.steps==4+5*((0xfe728-0x9b090)/4)+2);
 assert(r.registers[2]==0x800fe728&&m->ram[0x9b08f]==0xa5&&m->ram[0xfe728]==0xa5);
 for(size_t offset=0x9b090;offset<0xfe728;offset++)assert(m->ram[offset]==0);
 /* Overlay entry addresses are reused: unknown contents must execute,
  * never inherit the identified boot-check native compatibility stub. */
 memset(m,0,sizeof(*m));
 const uint32_t ordinary_overlay[]={I(9,0,2,77),RET,0};
 for(unsigned i=0;i<3;i++)Memories_WriteLE32(m->ram+0x1680f4+i*4,ordinary_overlay[i]);
 assert(LekakExec_Run(m,0x801680f4,0,0x801fff00,20,&r));
 assert(r.registers[2]==77&&r.native_boot_check_calls==0);
 free(m);puts("Bounded guest execution: load/branch delays, merge pair, signed division, overflow, MMIO/BIOS rejection, logical critical-section transitions, unknown SYS rejection, budget, delay-slot rejection and full retail BSS clear passed");return 0;
}
