#!/usr/bin/env python3
"""Exercise Objectives' real update body ahead of the port's navigation."""
import ast, os, subprocess, tempfile
from pathlib import Path
from build_iphone_engine import ROOT, stage_sources

source=(ROOT/'SourceMod/Lekak/objectives.c').read_text()
body=source[source.index('static s32 update(void)'):source.index('\nstatic void applied(')]
# update is the final function before applied; retain its actual production body.
fixture=r'''
#include <assert.h>
#include <stdio.h>
typedef unsigned short u16;
typedef int s32;
enum { PAD_DIRECTION_UP=1, PAD_DIRECTION_DOWN=2, PAD_BUTTON_CANCEL=4,
PAD_BUTTON_TRIANGLE=8, PAD_BUTTON_R1=16, PAD_DIRECTION_RIGHT=32,
PAD_BUTTON_L1=64, PAD_DIRECTION_LEFT=128, PAD_BUTTON_CONFIRM_MASK=256,
PAD_BUTTON_START=512 };
static u16 gInput_wPad1Pressed,gInput_wPad1Held,gInput_wPad1Repeat;
static int selected,opened,page,label=-1,english,update_logged,gMain_bMenuID=8;
struct Host {void (*log)(const struct Host *,const char *);void (*set_setting)(const struct Host *,const char *,int);};
static struct Host stub_host;static const struct Host *host=&stub_host;
static int stable(void){return 1;}
static void variant(int id,int active){(void)id;(void)active;}
static void close_book(void){opened=0;}
static void show_page(void){opened=1;}
static void remove_box(int id){(void)id;}
static void menu_visibility(int hide){(void)hide;}
static void cleanup(void){selected=opened=0;}
/* Model the upstream TitleMenu_Before input consumption, before stock update. */
static s32 title_update(void){
 unsigned navigation=gInput_wPad1Repeat|gInput_wPad1Pressed;
 if(navigation&PAD_DIRECTION_DOWN)gMain_bMenuID=9;
 if(navigation&PAD_DIRECTION_UP)gMain_bMenuID=8;
 gInput_wPad1Pressed=gInput_wPad1Repeat=0;return -1;
}
static void *old_update=(void *)title_update;
'''+body+r'''
static void press(unsigned bits){gInput_wPad1Pressed=bits;gInput_wPad1Repeat=bits;update();}
int main(void){
 press(PAD_DIRECTION_DOWN);assert(selected && gMain_bMenuID==8);
 press(PAD_BUTTON_CONFIRM_MASK);assert(opened);
 press(PAD_BUTTON_R1);assert(opened && page==1);
 press(PAD_BUTTON_CANCEL);assert(!opened && selected);
 press(PAD_DIRECTION_DOWN);assert(!selected && gMain_bMenuID==9);
 press(PAD_DIRECTION_UP);assert(selected && gMain_bMenuID==8);
 press(PAD_DIRECTION_UP);assert(!selected && gMain_bMenuID==8);
 puts("Objectives: Library -> Objectives -> Password, reverse navigation, open and close passed");
}
'''
with tempfile.TemporaryDirectory(prefix='lekak-objectives-') as folder:
 stage=Path(folder);stage_sources(stage)
 builder=(stage/'tools/pc/build_arm64.py').read_text();ast.parse(builder)
 assert 'source == "src/pc/platform/title_screen.c"' in builder
 assert "'-DMEMORIES_IOS'" in (ROOT/'tools/build_lekak_ios_objects.py').read_text()
 for ios,expected in [(False,'TitleScreen_Update'),(True,'TitleScreen_Update')]:
  flags=['-DMEMORIES_PC','-DMEMORIES_TRANSLATED','-DMEMORIES_MOD']
  if ios:flags.append('-DMEMORIES_IOS')
  out=subprocess.check_output([os.environ.get('CC','cc'),'-E','-P',*flags,'-I'+str(ROOT/'EngineSDK/src'),'-I'+str(ROOT/'EngineSDK/config'),str(ROOT/'SourceMod/Lekak/objectives.c')],text=True)
  assert '(void *)'+expected+',(void *)update,&old_update' in out
 test=stage/'test.c';test.write_text(fixture);binary=stage/'test'
 subprocess.run([os.environ.get('CC','cc'),'-std=gnu11','-Wall','-Wextra','-Werror',str(test),'-o',str(binary)],check=True)
 subprocess.run([str(binary)],check=True)
