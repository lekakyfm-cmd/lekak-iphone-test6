#!/usr/bin/env python3
"""Compare native pixel kernels byte-for-byte with the upstream algorithms."""
import os
import subprocess
import tempfile
from pathlib import Path
from build_iphone_engine import stage_sources, ROOT

def main():
    with tempfile.TemporaryDirectory(prefix='lekak-pixels-') as folder:
        stage=Path(folder)
        stage_sources(stage)
        original=(ROOT/'EngineSDK/src/pc/cards/art.c').read_text()
        resample=original[original.index('static void resample('):original.index('/* --- colors')]
        colors=original[original.index('typedef struct { int first, count; } Box;'):original.index('static void put_clut(')]
        harness='''#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
typedef struct { unsigned char r,g,b; } Rgb;
extern void LekakNative_CardResample(const Rgb *,int,int,Rgb *,int,int);
extern void LekakNative_CardQuantize(const Rgb *,int,int,unsigned short *,unsigned char *);
'''+resample+colors+'''
int main(void){
 Rgb input[102*96],a[102*96],b[102*96];
 unsigned char ia[102*96],ib[102*96];unsigned short ca[256],cb[256];
 for(int pattern=0;pattern<4;pattern++){
  for(int i=0;i<102*96;i++)input[i]=(Rgb){pattern==0?0:(i*37+pattern)%256,pattern==1?255:(i*71)%256,(i*13+pattern*7)%256};
  for(int shape=0;shape<3;shape++){
   int w=shape==0?102:shape==1?40:48,h=shape==0?96:shape==1?32:48;
   int n=w*h,col=shape==0?255:63;
   resample(input,102,96,a,w,h);LekakNative_CardResample(input,102,96,b,w,h);
   assert(!memcmp(a,b,n*sizeof(Rgb)));
   memset(ca,0,sizeof ca);memset(cb,0,sizeof cb);
   quantize(a,n,col,ca,ia);LekakNative_CardQuantize(b,n,col,cb,ib);
   assert(!memcmp(ca,cb,sizeof ca));assert(!memcmp(ia,ib,n));
  }
 }
 puts("Native card pixels: all resampled bytes, palettes and indices match upstream");
}
'''
        source=stage/'test.c';source.write_text(harness)
        binary=stage/'test'
        subprocess.run([os.environ.get('LEKAK_SDK_HOST_CC','cc'),'-std=gnu11','-O2','-Wall','-Wextra','-Werror',str(source),str(stage/'src/pc/platform/native_card_pixels.c'),'-o',str(binary)],check=True)
        subprocess.run([str(binary)],check=True)

if __name__=='__main__':main()
