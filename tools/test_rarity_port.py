#!/usr/bin/env python3
"""Validate the staged desktop 0.7.6 catalogue + independently compiled C rules.

This is NOT an iOS runtime-hook test or proof that rarities appear in gameplay.
"""
from pathlib import Path
import json,re,subprocess,tempfile,hashlib,shutil,collections,sys
ROOT=Path(__file__).resolve().parents[1]
PC=ROOT/'Portage/Raretes_PC_v0.7.6'


def verify():
    manifest=json.loads((PC/'mod.json').read_text(encoding='utf-8'))
    assert manifest['id']=='lekak-rarities-test'
    assert manifest['version']=='0.7.6'
    assert manifest['min_api']<=4
    assert len(manifest['cards'])==4960
    assert len(manifest['equips'])==136
    assert manifest['requires']==['Lekakmod']
    counts=collections.Counter()
    expected=set()
    art_paths=set()
    for c in manifest['cards']:
        match=re.fullmatch(r't([1-4])-b(\d{4})',c['id'])
        assert match, c.get('id')
        tier,number=map(int,match.groups())
        assert 1<=number<=1240
        assert c['id'] not in expected
        expected.add(c['id'])
        counts[tier]+=1
        assert f'<lekak-rarity: {tier}>' in c['notes']
        assert f'<lekak-base: {number}>' in c['notes']
        if 'art' in c:
            art_paths.add(c['art'])
    assert all(counts[t]==1240 for t in range(1,5)),counts
    assert len(expected)==4960
    missing=sorted(a for a in art_paths if not (PC/a).is_file())
    assert not missing,missing[:10]
    identities=(PC/'sources/identities.h').read_text()
    assert '#define RARITY_BASE_COUNT 1240' in identities
    assert identities.count('"lekak-rarities-test:t4-b')==1240
    windows=(PC/'sources/rarities.c').read_text()
    assert re.search(r'if\(roll<8500\)return 0;',windows)
    assert re.search(r'if\(roll<9500\)return 1;',windows)
    assert re.search(r'if\(roll<9800\)return 2;',windows)
    assert re.search(r'if\(roll<9950\)return 3;',windows)
    assert 'monster_bonus[5]={0,100,300,500,800}' in windows
    assert 'rates[5]={1,3,8,20,50}' in windows
    assert len(list((PC/'sources').glob('*.c')))==11

    subprocess.run([sys.executable,str(ROOT/'tools/generate_rarity_registry.py'),'--check'],check=True)

    compiler=shutil.which('clang') or shutil.which('cc')
    assert compiler, 'A host C compiler is required'
    with tempfile.TemporaryDirectory(prefix='lekak-rarity-') as temp:
        exe=Path(temp)/'test-rarities'
        command=[compiler,'-std=c11','-O2','-Wall','-Wextra','-Werror',
                 '-I'+str(ROOT/'SourceMod/RaritiesNative'),
                 str(ROOT/'SourceMod/RaritiesNative/rarity_core.c'),
                 str(ROOT/'tools/test_rarity_native.c'),'-o',str(exe)]
        subprocess.run(command,check=True)
        subprocess.run([str(exe)],check=True)
        reg_exe=Path(temp)/'test-registry'
        subprocess.run([compiler,'-std=c11','-O2','-Wall','-Wextra','-Werror',
            str(ROOT/'SourceMod/RaritiesNative/rarity_registry.c'),
            str(ROOT/'tools/test_rarity_registry.c'),'-o',str(reg_exe)],check=True)
        subprocess.run([str(reg_exe)],check=True)
        snap_exe=Path(temp)/'test-snapshot'
        subprocess.run([compiler,'-std=c11','-O2','-Wall','-Wextra','-Werror',
            str(ROOT/'SourceMod/RaritiesNative/rarity_registry.c'),
            str(ROOT/'SourceMod/RaritiesNative/rarity_snapshot.c'),
            str(ROOT/'tools/test_rarity_snapshot.c'),'-o',str(snap_exe)],check=True)
        subprocess.run([str(snap_exe)],check=True)
        bridge_exe=Path(temp)/'test-save-bridge'
        subprocess.run([compiler,'-std=c11','-O2','-Wall','-Wextra','-Werror',
            str(ROOT/'SourceMod/RaritiesNative/rarity_registry.c'),
            str(ROOT/'SourceMod/RaritiesNative/rarity_snapshot.c'),
            str(ROOT/'SourceMod/RaritiesNative/rarity_save_bridge.c'),
            str(ROOT/'tools/test_rarity_save_bridge.c'),'-o',str(bridge_exe)],check=True)
        subprocess.run([str(bridge_exe)],check=True)
    print('Windows rarity v0.7.6 data parity OK: 4 x 1240 entries, '
          f'{len(art_paths)} staged art references, 136 equips.')

if __name__=='__main__':verify()
