#!/usr/bin/env python3
"""Verify the actual portable engine sources and the iOS bridge on a host."""
from pathlib import Path
import os,subprocess,tempfile
ROOT=Path(__file__).resolve().parents[1]
SOURCES=['App/lekak_rules.c','App/audio_ring.c','App/display_snapshot.c','App/xa_audio.c','App/mdec.c','App/spu_reverb.c','App/spu_dsp.c','App/gpu_io.c','App/guest_exec.c','App/startup_memory.c','App/disc_loader.c','App/game_memory.c','App/engine_callbacks.c','App/optional_textures.c','Engine/src/pc/memory.c',
 'Engine/src/pc/rng.c','Engine/src/pc/compat/libgs_ot.c','Engine/src/pc/compat/gte.c',
 'Engine/src/pc/compat/pgxp.c','Engine/src/pc/render/packets.c','Engine/src/pc/render/soft_gpu.c']
with tempfile.TemporaryDirectory(prefix='lekak-engine-',dir=ROOT/'tools') as tmp:
 cc=os.environ.get('CC','cc')
 flags=['-std=c11','-O2','-Wall','-Wextra','-pthread','-I'+str(ROOT/'Engine/src')]
 objects=[]
 # Reuse only immutable object files. Every test runs in a fresh process.
 for index,source in enumerate(SOURCES):
  obj=Path(tmp)/f'common-{index}.o'
  subprocess.run([cc,*flags,'-c',str(ROOT/source),'-o',str(obj)],check=True)
  objects.append(str(obj))
 fixture=Path(tmp)/'fixture.o'
 subprocess.run([cc,*flags,'-c',str(ROOT/'tools/test_file.c'),'-o',str(fixture)],check=True)
 objects.append(str(fixture))
 unit_flags=[*flags,'-include',str(ROOT/'tools/test_file.h')]
 test_environment={**os.environ,'LEKAK_TEST_TMPDIR':tmp}
 for name,unit,extra in [
  ('bridge','tools/test_engine_bridge.c',['App/engine_bridge.c']),
  ('startup-memory','tools/test_startup_memory.c',[]),
  ('guest-execution','tools/test_guest_exec.c',[]),
  ('session','tools/test_session.c',[]),
  ('audio-ring','tools/test_audio_ring.c',[]),
  ('lekak-rules','tools/test_lekak_rules.c',[]),
  ('display-snapshot','tools/test_display_snapshot.c',[]),
  ('xa-audio','tools/test_xa_audio.c',[]),
  ('mdec','tools/test_mdec.c',[]),
  ('gpu-ports','tools/test_gpu_io.c',[]),
  ('timing','tools/test_timing.c',[]),
  ('irq-dma','tools/test_irq_dma.c',[]),
  ('bios-startup','tools/test_bios_startup.c',[]),
  ('bios-events','tools/test_bios_events.c',[]),
  ('pad-chain','tools/test_pad_chain.c',[]),
  ('load-card','tools/test_load_card.c',[]),
  ('card-patches','tools/test_card_patches.c',[]),
  ('cd-transport','tools/test_cd_transport.c',[]),
  ('spu-config','tools/test_spu_config.c',[]),
  ('spu-dsp','tools/test_spu_dsp.c',[]),
  ('spu-reverb','tools/test_spu_reverb.c',[]),
  ('spu-dma','tools/test_spu_dma.c',[]),
  ('cd-registers','tools/test_cd_registers.c',[]),
  ('disc-loader','tools/test_disc_loader.c',[]),
  ('game-memory','tools/test_game_memory.c',[]),
  ('ordering-table','Engine/tests/libgs_ot_test.c',[]),
  ('packets','Engine/tests/packet_test.c',[]),
  ('software-gpu','Engine/tests/soft_gpu_test.c',[])]:
  binary=Path(tmp)/name
  subprocess.run([cc,*unit_flags,str(ROOT/unit),*[str(ROOT/p) for p in extra],*objects,
   '-lm','-o',str(binary)],check=True)
  subprocess.run([str(binary)],env=test_environment,check=True)
  print(name+': passed',flush=True)
