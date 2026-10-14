#!/usr/bin/env python3
"""Prepare an opt-in, data-only iPhone rarities diagnostic app bundle.

Never call for a stable build. The preview has no 3D/duel/rarity hooks.
The bundle identifier is distinct, so iOS keeps its application sandbox
(and its user saves) separate from the stable Lekak installation.
"""
import json
import plistlib
import shutil
from pathlib import Path
from stage_rarity_ios_catalog import produce


def prepare_experimental_bundle(root, output, app, stable_info):
    root, output, app = map(Path, (root, output, app))
    if not app.is_dir() or not (app / 'mods' / 'Lekak' / 'mod.json').is_file():
        raise ValueError('Main Lekak manifest missing')
    if (app / 'mods' / 'lekak-rarities-test').exists():
        raise ValueError('Experimental rarities already staged')
    stable_id = stable_info.get('CFBundleIdentifier')
    if not isinstance(stable_id, str) or not stable_id:
        raise ValueError('Stable bundle identifier missing')
    preview = output / 'ios-experimental-catalog'
    shutil.rmtree(preview, ignore_errors=True)
    details = produce(preview)
    cards = json.loads((preview / 'mod.json').read_text())['cards']
    if len(cards) != 4960 or details.get('copied_art_assets') != 518:
        raise ValueError('Experimental catalog failed validation')
    shutil.copytree(preview, app / 'mods' / 'lekak-rarities-test')
    exp_info = dict(stable_info)
    exp_info.update({
        'CFBundleIdentifier': stable_id + '.raritycatalogtest',
        'CFBundleDisplayName': 'Lekak Rarity Test',
        'LekakTestUnlockAll': False,
        'LekakExperimentalRarityDataOnly': True,
    })
    (app / 'Info.plist').write_bytes(plistlib.dumps(exp_info))
    if exp_info['CFBundleIdentifier'] == stable_id:
        raise ValueError('Rarity preview must use a separate iOS data container')
    return details


def enable_experimental_drop_test(app, stable_info):
    """Convert a fully prepared data-only app to a FOURTH drop test app.

    This must never stage the rarity manifest by itself. It uses the existing
    complete data-only preview, and it has a separate app sandbox.
    """
    app=Path(app)
    info_path=app/'Info.plist'
    if not info_path.is_file() or not (app/'mods/lekak-rarities-test/mod.json').is_file():
        raise ValueError('The data-only catalogue preview must be prepared first')
    info=plistlib.loads(info_path.read_bytes())
    if info.get('LekakExperimentalRarityDataOnly') is not True or info.get('LekakExperimentalRarityDrops'):
        raise ValueError('Expected untouched data-only preview before adding drop hooks')
    stable_id=stable_info.get('CFBundleIdentifier')
    if not isinstance(stable_id,str) or not stable_id:
        raise ValueError('Missing stable bundle identifier')
    data_id=info['CFBundleIdentifier']
    drops_id=stable_id+'.raritydroptest'
    if drops_id==stable_id or drops_id==data_id:
        raise ValueError('Drop preview must use a separate sandbox')
    info.update(CFBundleIdentifier=drops_id,
        CFBundleDisplayName='Lekak Rarity Drops',
        LekakExperimentalRarityDrops=True,
        LekakTestUnlockAll=False)
    info_path.write_bytes(plistlib.dumps(info))
    return drops_id
