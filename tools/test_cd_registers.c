#include "../App/cd_registers.h"
#include "../App/guest_exec.h"
#include <assert.h>
#include <stdlib.h>
static void cycles(LekakCdRegisters *c,unsigned n){while(n--)LekakCd_Cycle(c);}
static void write_cd(LekakCdRegisters *c,unsigned port,uint8_t v){assert(LekakCd_Write(c,0x1f801800u+port,v)==1);}
static uint8_t read_cd(LekakCdRegisters *c,unsigned port){uint8_t v=0xaa;assert(LekakCd_Read(c,0xbf801800u+port,&v)==1);return v;}
static void image_tests(unsigned stride){
 FILE *f=tmpfile();assert(f);uint8_t raw[2352]={0},out[2048],before[2048];
 if(stride==2352)raw[15]=2;
 unsigned offset=stride==2352?24:0;
 for(unsigned i=0;i<2048;i++)raw[offset+i]=(uint8_t)(i*13);
 assert(fwrite(raw,1,stride,f)==stride);assert(fwrite(raw,1,stride,f)==stride);assert(fseek(f,17,SEEK_SET)==0);
 LekakDiscImage d;assert(LekakDiscImage_Init(&d,f,stride));assert(ftell(f)==17);assert(d.sectors==2);
 assert(LekakDiscImage_Read(&d,1,out));assert(!memcmp(out,raw+offset,2048));assert(d.reads==1);
 memcpy(before,out,2048);assert(!LekakDiscImage_Read(&d,2,out));assert(!memcmp(before,out,2048));
 if(stride==2352){
  for(unsigned kind=0;kind<3;kind++){
   uint8_t bad[2352];memcpy(bad,raw,stride);
   if(kind==0)bad[15]=1;
   if(kind==1)bad[18]=bad[22]=0x20;
   if(kind==2)bad[20]=1;
   assert(!fseek(f,0,SEEK_SET));assert(fwrite(bad,1,stride,f)==stride);assert(!fflush(f));
   assert(!LekakDiscImage_Read(&d,0,out));assert(!memcmp(before,out,2048));
  }
 }
 fclose(f);
 f=tmpfile();assert(f);fputc(0,f);assert(!LekakDiscImage_Init(&d,f,stride));fclose(f);
}
int main(void){
 image_tests(2048);image_tests(2352);
 FILE *f=tmpfile();assert(f);uint8_t zero[2048]={0};assert(fwrite(zero,1,sizeof(zero),f)==sizeof(zero));
 LekakDiscImage image;assert(LekakDiscImage_Init(&image,f,2048));
 LekakCdRegisters c={0};uint8_t untouched=0xa5;
 assert(LekakCd_Write(&c,0x1f801801,1)==-1); /* no mounted medium */
 c.image=&image;assert(read_cd(&c,0)==0x18);
 assert(LekakCd_Read(&c,0x1f801801,&untouched)==-1&&untouched==0xa5);
 write_cd(&c,1,1);assert(read_cd(&c,0)&0x80);cycles(&c,1999);assert(!c.flags);
 cycles(&c,1);assert(c.flags==3&&c.acks==1&&LekakCd_Irq(&c)==0);
 write_cd(&c,0,1);write_cd(&c,2,7);assert(LekakCd_Irq(&c)==4);assert(read_cd(&c,3)==0xe3);
 assert(read_cd(&c,1)==2);assert(LekakCd_Read(&c,0x1f801801,&untouched)==-1);
 write_cd(&c,3,7);assert(c.flags==0&&LekakCd_Irq(&c)==0);
 write_cd(&c,0,0);write_cd(&c,1,0x0a);cycles(&c,2000);assert(c.flags==3&&c.second);
 cycles(&c,8000);assert(c.flags==3&&c.completions==0); /* no merged INT5 */
 write_cd(&c,0,1);write_cd(&c,3,7);cycles(&c,1999);assert(c.flags==0);
 cycles(&c,1);assert(c.flags==2&&c.completions==1&&read_cd(&c,1)==2);
 write_cd(&c,3,7);write_cd(&c,0,0);write_cd(&c,1,0x0b);cycles(&c,2000);assert(c.muted);
 write_cd(&c,0,1);write_cd(&c,3,7);write_cd(&c,0,0);write_cd(&c,1,0x0c);cycles(&c,2000);assert(!c.muted);
 write_cd(&c,0,1);write_cd(&c,3,7);write_cd(&c,0,0);
 assert(LekakCd_Write(&c,0x1f801801,6)==-1);assert(!c.busy); /* real ReadN still unsupported */
 for(unsigned i=0;i<16;i++)write_cd(&c,2,(uint8_t)i);
 assert(!(read_cd(&c,0)&0x10));assert(LekakCd_Write(&c,0x1f801802,17)==-1);
 assert(LekakCd_Write(&c,0x1f801801,1)==-1);write_cd(&c,0,1);write_cd(&c,3,0x47);assert(!c.param_count);
 assert(LekakCd_Write(&c,0x1f801803,0x80)==-1);assert(LekakCd_Read(&c,0x1f801802,&untouched)==-1);
 assert(LekakCd_Read(&c,0x1f801804,&untouched)==0);
 /* Actual interpreter MMIO dispatch, load delay, clock and boot context. */
 MemoriesMemory *m=calloc(1,sizeof(*m));assert(m);uint32_t program[]={
  0x3c081f80,0x35081800,0x24090001,0xa1090001, /* command Nop */
  0x24090bb8,0x2529ffff,0x1520fffe,0, /* wait 3000 iterations */
  0x24090001,0xa1090000,0x91020003,0,0x03e00008,0};
 for(unsigned i=0;i<sizeof(program)/sizeof(*program);i++)Memories_WriteLE32(m->ram+0x10000+i*4,program[i]);
 LekakExecResult r;assert(LekakExec_RunWithDisc(m,0x80010000,0,0x801fff00,12000,&r,NULL,&image));
 assert(r.registers[2]==0xe3&&r.cd_commands==1&&r.cd_acks==1&&r.irq_status==4);
 assert(!LekakExec_Run(m,0x80010000,0,0x801fff00,12000,&r));assert(strstr(r.reason,"CD register write"));
 uint32_t config[]={0x3c081f80,0x24091325,0xad091020,0x8d021020,0,
  0x24091234,0xa5091db0,0x95031db0,0,0xa5091d80,0x95041d80,0,0x03e00008,0};
 for(unsigned i=0;i<sizeof(config)/sizeof(*config);i++)Memories_WriteLE32(m->ram+0x10000+i*4,config[i]);
 assert(LekakExec_Run(m,0x80010000,0,0x801fff00,100,&r));
 assert(r.registers[2]==0x1325&&r.registers[3]==0x1234&&r.registers[4]==0x1234);
 Memories_WriteLE32(m->ram+0x10004,0x24091326);
 assert(!LekakExec_Run(m,0x80010000,0,0x801fff00,100,&r));assert(strstr(r.reason,"COMMON_DELAY"));
 Memories_WriteLE32(m->ram+0x10004,0x24091325);
 Memories_WriteLE32(m->ram+0x10014,0x34098000);
 assert(!LekakExec_Run(m,0x80010000,0,0x801fff00,100,&r));assert(strstr(r.reason,"SPU write"));
 free(m);fclose(f);puts("CD indexed registers and image reader passed");return 0;
}
