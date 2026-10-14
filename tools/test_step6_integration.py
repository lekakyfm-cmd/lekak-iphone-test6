#!/usr/bin/env python3
"""Static checks that a workflow keeps the native rarity hooks isolated."""
from pathlib import Path
import re
root=Path(__file__).resolve().parents[1]
s=(root/'Corrections/EngineSDK/src/game/duel_result_runtime.c').read_text()
w=(root/'.github/workflows/lekak-native-ios.yml').read_text()
b=(root/'tools/build_native_ipa.py').read_text()
e=(root/'App/native_entry.c').read_text()
# Immutable engine source must still match its original pin.
import hashlib, json
pins=json.loads((root/'EngineSDK/source-provenance.json').read_text())['files']
expected=next(x['sha256'] for x in pins if x['path']=='src/game/duel_result_runtime.c')
assert hashlib.sha256((root/'EngineSDK/src/game/duel_result_runtime.c').read_bytes()).hexdigest()==expected
assert s.count('LekakRarity_DropBridgeChoose(')==2  # declaration and call
# Immutable engine source must still match its original pin.
import hashlib, json
pins=json.loads((root/'EngineSDK/source-provenance.json').read_text())['files']
expected=next(x['sha256'] for x in pins if x['path']=='src/game/duel_result_runtime.c')
assert hashlib.sha256((root/'EngineSDK/src/game/duel_result_runtime.c').read_bytes()).hexdigest()==expected
assert s.count('ios_rarity_reward(')>=4
assert '#if defined(MEMORIES_IOS) && defined(MEMORIES_PC)' in s
assert 'rarity_hook.read_bytes()' in (root/'tools/build_iphone_engine.py').read_text() and 'hook_source.write_bytes' in (root/'tools/build_iphone_engine.py').read_text()
assert 'experimental_rarity_drops' in w and '--experimental-rarity-drops' in w
assert '--experimental-rarity-drops' in b and 'enable_experimental_drop_test(app, info)' in b
assert '.raritydroptest' in (root/'tools/experimental_rarity_bundle.py').read_text()
assert 'LEKAK_IOS_RARITY_DROPS' in e and 'if(test_drops) LekakRarity_DropBridgeSetEnabled(1);' in e
assert 'LEKAK_IOS_RARITY_DROPS' in (root/'App/native_main.m').read_text()
print('Step 6 opt-in and corrected engine source wiring OK')
