#include "../App/guest_exec.h"
#include "../App/bios_patches.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define I(op,rs,rt,im) (((uint32_t)(op)<<26)|((rs)<<21)|((rt)<<16)|((im)&0xffffu))
#define R(rs,rt,rd,fn) (((rs)<<21)|((rt)<<16)|((rd)<<11)|(fn))
#define RET R(31,0,0,8)
static void put(MemoriesMemory *m,unsigned at,const uint32_t *w,size_t n){for(size_t i=0;i<n;i++)Memories_WriteLE32(m->ram+at+i*4,w[i]);}
int main(void){
 MemoriesMemory *m=calloc(1,sizeof(*m));assert(m);LekakExecResult r;
 const uint32_t code[]={0x8c420018,0,0x24420028,0x00407821,0x3c0a8000,0x254a3000,0x3c098000,0x25293018,
  0x8d430000,0x8c4b0000,0x254a0004,0x146b000e,0x24420004,0x1549fffa,0,0x01e01021,
  0x3c0a8000,0x254a3018,0x3c098000,0x25293030,0x8d430000,0,0xac430000,0x254a0004,0x1549fffb,0x24420004};
 const uint32_t data[]={0xaf410004,0xaf420008,0xaf43000c,0xaf5f007c,0x40037000,0,
  0xaf410004,0xaf420008,0x40026800,0xaf43000c,0x40037000,0xaf5f007c};
 put(m,0x1014,code,26);put(m,0x3000,data,12);
 assert(LekakBios_CausePatch(m,0x80001014)==0x8000107c);
 for(unsigned i=0;i<26;i++){
  Memories_WriteLE32(m->ram+0x1014+i*4,code[i]^0x10000u);
  assert(!LekakBios_CausePatch(m,0x80001014));Memories_WriteLE32(m->ram+0x1014+i*4,code[i]);
 }
 Memories_WriteLE32(m->ram+0x3020,0);assert(!LekakBios_CausePatch(m,0x80001014));put(m,0x3000,data,12);
 assert(!LekakBios_CausePatch(m,0x801ffffc));
 const uint32_t enter[]={R(31,0,16,0x21),I(9,0,9,0x56),I(9,0,8,0xb0),R(8,0,31,9),0};
 const uint32_t leave[]={R(16,0,31,0x21),RET,0};put(m,0x1000,enter,5);put(m,0x107c,leave,3);
 assert(LekakExec_Run(m,0x80001000,0,0x801fff00,100,&r)&&r.bios_cause_patches==1&&r.bios_calls==1);
 assert(Memories_ReadLE32(m->ram+0x18)==0); /* No fictitious jump-table address. */
 Memories_WriteLE32(m->ram+0x1014,0);assert(!LekakExec_Run(m,0x80001000,0,0x801fff00,100,&r)&&strstr(r.reason,"Unrecognized BIOS"));
 memset(m,0,sizeof(*m));
 const uint32_t pad[]={0x240a0009,0x8c42016c,0,0x2043062c,0xac600000,0x24630004,0x254affff,0x1540fffc,0};
 put(m,0x1014,pad,9);put(m,0x1038,leave,3);put(m,0x1000,enter,5);Memories_WriteLE32(m->ram+0x1004,I(9,0,9,0x57));
 assert(LekakBios_PadAckPatch(m,0x80001014)==0x80001038);
 assert(LekakExec_Run(m,0x80001000,0,0x801fff00,100,&r)&&r.pad_ack_patch_enabled&&r.bios_pad_ack_patches==1);
 Memories_WriteLE32(m->ram+0x1030,0);assert(!LekakBios_PadAckPatch(m,0x80001014));
 memset(m,0,sizeof(*m));
 const uint32_t error[]={0x8c42016c,0x2409000b,0x20430884,0x3c018000,0xac230500,
  0x20430894,0x3c018000,0xac230504,0xac400594,0x24420004,0x2529ffff,0x1520fffc,0};
 put(m,0x1014,error,13);put(m,0x1000,enter,5);Memories_WriteLE32(m->ram+0x1004,I(9,0,9,0x57));
 const uint32_t callback[]={I(15,0,8,0x8000),I(0x23,8,9,0x504),0,R(9,0,31,9),0,R(16,0,31,0x21),RET,0};
 put(m,0x1048,callback,sizeof(callback)/4);uint32_t slots[2];
 assert(LekakBios_PadErrorPatch(m,0x80001014,slots)==0x80001048&&slots[0]==0x80000500&&slots[1]==0x80000504);
 assert(LekakExec_Run(m,0x80001000,0,0x801fff00,100,&r)&&r.bios_pad_error_patches==1&&r.pad_error_patch_enabled);
 assert(Memories_ReadLE32(m->ram+0x500)==LEKAK_PAD_ENABLE_FN&&Memories_ReadLE32(m->ram+0x504)==LEKAK_PAD_DISABLE_FN);
 assert(r.pad_poll_callbacks==1&&!r.pad_poll_enabled);
 Memories_WriteLE32(m->ram+0x1040,0);assert(!LekakBios_PadErrorPatch(m,0x80001014,slots));
 memset(m,0,sizeof(*m));
 /* MFC0 has a load delay; MTC0/SYS keep logical enables consistent. */
 const uint32_t sr[]={0x40086000,I(9,8,9,1),I(9,8,10,1),I(9,0,11,0),0x408b6000,
  0x400c6000,0,I(9,0,4,2),0xc,0x400d6000,0,RET,0};put(m,0x1000,sr,sizeof(sr)/4);
 assert(LekakExec_Run(m,0x80001000,0,0x801fff00,100,&r));
 assert(r.registers[9]==1&&r.registers[10]==0x40000402&&r.registers[12]==0&&r.registers[13]==0x401);
 assert(r.cp0_sr_reads==3&&r.cp0_sr_writes==1&&r.logical_interrupts_enabled);
 const uint32_t bad_sr[]={I(15,0,8,1),0x40886000};put(m,0x1000,bad_sr,2);
 assert(!LekakExec_Run(m,0x80001000,0,0x801fff00,100,&r)&&strstr(r.reason,"SR control"));
 const uint32_t gte_disabled[]={0x40806000,0x4a000001};put(m,0x1000,gte_disabled,2);
 assert(!LekakExec_Run(m,0x80001000,0,0x801fff00,100,&r)&&strstr(r.reason,"COP2 disabled"));
 memset(m,0,sizeof(*m));
 const uint32_t gpu[]={R(31,0,16,0x21),I(9,0,8,0xa0),I(9,0,9,0x49),I(15,0,4,0xe100),I(13,4,4,0x123),
  R(8,0,31,9),0,R(2,0,17,0x21),I(9,0,9,0x48),I(15,0,4,0x0300),R(8,0,31,9),0,
  R(16,0,31,0x21),RET,0};put(m,0x1000,gpu,sizeof(gpu)/4);
 assert(LekakExec_Run(m,0x80001000,0,0x801fff00,100,&r)&&r.registers[17]==0&&r.gp0_commands==1&&r.gp1_commands==1);
 assert((r.gpu_status&0x7ff)==0x123&&!(r.gpu_status&0x800000));
 memset(m,0,sizeof(*m));
 const uint32_t chain[]={R(31,0,16,0x21),I(9,0,8,0xc0),I(9,0,9,3),I(9,0,4,1),I(15,0,5,0x8000),I(13,5,5,0x500),
  R(8,0,31,9),0,R(2,0,17,0x21),I(9,0,9,2),R(8,0,31,9),0,I(9,0,9,3),R(8,0,31,9),0,R(2,0,18,0x21),
  R(16,0,31,0x21),RET,0};put(m,0x1000,chain,sizeof(chain)/4);
 assert(LekakExec_Run(m,0x80001000,0,0x801fff00,100,&r)&&r.registers[17]==0&&r.registers[18]==0x80000500&&r.interrupt_chains[1]==0);
 const uint32_t nonhead[]={R(31,0,16,0x21),I(9,0,8,0xc0),I(9,0,9,2),I(9,0,4,1),I(15,0,5,0x8000),I(13,5,5,0x500),
  R(8,0,31,9),0,I(9,5,5,16),R(8,0,31,9),0,I(9,0,9,3),I(9,5,5,-16),R(8,0,31,9),0};
 memset(m,0,sizeof(*m));put(m,0x1000,nonhead,sizeof(nonhead)/4);
 assert(!LekakExec_Run(m,0x80001000,0,0x801fff00,100,&r)&&strstr(r.reason,"head removal")&&r.interrupt_chains[1]==0x80000510);
 memset(m,0,sizeof(*m));
 const uint32_t unexported[]={I(15,0,8,0xffff),I(13,8,8,0xffe8),R(8,0,0,8),0};put(m,0x1000,unexported,4);
 assert(!LekakExec_Run(m,0x80001000,0,0x801fff00,100,&r)&&strstr(r.reason,"before its patch"));
 free(m);puts("Known BIOS patch semantics, unknown/mutated rejection, SR load delay/control, COP2 gate and A48/A49 graphics routing passed");return 0;
}
