#include "../App/mdec.h"
#include "../App/guest_exec.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
static void write(LekakMdec *m,uint32_t v){assert(LekakMdec_Write(m,0x1f801820u,v)==1);}
static void setup(LekakMdec *m){
 LekakMdec_Init(m);assert(LekakMdec_Status(m)==0x80040000u);
 assert(LekakMdec_Write(m,0x1f801824u,0x60000000u)==1);
 write(m,0x40000001u);for(unsigned i=0;i<32;i++)write(m,0x02020202u);
 write(m,0x60000000u);
 /* DC-only scale matrix supplies an independent constant-block oracle. */
 for(unsigned i=0;i<32;i++)write(m,i<4?0x5a825a82u:0u);
}
static void picture(LekakMdec *m,uint32_t command,const int dc[6]){
 write(m,command);for(unsigned i=0;i<6;i++)write(m,0xfe000400u|((unsigned)dc[i]&1023u));
}
static uint16_t p16(const uint8_t *p){return p[0]|((uint16_t)p[1]<<8);}
static uint32_t code[256];static unsigned used;
static void emit(uint32_t v){assert(used<256);code[used++]=v;}
static void store(unsigned offset,uint32_t v){emit(0x3c090000u|(v>>16));emit(0x35290000u|(v&0xffffu));emit(0xad090000u|offset);}
static void dma(unsigned channel,uint32_t address,uint32_t bcr,uint32_t control){unsigned at=0x1080+channel*16;store(at,address);store(at+4,bcr);store(at+8,control);}
static void install(MemoriesMemory *ram){
 memset(ram,0,sizeof(*ram));for(unsigned i=0;i<used;i++)Memories_WriteLE32(ram->ram+0x1000+i*4,code[i]);
 for(unsigned i=0;i<32;i++){Memories_WriteLE32(ram->ram+0x300+i*4,0x02020202u);Memories_WriteLE32(ram->ram+0x400+i*4,i<4?0x5a825a82u:0);}
 const int dc[6]={0,0,0,128,256,-256};for(unsigned i=0;i<6;i++)Memories_WriteLE32(ram->ram+0x500+i*4,0xfe000400u|((unsigned)dc[i]&1023u));
 memset(ram->ram+0x600,0xa5,512);
}
int main(void){
 LekakMdec *m=malloc(sizeof(*m));assert(m);uint8_t out[768];uint32_t value;
 setup(m);assert(m->quant[0]==2&&m->quant[127]==2&&m->scale[0]==0x5a82);
 const int dc[6]={0,0,0,128,256,-256};picture(m,0x38000006u,dc);
 assert(LekakMdec_DmaRead(m,out,128)==1&&m->macroblocks==1);
 /* Each quadrant's DC level survives DMA's 8x8-to-16x16 ordering. */
 const uint16_t colors[4]={0x4210,0x5294,0x6318,0x2108};
 for(unsigned y=0;y<16;y++)for(unsigned x=0;x<16;x++)assert(p16(out+(y*16+x)*2)==colors[(y/8)*2+x/8]);
 assert(m->input_position==m->input_count);
 assert(LekakMdec_Write(m,0x1f801824u,0xe0000000u)==1);
 assert(m->quant[0]==2&&m->scale[0]==0x5a82&&LekakMdec_Status(m)==0x80040000u);
 const int zero[6]={0};picture(m,0x3a000006u,zero);
 assert(LekakMdec_Read(m,0x1f801820u,&value)==1&&value==0xc210c210u);
 assert(LekakMdec_Write(m,0x1f801824u,0xe0000000u)==1);
 picture(m,0x30000006u,zero);assert(LekakMdec_DmaRead(m,out,192)==1);
 for(unsigned i=0;i<768;i++)assert(out[i]==128);
 assert(LekakMdec_Write(m,0x1f801824u,0xe0000000u)==1);
 picture(m,0x34000006u,zero);assert(LekakMdec_DmaRead(m,out,192)==1);
 for(unsigned i=0;i<768;i++)assert(out[i]==0);
 /* Actual monochrome 8/4bit outputs, unsigned and signed. */
 write(m,0x28000001u);write(m,0xfe000400u);assert(LekakMdec_DmaRead(m,out,16)==1);
 for(unsigned i=0;i<64;i++)assert(out[i]==128);
 write(m,0x20000001u);write(m,0xfe000400u);assert(LekakMdec_DmaRead(m,out,8)==1);
 for(unsigned i=0;i<32;i++)assert(out[i]==0x88);
 /* Disabled/incomplete requests wait; malformed block leaves output/state. */
 write(m,0x38000006u);memset(out,0xa5,sizeof(out));
 assert(LekakMdec_DmaRead(m,out,128)==0&&out[0]==0xa5);
 for(unsigned i=0;i<6;i++)write(m,0);
 unsigned at=m->input_position;
 assert(LekakMdec_DmaRead(m,out,128)==-1&&out[0]==0xa5&&m->input_position==at);
 assert(LekakMdec_Write(m,0x1f801824u,0x80000000u)==1);
 write(m,0x40000000u);uint8_t tables[64];memset(tables,9,sizeof(tables));
 assert(LekakMdec_DmaWrite(m,tables,16)==0&&m->remaining==16&&m->quant[0]==2);
 assert(LekakMdec_Write(m,0x1f801824u,0x40000000u)==1);
 assert(LekakMdec_DmaWrite(m,tables,17)==-1&&m->quant[0]==2);
 assert(LekakMdec_DmaWrite(m,tables,16)==1&&m->quant[0]==9&&m->quant[63]==9&&m->quant[64]==2);
 assert(LekakMdec_Write(m,0x1f801820u,0xe0000000u)==-1);
 assert(LekakMdec_Write(m,0x1f801824u,0x60000000u)==1);
 /* Custom scale tables must affect AC coefficients too; q=0 uses linear
  * coefficient order. Identity/4 per pass gives independently known pixels. */
 write(m,0x60000000u);
 for(unsigned i=0;i<32;i++){unsigned lo=2*i,hi=lo+1;write(m,((lo/8==lo%8)?0x4000u:0)|((hi/8==hi%8)?0x40000000u:0));}
 write(m,0x28000002u);write(m,0x00100008u);write(m,0xfe00fe00u);
 assert(LekakMdec_DmaRead(m,out,16)==1&&out[0]==129&&out[1]==130&&out[2]==128);
 /* Actual MIPS MMIO/DMA0 input and reordered DMA1 output, not direct API only. */
 MemoriesMemory *ram=calloc(1,sizeof(*ram));assert(ram);LekakExecResult result;
 emit(0x3c081f80u);store(0x1824,0x60000000u);store(0x10f0,0x88);store(0x10f4,0x00830000u);
 store(0x1820,0x40000001u);dma(0,0x300,0x00010020u,0x01000201u);
 store(0x1820,0x60000000u);dma(0,0x400,0x00010020u,0x01000201u);
 store(0x1820,0x38000006u);dma(0,0x500,0x00010006u,0x01000201u);
 unsigned output_program=used;dma(1,0x600,0x00040020u,0x01000200u);emit(0x8d0a1098u);emit(0);emit(0x03e00008u);emit(0);
 install(ram);assert(LekakExec_Run(ram,0x80001000u,0,0x801fff00u,1000,&result));
 assert(result.mdec_dma_transfers[0]==3&&result.mdec_dma_words[0]==70);
 assert(result.mdec_dma_transfers[1]==1&&result.mdec_dma_words[1]==128&&result.mdec_macroblocks==1);
 assert(result.registers[10]==0x200&&result.dma_interrupt==0x83830000u&&(result.irq_status&8));
 for(unsigned y=0;y<16;y++)for(unsigned x=0;x<16;x++)assert(p16(ram->ram+0x600+(y*16+x)*2)==colors[(y/8)*2+x/8]);
 uint32_t budget=result.steps-127;install(ram);
 assert(!LekakExec_Run(ram,0x80001000u,0,0x801fff00u,budget,&result)&&strstr(result.reason,"MDEC DMA exceeds diagnostic budget"));
 for(unsigned i=0;i<512;i++)assert(ram->ram[0x600+i]==0xa5);
 /* Full destination span validated before either decoding or writing bytes. */
 install(ram);Memories_WriteLE32(ram->ram+0x1000+output_program*4,0x3c09001fu);Memories_WriteLE32(ram->ram+0x1000+(output_program+1)*4,0x3529fffcu);
 ram->ram[0x1ffffc]=0xa5;
 assert(!LekakExec_Run(ram,0x80001000u,0,0x801fff00u,1000,&result)&&strstr(result.reason,"memory"));
 assert(result.mdec_dma_transfers[1]==0&&ram->ram[0x1ffffc]==0xa5);free(ram);
 free(m);puts("MDEC actual tables, reset retention, DC/RGB/quadrant ordering, signed/mono output and transactional bounds/wait/malformed-data checks passed");return 0;
}
