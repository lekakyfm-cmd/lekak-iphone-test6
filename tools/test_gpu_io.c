#include "../App/gpu_io.h"
#include "pc/render/soft_gpu.h"
#include <assert.h>
#include <stdio.h>
int main(void){
 LekakGpuIo g;uint32_t value;LekakGpuIo_Init(&g);
 assert(LekakGpuIo_Read(&g,0xbf801814,&value)&&value==0x14802000);
 assert(!LekakGpuIo_Read(&g,0x1f801800,&value));
 assert(LekakGpuIo_Write(&g,0x1f801814,0x03000000)==1);
 assert(!(g.status&0x800000));
 assert(LekakGpuIo_Write(&g,0x1f801814,0x08000010)==1&&(g.status&0x200000u));
 assert(LekakGpuIo_Write(&g,0x1f801814,0x08000020)==-1);
 assert(LekakGpuIo_Write(&g,0x1f801810,0xe2001234)==1);
 assert(LekakGpuIo_Write(&g,0x1f801814,0x10000002)==1);
 assert(LekakGpuIo_Read(&g,0x1f801810,&value)&&value==0x1234);
 assert(LekakGpuIo_Write(&g,0x1f801814,0x10000006)==1&&g.read_latch==0x1234);
 /* Fixed-size fill only executes once the complete packet arrives. */
 assert(LekakGpuIo_Write(&g,0x1f801810,0x020000ff)==1);
 assert(LekakGpuIo_Write(&g,0x1f801810,(20u<<16)|16u)==1);
 assert(SoftGpu_Vram()[20*1024+16]==0);
 assert(LekakGpuIo_Write(&g,0x1f801810,(1u<<16)|16u)==1);
 assert(SoftGpu_Vram()[20*1024+16]==31);
 assert(LekakGpuIo_Write(&g,0x1f801814,0)==1);
 assert(SoftGpu_Vram()[20*1024+16]==31&&g.status==0x14802000);
 assert(LekakGpuIo_Write(&g,0x1f801810,0x0200ff00)==1&&g.count==1);
 assert(LekakGpuIo_Write(&g,0x1f801814,0x01000000)==1&&g.count==0);
 /* Odd transfer wraps both axes; padding is not another command/pixel. */
 assert(LekakGpuIo_Write(&g,0x1f801810,0xa0000000)==1);
 assert(LekakGpuIo_Write(&g,0x1f801810,0x01ff03ff)==1);
 assert(LekakGpuIo_Write(&g,0x1f801810,0x00010003)==1);
 assert(LekakGpuIo_Write(&g,0x1f801810,0x22221111)==1);
 assert(LekakGpuIo_Write(&g,0x1f801810,0xdead3333)==1);
 assert(!g.transfer_mode&&SoftGpu_Vram()[511*1024+1023]==0x1111);
 assert(SoftGpu_Vram()[511*1024]==0x2222&&SoftGpu_Vram()[511*1024+1]==0x3333);
 assert(LekakGpuIo_Write(&g,0x1f801810,0xc0000000)==1);
 assert(LekakGpuIo_Write(&g,0x1f801810,0x01ff03ff)==1);
 assert(LekakGpuIo_Write(&g,0x1f801810,0x00010003)==1);
 assert(g.status&0x08000000);
 assert(LekakGpuIo_Read(&g,0x1f801810,&value)&&value==0x22221111);
 assert(LekakGpuIo_Read(&g,0x1f801810,&value)&&value==0x3333);
 assert(!(g.status&0x08000000));
 /* A row after y511 wraps to y0 independently of horizontal wrapping. */
 const uint32_t vertical[]={0xa0000000,0x01ff0005,0x00020001,0x55554444};
 for(unsigned i=0;i<4;i++)assert(LekakGpuIo_Write(&g,0x1f801810,vertical[i])==1);
 assert(SoftGpu_Vram()[511*1024+5]==0x4444&&SoftGpu_Vram()[5]==0x5555);
 /* E6 force/check mask applies to uploads through the actual rasterizer. */
 assert(LekakGpuIo_Write(&g,0x1f801810,0xe6000001)==1);
 const uint32_t upload[]={0xa0000000,0,0x00010001,0x1234};
 for(unsigned i=0;i<4;i++)assert(LekakGpuIo_Write(&g,0x1f801810,upload[i])==1);
 assert(SoftGpu_Vram()[0]==0x9234);
 assert(LekakGpuIo_Write(&g,0x1f801810,0xe6000002)==1);
 for(unsigned i=0;i<4;i++)assert(LekakGpuIo_Write(&g,0x1f801810,i==3?0x5678:upload[i])==1);
 assert(SoftGpu_Vram()[0]==0x9234);
 /* Zero dimensions mean full 1024x512, and GP1 reset aborts transfer. */
 for(unsigned i=0;i<3;i++)assert(LekakGpuIo_Write(&g,0x1f801810,i?0:0xa0000000)==1);
 assert(g.transfer_pixels==524288);
 assert(LekakGpuIo_Write(&g,0x1f801814,0x01000000)==1&&!g.transfer_mode);
 assert(LekakGpuIo_Write(&g,0x1f801810,0xa1000000)==-1);
 assert(LekakGpuIo_Write(&g,0x1f801814,0x04000002)==1&&(g.status&0x62000000)==0x42000000);
 /* Real New Game UI uses variable-length flat polylines. Draw only after
  * a valid terminator; the following command must remain a separate packet. */
 LekakGpuIo_Init(&g);
 assert(LekakGpuIo_Write(&g,0x1f801810,0xe407ffff)==1);
 const uint32_t flat[]={0x4800ff00,0x00050005,0x00050008,0x00080008,0x55555555,0xe1000000};
 assert(LekakGpuIo_ValidateDrawing(flat,6)==MEMORIES_GPU_OK);
 for(unsigned i=0;i<4;i++)assert(LekakGpuIo_Write(&g,0x1f801810,flat[i])==1);
 assert(SoftGpu_Vram()[5*1024+6]==0&&g.count==4);
 assert(LekakGpuIo_Write(&g,0x1f801810,flat[4])==1&&!g.count);
 assert(SoftGpu_Vram()[5*1024+6]==0x3e0&&SoftGpu_Vram()[7*1024+8]==0x3e0);
 assert(LekakGpuIo_Write(&g,0x1f801810,flat[5])==1&&!g.count);
 const uint32_t shaded[]={0x58ffffff,0x000a0005,0xffffff,0x000a0008,0xffffff,0x000d0008,0x50005000};
 assert(LekakGpuIo_ValidateDrawing(shaded,7)==MEMORIES_GPU_OK);
 for(unsigned i=0;i<7;i++)assert(LekakGpuIo_Write(&g,0x1f801810,shaded[i])==1);
 assert(SoftGpu_Vram()[10*1024+6]==0x7fff&&!g.count);
 assert(LekakGpuIo_ValidateDrawing(flat,4)==MEMORIES_GPU_TRUNCATED_COMMAND);
 assert(LekakGpuIo_ValidateDrawing(shaded,6)==MEMORIES_GPU_TRUNCATED_COMMAND);
 /* A Gouraud terminator is a color-position boundary, never a coordinate. */
 const uint32_t coordinate[]={0x580000ff,0,0xff,0x50005000,0xff,0,0x55555555};
 assert(LekakGpuIo_ValidateDrawing(coordinate,7)==MEMORIES_GPU_OK);
 uint32_t large[LEKAK_GPU_PACKET_WORDS+1]={0};large[0]=0x480000ff;
 large[LEKAK_GPU_PACKET_WORDS-1]=0x50005000;
 assert(LekakGpuIo_ValidateDrawing(large,LEKAK_GPU_PACKET_WORDS)==MEMORIES_GPU_OK);
 large[LEKAK_GPU_PACKET_WORDS-1]=0;large[LEKAK_GPU_PACKET_WORDS]=0x50005000;
 assert(LekakGpuIo_ValidateDrawing(large,LEKAK_GPU_PACKET_WORDS+1)==MEMORIES_GPU_CAPACITY);
 for(unsigned i=0;i<LEKAK_GPU_PACKET_WORDS;i++)assert(LekakGpuIo_Write(&g,0x1f801810,large[i])==1);
 assert(LekakGpuIo_Write(&g,0x1f801810,large[LEKAK_GPU_PACKET_WORDS])==-1);
 assert(g.count==LEKAK_GPU_PACKET_WORDS);
 assert(LekakGpuIo_Write(&g,0x1f801814,0x01000000)==1&&!g.count);
 puts("GPU CPU ports: packet assembly, actual rasterization, reset preservation, info latch, configuration and image transfers passed");
 return 0;
}
