#include "../App/audio_ring.h"
#include "../App/guest_exec.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <sched.h>
static void *consume(void *context){
 LekakAudioRing *r=context;
 for(unsigned i=0;i<500000;i++){
 int16_t pair[2];while(!LekakAudioRing_Pop(r,pair))sched_yield();
 assert(pair[0]==(int16_t)i&&pair[1]==(int16_t)~i);
 }return NULL;
}
int main(void){
 LekakAudioRing *r=malloc(sizeof(*r));assert(r);LekakAudioRing_Init(r);int16_t pair[2]={1,1};
 assert(!LekakAudioRing_Pop(r,pair)&&pair[0]==0&&pair[1]==0);
 for(unsigned i=0;i<LEKAK_AUDIO_CAPACITY;i++)assert(LekakAudioRing_Push(r,(int16_t)i,(int16_t)-i));
 assert(!LekakAudioRing_Push(r,123,456)&&atomic_load(&r->dropped)==1);
 for(unsigned i=0;i<LEKAK_AUDIO_CAPACITY;i++){assert(LekakAudioRing_Pop(r,pair));assert(pair[0]==(int16_t)i&&pair[1]==(int16_t)-i);}
 assert(!LekakAudioRing_Pop(r,pair));
 atomic_store(&r->read_position,0xfffffff0u);atomic_store(&r->write_position,0xfffffff0u);
 for(unsigned i=0;i<64;i++)assert(LekakAudioRing_Push(r,(int16_t)(i+100),(int16_t)(i-100)));
 for(unsigned i=0;i<64;i++){assert(LekakAudioRing_Pop(r,pair));assert(pair[0]==(int16_t)(i+100)&&pair[1]==(int16_t)(i-100));}
 assert(atomic_is_lock_free(&r->write_position)&&atomic_is_lock_free(&r->read_position));
 pthread_t thread;assert(!pthread_create(&thread,NULL,consume,r));
 for(unsigned i=0;i<500000;i++)while(!LekakAudioRing_Push(r,(int16_t)i,(int16_t)~i))sched_yield();
 assert(!pthread_join(thread,NULL));assert(!LekakAudioRing_Pop(r,pair));
 MemoriesMemory *m=calloc(1,sizeof(*m));assert(m);Memories_WriteLE32(m->ram+0x1000,0x1000ffff);
 LekakExecSession *s=LekakExec_Create(m,0x80001000,0,0x801fff00,NULL,NULL);assert(s);
 LekakExec_SetAudioSink(s,LekakAudioRing_Sink,r);LekakExecResult out;
 assert(LekakExec_Slice(s,10000,&out)==LEKAK_EXEC_YIELDED);
 unsigned ticks=0;while(LekakAudioRing_Pop(r,pair)){assert(pair[0]==0&&pair[1]==0);ticks++;}
 assert(ticks==out.spu_ticks&&ticks>0);
 LekakExec_SetAudioSink(s,NULL,NULL);assert(LekakExec_Slice(s,10000,&out)==LEKAK_EXEC_YIELDED);
 assert(!LekakAudioRing_Pop(r,pair));LekakExec_Destroy(s);free(m);free(r);
 puts("Audio ring: stereo order, wrap, 500000-frame threaded handoff, full/empty and actual session cadence/silence passed");return 0;
}
