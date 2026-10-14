#!/usr/bin/env python3
"""Offline packaging checks; no iOS simulator, device, or signed IPA required."""
import json
import plistlib
from pathlib import Path
import tempfile
from experimental_rarity_bundle import prepare_experimental_bundle, enable_experimental_drop_test
ROOT = Path(__file__).resolve().parents[1]


def test():
    # Checks the same staging path used by the optional third IPA.
    with tempfile.TemporaryDirectory(prefix='lekak-rarity-optin-') as tmp:
        dest=Path(tmp)
        app=dest/'Payload'/'LekakNative.app'
        original=app/'mods'/'Lekak'
        original.mkdir(parents=True)
        original.joinpath('mod.json').write_text('{"id":"Lekakmod"}')
        stable_info={'CFBundleIdentifier':'org.lekak.iosprobe',
            'LekakTestUnlockAll': False}
        report=prepare_experimental_bundle(ROOT, dest, app, stable_info)
        info=plistlib.loads((app/'Info.plist').read_bytes())
        assert info['CFBundleIdentifier']=='org.lekak.iosprobe.raritycatalogtest'
        assert info['CFBundleIdentifier']!=stable_info['CFBundleIdentifier']
        assert info['LekakExperimentalRarityDataOnly'] is True
        assert info['LekakTestUnlockAll'] is False
        assert (original/'mod.json').read_text()=='{"id":"Lekakmod"}'
        preview=app/'mods'/'lekak-rarities-test'
        assert len(list((preview/'art').glob('*.png')))==518
        cards=json.loads((preview/'mod.json').read_text())['cards']
        assert len(cards)==4960
        assert all(c.get('drops') is False and c.get('opponents') is False
                   and c.get('password','bad') is None for c in cards)
        assert 'library' not in json.loads((preview/'mod.json').read_text())
        assert report['status']=='OFFLINE_ONLY_NOT_BUNDLED'
        assert report['variant_cards']==4960
        # Fourth IPA: distinct bundle id, explicit switch, no source save sharing.
        drops_id=enable_experimental_drop_test(app,stable_info)
        drops_info=plistlib.loads((app/'Info.plist').read_bytes())
        assert drops_id=='org.lekak.iosprobe.raritydroptest'
        assert drops_info['LekakExperimentalRarityDrops'] is True
        assert drops_info['LekakExperimentalRarityDataOnly'] is True
        assert drops_info['LekakTestUnlockAll'] is False
        assert drops_info['CFBundleIdentifier'] not in (
            stable_info['CFBundleIdentifier'],info['CFBundleIdentifier'])
        try:
            enable_experimental_drop_test(app, stable_info)
            raise AssertionError('Duplicate drop preview activation allowed')
        except ValueError:
            pass
        try:
            prepare_experimental_bundle(ROOT, dest, app, stable_info)
            raise AssertionError('Duplicate activation erroneously allowed')
        except ValueError:
            pass
    code=(ROOT/'App/native_entry.c').read_text()
    assert 'MEMORIES_MOD_LEKAK_RARITIES_TEST' in code
    assert 'snapshot!=1 || gCard_nCount!=6200' in code
    assert 'return -12;' in code
    # Stable builds remain unchanged by default.
    build=(ROOT/'tools/build_native_ipa.py').read_text()
    assert 'experimental_data_catalog=False' in build
    assert 'if experimental_data_catalog:' in build
    print('Opt-in iPhone catalogue packaging: 4960 variants, 518 art, separate sandbox, stable default off: OK')

if __name__=='__main__':test()
