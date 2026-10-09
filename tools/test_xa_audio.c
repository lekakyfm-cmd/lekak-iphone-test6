#include "../App/xa_audio.h"
#include "../App/spu_config.h"
#include "../App/cd_registers.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
int main(void){
 LekakXaAudio *s=calloc(1,sizeof(*s));assert(s);uint8_t raw[2340]={0};int16_t frame[2];
 raw[7]=1;
 for(unsigned g=0;g<18;g++){
  uint8_t *group=raw+12+g*128;
  for(unsigned u=0;u<8;u++)group[4+u]=12;
  memset(group+16,0x21,112);
 }
 assert(LekakXa_Decode(s,raw)==1&&s->sectors==1&&s->count==2352&&s->frames==2352);
 for(unsigned i=0;i<2352;i++){LekakXa_Frame(s,frame);assert(frame[0]==1&&frame[1]==2);}
 LekakXa_Frame(s,frame);assert(frame[0]==0&&frame[1]==0&&s->consumed==2352);
 raw[7]=5;assert(LekakXa_Decode(s,raw)==1&&s->count==4704);
 for(unsigned i=0;i<4704;i++){LekakXa_Frame(s,frame);assert(frame[0]==1&&frame[1]==2);}
 raw[7]=0;assert(LekakXa_Decode(s,raw)==1&&s->count==4704);
 for(unsigned i=0;i<28*7/6;i++){LekakXa_Frame(s,frame);assert(frame[0]==1&&frame[1]==1);}
 uint32_t count=s->count,sectors=s->sectors;raw[7]=0x11;
 assert(LekakXa_Decode(s,raw)==-1&&s->count==count&&s->sectors==sectors);
 raw[7]=0;s->count=LEKAK_XA_QUEUE-1;int old=s->history[0][0];
 assert(LekakXa_Decode(s,raw)==0&&s->count==LEKAK_XA_QUEUE-1&&s->history[0][0]==old);
 LekakSpuConfig *spu=calloc(1,sizeof(*spu));assert(spu);
 spu->control=0xc001;spu->main_gain[0]=spu->main_gain[1]=0x3fff;spu->cd_gain[0]=spu->cd_gain[1]=0x7fff;
 spu->dsp.cd_input[0]=4096;spu->dsp.cd_input[1]=-4096;
 for(unsigned i=0;i<768;i++)LekakSpu_Cycle(spu);
 assert(spu->dsp.last[0]==4094&&spu->dsp.last[1]==-4096&&spu->dsp.nonzero_frames==1);
 spu->control=0xc000;for(unsigned i=0;i<768;i++)LekakSpu_Cycle(spu);
 assert(spu->dsp.last[0]==0&&spu->dsp.last[1]==0);
 /* Register staging/apply, stereo routing and mute consume real queued PCM. */
 LekakCdRegisters *cd=calloc(1,sizeof(*cd));assert(cd);
 cd->xa.count=3;for(unsigned i=0;i<3;i++){cd->xa.pcm[i][0]=1000;cd->xa.pcm[i][1]=-2000;}
 assert(LekakCd_Write(cd,0x1f801800,2)==1);
 assert(LekakCd_Write(cd,0x1f801802,128)==1); /* L->L */
 assert(LekakCd_Write(cd,0x1f801803,64)==1); /* L->R */
 assert(LekakCd_Write(cd,0x1f801800,3)==1);
 assert(LekakCd_Write(cd,0x1f801801,128)==1); /* R->R */
 assert(LekakCd_Write(cd,0x1f801802,32)==1); /* R->L */
 LekakCd_AudioFrame(cd,frame);assert(frame[0]==0&&frame[1]==0);
 assert(LekakCd_Write(cd,0x1f801803,32)==1);
 LekakCd_AudioFrame(cd,frame);assert(frame[0]==500&&frame[1]==-1500);
 cd->muted=1;LekakCd_AudioFrame(cd,frame);assert(frame[0]==0&&frame[1]==0&&cd->xa.consumed==3);free(cd);
 free(spu);free(s);puts("XA4-bit stereo/mono,37800/18900 conversion, FIFO bounds/underflow, rejected8-bit data and actual CD/SPU gain gating passed");return 0;
}
