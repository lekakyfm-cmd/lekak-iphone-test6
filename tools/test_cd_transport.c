#include "../App/cd_registers.h"
#include "../App/guest_exec.h"
#include <assert.h>
#include <stdlib.h>
static void tick(LekakCdRegisters *c,unsigned n){while(n--)assert(LekakCd_Cycle(c)==0);}
static void wr(LekakCdRegisters *c,unsigned p,uint8_t v){assert(LekakCd_Write(c,0x1f801800+p,v)==1);}
static uint8_t rd(LekakCdRegisters *c,unsigned p){uint8_t v;assert(LekakCd_Read(c,0x1f801800+p,&v)==1);return v;}
static void response(LekakCdRegisters *c,unsigned flag,const uint8_t *bytes,unsigned n){
 wr(c,0,1);assert((rd(c,3)&7u)==flag);for(unsigned i=0;i<n;i++)assert(rd(c,1)==bytes[i]);wr(c,3,7);wr(c,0,0);
}
static void cmd(LekakCdRegisters *c,uint8_t op,const uint8_t *params,unsigned n,const uint8_t *ack,unsigned ackn){
 wr(c,0,0);for(unsigned i=0;i<n;i++)wr(c,2,params[i]);wr(c,1,op);tick(c,2000);response(c,3,ack,ackn);
}
static uint32_t code[256];static unsigned count;
static void emit(uint32_t w){assert(count<256);code[count++]=w;}
static void set9(uint32_t v){emit(0x3c090000u|(v>>16));emit(0x35290000u|(v&0xffff));}
static void byte(unsigned port,uint8_t v){set9(v);emit(0xa1090000u|port);}
static void wait_response(void){
 byte(0,1);unsigned loop=count;emit(0x910a0003);emit(0);emit(0x314a0007);
 emit(0x11400000u|(uint16_t)((int)loop-(int)count-1));emit(0);
 emit(0x910a0001);emit(0);byte(3,7);byte(0,0);
}
static void guestcmd(uint8_t command,const uint8_t *args,unsigned n){byte(0,0);for(unsigned i=0;i<n;i++)byte(2,args[i]);byte(1,command);wait_response();}
static unsigned make_program_ex(unsigned words,uint32_t address,uint32_t control,uint8_t mode,int header){
 count=0;emit(0x3c081f80);emit(0x35081800);
 uint8_t loc[]={0,2,0};guestcmd(0x0e,&mode,1);guestcmd(2,loc,3);guestcmd(6,NULL,0);
 wait_response(); /* sector INT1, rather than command INT3 */
 byte(3,0x80); /* host data FIFO */
 emit(0x3c0b1f80);set9(0x0765c321);emit(0xad6910f0); /* enable DMA3 */
 if(header){
  set9(0x21000);emit(0xad6910b0);set9(3);emit(0xad6910b4);set9(0x11000000);emit(0xad6910b8);
  byte(3,0x80); /* SDK partial block reassertion must retain FIFO position. */
 }
 set9(address);emit(0xad6910b0);set9(words);emit(0xad6910b4);set9(control);emit(0xad6910b8);
 emit(0x03e00008);emit(0);return count;
}
static unsigned make_program(unsigned words,uint32_t address,uint32_t control){return make_program_ex(words,address,control,0x80,0);}
static void install(MemoriesMemory *m){memset(m,0,sizeof(*m));memset(m->ram+0x20000,0xa5,4096);for(unsigned i=0;i<count;i++)Memories_WriteLE32(m->ram+0x10000+i*4,code[i]);}
int main(void){
 FILE *f=tmpfile();assert(f);uint8_t sectors[4096];for(unsigned i=0;i<4096;i++)sectors[i]=(uint8_t)(i*13+i/2048);
 assert(fwrite(sectors,1,sizeof(sectors),f)==sizeof(sectors));LekakDiscImage image;assert(LekakDiscImage_Init(&image,f,2048));
 LekakCdRegisters c={0};c.image=&image;
 uint8_t ack[]={2},tn[]={2,1,1},td[]={2,0,2};
 cmd(&c,0x13,NULL,0,tn,3);uint8_t track=1;cmd(&c,0x14,&track,1,td,3);
 track=0;cmd(&c,0x14,&track,1,td,3); /* short image lead-out rounds to 00:02 */
 uint8_t bad=2;wr(&c,2,bad);LekakCdRegisters snapshot=c;
 assert(LekakCd_Write(&c,0x1f801801,0x14)==-1);assert(!memcmp(&c,&snapshot,sizeof(c)));wr(&c,0,1);wr(&c,3,0x47);wr(&c,0,0);
 uint8_t mode=0x80,loc[]={0,2,0};cmd(&c,0x0e,&mode,1,ack,1);
 uint8_t getparam[]={2,0x80,0,0,0};cmd(&c,0x0f,NULL,0,getparam,5);
 cmd(&c,2,loc,3,ack,1);uint8_t reading[]={0x22};cmd(&c,6,NULL,0,reading,1);
 tick(&c,225791);assert(c.sectors_decoded==0);tick(&c,1);assert(c.flags==1&&c.next_lba==1&&c.decoded_ready);
 wr(&c,3,0x80);response(&c,1,reading,1);assert(rd(&c,0)&0x40);
 uint32_t word;assert(LekakCd_ReadData(&c,1,&word)==1&&word==sectors[0]);
 wr(&c,3,0x80);assert(c.fifo_pos==1);
 assert(LekakCd_ReadData(&c,2,&word)==1&&word==(sectors[1]|(uint32_t)sectors[2]<<8));
 for(unsigned i=3;i<2047;i++)assert(rd(&c,2)==sectors[i]);
 word=0xdeadbeef;assert(LekakCd_ReadData(&c,2,&word)==-1&&word==0xdeadbeef&&c.fifo_pos==2047);
 assert(rd(&c,2)==sectors[2047]&&!(rd(&c,0)&0x40));
 tick(&c,225792);assert(c.sectors_decoded==2&&c.decoded_ready);
 wr(&c,3,0x80);response(&c,1,reading,1);assert(LekakCd_ReadData(&c,4,&word)==1&&word==Memories_ReadLE32(sectors+2048));
 wr(&c,3,0);assert(c.fifo_size==0);tick(&c,225791);assert(LekakCd_Cycle(&c)==-1); /* EOF never manufactures a sector */
 c.streaming=0;cmd(&c,9,NULL,0,ack,1);tick(&c,2000);response(&c,2,ack,1);
 /* Restart after an ACKed but unrequested decoded block: a new ReadN must
  * produce a fresh sector, not be blocked forever by the old decoder slot. */
 cmd(&c,2,loc,3,ack,1);cmd(&c,6,NULL,0,reading,1);
 tick(&c,225792);assert(c.decoded_ready);response(&c,1,reading,1);
 cmd(&c,9,NULL,0,ack,1);tick(&c,2000);response(&c,2,ack,1);
 uint8_t loc1[]={0,2,1};cmd(&c,2,loc1,3,ack,1);cmd(&c,6,NULL,0,reading,1);
 assert(!c.decoded_ready);tick(&c,225792);assert(c.decoded_ready&&c.next_lba==2);
 wr(&c,3,0x80);response(&c,1,reading,1);assert(rd(&c,2)==sectors[2048]);
 cmd(&c,9,NULL,0,ack,1);tick(&c,2000);response(&c,2,ack,1);wr(&c,3,0);
 /* Both seek variants complete at the requested data-track position;
  * ReadS returns actual subsequent bytes, never fabricated audio sectors. */
 cmd(&c,2,loc,3,ack,1);uint8_t seek[]={0x42};
 cmd(&c,0x16,NULL,0,seek,1);assert(!c.streaming&&c.next_lba==0);
 tick(&c,2000);response(&c,2,ack,1);
 cmd(&c,0x15,NULL,0,seek,1);tick(&c,2000);response(&c,2,ack,1);
 cmd(&c,0x1b,NULL,0,reading,1);tick(&c,225792);
 wr(&c,3,0x80);response(&c,1,reading,1);assert(rd(&c,2)==sectors[0]);
 cmd(&c,9,NULL,0,ack,1);tick(&c,2000);response(&c,2,ack,1);wr(&c,3,0);
 /* Invalid BCD and pre-gap locations must not modify pending command state. */
 uint8_t invalid[][3]={{0,1,0},{0,0x60,0},{0,2,0x75},{0xfa,2,0},{0,2,2}};
 for(unsigned i=0;i<5;i++){
  for(unsigned j=0;j<3;j++)wr(&c,2,invalid[i][j]);
  snapshot=c;
  assert(LekakCd_Write(&c,0x1f801801,2)==-1);assert(!memcmp(&c,&snapshot,sizeof(c)));
  wr(&c,0,1);wr(&c,3,0x47);wr(&c,0,0);
 }
 uint8_t xa=0x40;wr(&c,2,xa);assert(LekakCd_Write(&c,0x1f801801,0x0e)==-1);
 /* Real interpreter drives a sector into DMA3, including destination/budget
  * checks before any write or FIFO consumption. */
 MemoriesMemory *m=calloc(1,sizeof(*m));assert(m);LekakExecResult r;
 make_program(512,0x20000,0x11000000);install(m);
 assert(LekakExec_RunWithDisc(m,0x80010000,0,0x801fff00,1000000,&r,NULL,&image));
 assert(r.cd_dma_transfers==1&&r.cd_dma_words==512&&r.cd_data_bytes==2048&&r.cd_sectors_decoded==1);
 assert(!memcmp(m->ram+0x20000,sectors,2048)&&m->ram[0x20800]==0xa5);
 uint32_t budget=r.steps-510;install(m);
 assert(!LekakExec_RunWithDisc(m,0x80010000,0,0x801fff00,budget,&r,NULL,&image));assert(strstr(r.reason,"CD DMA exceeds diagnostic budget"));
 for(unsigned i=0;i<4096;i++)assert(m->ram[0x20000+i]==0xa5);
 make_program(513,0x20000,0x11000000);install(m);
 assert(!LekakExec_RunWithDisc(m,0x80010000,0,0x801fff00,1000000,&r,NULL,&image));assert(strstr(r.reason,"available sector FIFO"));
 for(unsigned i=0;i<4096;i++)assert(m->ram[0x20000+i]==0xa5);
 make_program(512,0x1ffffc,0x11000000);install(m);memset(m->ram+0x1ffffc,0xa5,4);
 assert(!LekakExec_RunWithDisc(m,0x80010000,0,0x801fff00,1000000,&r,NULL,&image));assert(r.detail==0x1ffffc&&m->ram[0x1ffffc]==0xa5);
 make_program(512,0x20000,0x11000200);install(m);
 assert(!LekakExec_RunWithDisc(m,0x80010000,0,0x801fff00,1000000,&r,NULL,&image));assert(strstr(r.reason,"CD DMA mode"));
 /* Real raw header + payload are split into the same 3-word/512-word pair
  * observed in the game; no reset or fabricated bytes between the blocks. */
 FILE *rawfile=tmpfile();assert(rawfile);uint8_t raw[2352]={0},rawout[2340];raw[15]=2;
 for(unsigned i=0;i<2048;i++)raw[24+i]=sectors[i];
 for(unsigned i=2072;i<2352;i++)raw[i]=(uint8_t)(i*7);
 assert(fwrite(raw,1,sizeof(raw),rawfile)==sizeof(raw));LekakDiscImage rawimage;assert(LekakDiscImage_Init(&rawimage,rawfile,2352));
 assert(LekakDiscImage_ReadData(&rawimage,0,rawout,2340));assert(!memcmp(rawout,raw+12,2340));
 make_program_ex(512,0x20000,0x11000000,0xa0,1);install(m);
 assert(LekakExec_RunWithDisc(m,0x80010000,0,0x801fff00,1000000,&r,NULL,&rawimage));
 assert(r.cd_dma_transfers==2&&r.cd_dma_words==515&&r.cd_data_bytes==2060);
 assert(!memcmp(m->ram+0x21000,raw+12,12)&&!memcmp(m->ram+0x20000,sectors,2048));
 /* Raw Form2 sectors expose their real 2324-byte payload/tail in2340 mode.
  * The ISO/filesystem2048 reader must still reject Form2. */
 raw[18]=raw[22]=0x20;
 assert(!fseek(rawfile,0,SEEK_SET)&&fwrite(raw,1,sizeof(raw),rawfile)==sizeof(raw)&&!fflush(rawfile));
 assert(LekakDiscImage_ReadData(&rawimage,0,rawout,2340)&&!memcmp(rawout,raw+12,2340));
 assert(!LekakDiscImage_ReadData(&rawimage,0,rawout,2048));
 fclose(rawfile);free(m);fclose(f);puts("Image-backed sector stream, BCD/track queries, FIFO widths/EOF, DMA3 payload and atomic bounds/budget rejection passed");return 0;
}
