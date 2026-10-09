/* Local reproduction using the same disc loader and executor as the iPhone.
 * Never bundle the user-supplied disc in a source archive. */
#include "../App/disc_loader.h"
#include "../App/guest_exec.h"
#include <stdlib.h>
#include "pc/render/soft_gpu.h"
#include "../App/display_snapshot.h"
#include <stdio.h>
int main(int argc,char **argv){
 if(argc<2||argc>3){fprintf(stderr,"Usage: run-disc IMAGE [instruction-budget]\n");return 2;}
 FILE *f=fopen(argv[1],"rb");if(!f){perror("disc");return 2;}
 MemoriesMemory *m=calloc(1,sizeof(*m));if(!m){fclose(f);return 2;}
 LekakDiscResult d;
 if(!LekakDisc_Load(f,m,&d)){fprintf(stderr,"Load failed: %s\n",d.error);fclose(f);free(m);return 1;}
 LekakDiscImage image;if(!LekakDiscImage_Init(&image,f,d.sector_bytes)){fclose(f);free(m);return 1;}
 LekakExecResult r;
 uint32_t budget=argc==3?(uint32_t)strtoul(argv[2],NULL,10):1000000u;
 LekakExecPads pads={0};const char *press=getenv("LEKAK_HOST_PRESS_FRAME");
 if(press){pads.connected[0]=1;pads.pressed[0]=8;pads.press_frame=(uint32_t)strtoul(press,NULL,10);pads.release_frame=pads.press_frame+4;}
 const char *buttons=getenv("LEKAK_HOST_PRESS_BUTTONS");if(buttons&&press)pads.pressed[0]=(uint16_t)strtoul(buttons,NULL,0);
 const char *repeat=getenv("LEKAK_HOST_PRESS_REPEAT");if(repeat)pads.repeat_frames=(uint32_t)strtoul(repeat,NULL,10);
 LekakExec_RunWithDisc(m,d.entry,d.gp?d.gp:0x8009af08u,d.stack_base?d.stack_base+d.stack_bytes:0x801fff00u,budget,&r,press?&pads:NULL,&image);
 fclose(f);
 printf("hash=%08X entry=%08X loaded=%u steps=%u pc=%08X detail=%08X reason=%s\n",d.payload_hash,d.entry,d.load_bytes,r.steps,r.pc,r.detail,r.reason);
 printf("CD sectors/dataIRQs/bytes=%u/%u/%u nextLBA=%u DMA transfers/words=%u/%u\n",r.cd_sectors_decoded,r.cd_data_irqs,r.cd_data_bytes,r.cd_next_lba,r.cd_dma_transfers,r.cd_dma_words);
 printf("SPU reads/writes=%u/%u control/status=%04X/%04X main/current/CD L=%04X/%04X/%04X R=%04X/%04X/%04X ticks=%u\n",r.spu_reads,r.spu_writes,r.spu_control,r.spu_status,r.spu_main_gain[0],r.spu_current_gain[0],r.spu_cd_gain[0],r.spu_main_gain[1],r.spu_current_gain[1],r.spu_cd_gain[1],r.spu_ticks);
 printf("SPU manualHalfwords/frames/nonzero/hash=%u/%u/%u/%08X\n",r.spu_manual_halfwords,r.spu_frames,r.spu_nonzero_frames,r.spu_pcm_hash);
 printf("CD stream/decoded/fifoPos/fifoSize/command=%u/%u/%u/%u/%02X\n",r.cd_streaming,r.cd_decoded_ready,r.cd_fifo_pos,r.cd_fifo_size,r.cd_last_command);
 printf("SPU DMA4 transfers/words=%u/%u event callbacks=%u\n",r.spu_dma_transfers,r.spu_dma_words,r.bios_event_callbacks);
 printf("CD registers reads/writes=%u/%u commands/ACKs/completions=%u/%u/%u flags=%u enable=%u\n",r.cd_reads,r.cd_writes,r.cd_commands,r.cd_acks,r.cd_completions,r.cd_flags,r.cd_enable);
 printf("critical=%u/%u IRQ reads=%u writes=%u status=%08X mask=%08X\n",r.critical_enters,r.critical_exits,r.irq_reads,r.irq_writes,r.irq_status,r.irq_mask);
 printf("CD removals=%u attached=%d BIOS calls=%u GPU GP0=%u GP1=%u reads=%u\n",r.cd_driver_removals,r.cd_iso_driver_registered,r.bios_calls,r.gp0_commands,r.gp1_commands,r.gpu_reads);
 printf("nominal cycles=%llu lines=%u frames=%u HBlank=%u VBlank=%u timers=%u/%u/%u hook=%08X chains=%08X/%08X/%08X/%08X\n",(unsigned long long)r.nominal_cycles,r.video_lines,r.video_frames,r.hblank_edges,r.vblank_edges,r.timer_counts[0],r.timer_counts[1],r.timer_counts[2],r.interrupt_hook,r.interrupt_chains[0],r.interrupt_chains[1],r.interrupt_chains[2],r.interrupt_chains[3]);
 printf("IRQ entries/returns=%u/%u GPU DMA transfers/words=%u/%u\n",r.irq_entries,r.irq_returns,r.gpu_dma_transfers,r.gpu_dma_words);
 printf("MDEC commands/blocks/status=%u/%u/%08X DMA0=%u/%u DMA1=%u/%u\n",r.mdec_commands,r.mdec_macroblocks,r.mdec_status,r.mdec_dma_transfers[0],r.mdec_dma_words[0],r.mdec_dma_transfers[1],r.mdec_dma_words[1]);
 printf("Native boot-check compatibility calls=%u\n",r.native_boot_check_calls);
 printf("XA sectors/frames/consumed/queued=%u/%u/%u/%u\n",r.xa_audio_sectors,r.xa_audio_frames,r.xa_audio_consumed,r.xa_audio_queued);
 printf("Pad init/start=%d/%d buffers=%08X/%08X initializations/polls=%u/%u chain calls=%u\n",r.pad_initialized,r.pad_started,r.pad_buffers[0],r.pad_buffers[1],r.pad_initializations,r.pad_polls,r.irq_chain_calls);
 printf("JOY control reads/writes=%u/%u card init/shared=%d/%d\n",r.joy_control_reads,r.joy_control_writes,r.card_initialized,r.card_pad_shared);
 printf("Card patches early/delay/info=%u/%u/%u running=%d backup registry=%d\n",r.bios_card_early_patches,r.bios_card_delay_patches,r.bios_card_info_patches,r.card_running,r.bu_driver_registered);
 printf("OTC transfers/words=%u/%u BIOS patches Cause/PadAck/PadError=%u/%u/%u SR=%08X reads/writes=%u/%u\n",r.otc_dma_transfers,r.otc_dma_words,r.bios_cause_patches,r.bios_pad_ack_patches,r.bios_pad_error_patches,r.cp0_sr,r.cp0_sr_reads,r.cp0_sr_writes);
 for(unsigned i=0;i<4;i++)if(r.interrupt_chains[i]){
  const uint8_t *p=Memories_Resolve(m,r.interrupt_chains[i],16,4);
  if(p)printf("chain%u next=%08X second=%08X first=%08X\n",i,Memories_ReadLE32(p),Memories_ReadLE32(p+4),Memories_ReadLE32(p+8));
 }
 if(r.console_bytes)printf("Game console%s:\n%.*s\n",r.console_truncated?" (truncated)":"",(int)r.console_bytes,r.console_output);
 for(unsigned i=0;i<32;i++)printf("r%u=%08X%s",i,r.registers[i],i%4==3?"\n":" ");
 unsigned n=r.trace_count<LEKAK_EXEC_TRACE?r.trace_count:LEKAK_EXEC_TRACE;
 unsigned first=r.trace_count>LEKAK_EXEC_TRACE?r.trace_count%LEKAK_EXEC_TRACE:0;
 for(unsigned i=0;i<n;i++){unsigned ix=(first+i)%LEKAK_EXEC_TRACE;printf("%08X %08X\n",r.trace_pc[ix],r.trace_ins[ix]);}
 if(getenv("LEKAK_HOST_DEBUG")){
  uint32_t state=Memories_ReadLE32(m->ram+0x9b45c);const uint8_t *sd=Memories_Resolve(m,state,0x50,4);
  if(sd)printf("SD state=%08X stage=%u flags=%04X sourceFlags=%02X\n",state,Memories_ReadLE32(sd+0x3c),(unsigned)(sd[0x40]|sd[0x41]<<8),sd[0x4a]);
  const uint16_t *pixels=SoftGpu_Vram();unsigned nonzero=0;
  for(unsigned i=0;i<1024*512;i++)if(pixels[i])nonzero++;
  printf("VRAM nonzero=%u\n",nonzero);
  printf("Display origin=%08X status=%08X ranges=%08X/%08X\n",r.gpu_display_start,r.gpu_status,r.gpu_display_horizontal,r.gpu_display_vertical);
  const char *frame_path=getenv("LEKAK_HOST_FRAME");
  if(frame_path){
   unsigned width=0,height=0;uint8_t *rgba=malloc(640*512*4);
   if(rgba&&LekakDisplay_CopyRGBA(pixels,r.gpu_status,r.gpu_display_start,r.gpu_display_vertical,rgba,640*512*4,&width,&height)){
    FILE *frame=fopen(frame_path,"wb");
    if(frame){fprintf(frame,"P6\n%u %u\n255\n",width,height);for(unsigned i=0;i<width*height;i++)fwrite(rgba+i*4,1,3,frame);fclose(frame);}
   }
   free(rgba);
  }
  uint32_t boot_hash=2166136261u;
  for(unsigned i=0;i<0x1800;i++)boot_hash=(boot_hash^m->ram[0x168000+i])*16777619u;
  printf("Boot module diagnostic hash=%08X\n",boot_hash);
  const uint8_t *stack=Memories_Resolve(m,r.registers[29],128,4);
  if(stack)for(unsigned i=0;i<32;i++)printf("stack+%02X=%08X\n",i*4,Memories_ReadLE32(stack+i*4));
 }
 free(m);return 0;
}
