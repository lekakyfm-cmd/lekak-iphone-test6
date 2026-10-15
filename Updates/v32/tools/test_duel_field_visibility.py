"""Production GPU regression for the shared field primitive (normal after rarity).

Compile the exact field palette setup and the actual rarity page/palette code.
Reuse one texture-page word, as Duel_DrawFieldCards does for both players.
"""
import os,subprocess,tempfile
from pathlib import Path
from build_iphone_engine import ROOT,stage_sources
field=(ROOT/'SourceMod/Lekak/Rarities/field.c').read_text()
setup=field[field.index('    prim->clut ='):field.index('    setPolyGT4(prim);')]
r=(ROOT/'SourceMod/Lekak/Rarities/rarities.c').read_text()
colours=r[r.index('static const unsigned colours'):r.index('int Rarity_Kind(')]
recolour=r[r.index('static unsigned short recolour('):r.index('/* Bank CLUTs')]
palettes=r[r.index('static unsigned short palette_source'):r.index('static void (*original_packet)')]
base=(ROOT/'tools/test_rarity_gpu.c').read_text()
prefix=base[:base.index('int main(void)')]
prefix+=r'''
typedef unsigned short u16;
static int Rarity_Slot(int id){return id>=101&&id<=120?id-101:-1;}
typedef struct {int card_id;} DuelCardRecord;
typedef struct {unsigned short clut,tpage;} FieldPacket;
static void setup_packet(int id,int kind,unsigned short *page,unsigned short *clut){
 struct {struct {struct {unsigned short field_42;} h;} field_40;} object;
 __typeof__(object) *obj=&object;obj->field_40.h.field_42=native_frame[kind];
 DuelCardRecord card={id};void *record=&card;
 FieldPacket packet={.tpage=*page},*prim=&packet;
'''
suffix=r'''
 *page=prim->tpage;*clut=prim->clut;
}
static unsigned draw(unsigned short page,unsigned short clut,int back,int reverse){
 unsigned u=back?0x38:0,v=0x80;
 uint32_t q[]={0x3c808080,10|(12<<16),u|(v<<8)|((unsigned)clut<<16),
 0x00808080,14|(12<<16),(u+4)|(v<<8)|((unsigned)page<<16),
 0x00808080,10|(16<<16),u|((v+4)<<8),
 0x00808080,14|(16<<16),(u+4)|((v+4)<<8)};
 if(reverse){uint32_t a=q[1];q[1]=q[4];q[4]=a;a=q[7];q[7]=q[10];q[10]=a;}
 SoftGpu_Fill(10,12,4,4,0);assert(SoftGpu_Gp0(q,12)==12);
 return SoftGpu_Vram()[13*1024+11];
}
int main(void){
 assert(!GuestRuntime_Bind(&memory));SoftGpu_Reset();
 unsigned short *bank=SoftGpu_Bank(13);assert(bank);memset(bank,0,1024*512*2);
 unsigned short page_words[128*256];for(unsigned i=0;i<128*256;i++)page_words[i]=0x0101;
 SoftGpu_Load(896,256,128,256,page_words);
 const unsigned short native[]={0x001f,0x03e0,0x7c00,0x7c1f};
 for(int k=0;k<4;k++){unsigned short palette[256]={0};palette[1]=native[k];SoftGpu_Load(256,241+k,256,1,palette);}
 /* Reproduce the old defect: a normal CLUT is absent in private bank 13. */
 unsigned short page=0x9e,clut;setup_packet(101,0,&page,&clut);assert(draw(page,clut,0,0));
 clut=((241+native_frame[0])<<6)|16;
 assert(!draw(page,clut,0,0));
 puts("Old shared-page state reproduced: normal opponent card becomes transparent after variant");
 unsigned cases=0;
 for(int slot=0;slot<20;slot++)for(int kind=0;kind<5;kind++)for(int back=0;back<2;back++)for(int camera=0;camera<2;camera++){
  page=0x9e;
  for(int round=0;round<3;round++){
   setup_packet(101+slot,kind,&page,&clut);assert((page>>11&15)==13);
   assert(draw(page,clut,back,camera));
   /* Same page variable, next field record: normal card of either side. */
   setup_packet(1,kind,&page,&clut);assert((page>>11&15)==0);
   assert(draw(page,clut,back,camera)==native[native_frame[kind]]);
   /* Normal followed by normal must also keep its native palette. */
   setup_packet(2,kind,&page,&clut);assert(draw(page,clut,back,camera)==native[native_frame[kind]]);
  }
  cases++;
 }
 GuestRuntime_Reset();
 printf("%u mixed variant/normal cases passed: all 20 rarity/type slots, 5 native kinds, face/back UVs, both camera windings, 3 reuse cycles\n",cases);
}
'''
with tempfile.TemporaryDirectory(prefix='lekak-field-') as d:
 stage=Path(d);stage_sources(stage)
 test=stage/'field-test.c';test.write_text(prefix.split('static int Rarity_Slot')[0]+colours+recolour+palettes+'static int Rarity_Slot'+prefix.split('static int Rarity_Slot',1)[1]+setup+suffix)
 binary=stage/'field-test'
 subprocess.run([os.environ.get('CC','cc'),'-std=gnu11','-O2','-DMEMORIES_PC','-DMEMORIES_TRANSLATED','-DMEMORIES_IOS','-I'+str(stage/'src'),'-I'+str(ROOT/'tools'),str(test),str(stage/'src/pc/render/soft_gpu.c'),str(stage/'src/pc/guest/translated_runtime.c'),str(stage/'src/pc/memory.c'),'-o',str(binary)],check=True)
 subprocess.run([str(binary)],check=True)
