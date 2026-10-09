#!/usr/bin/env python3
"""Import supplied mod resources without loading its x86 object files.

Data preparation only. This does not install runtime hooks or expand guest RAM.
"""
import argparse, hashlib, json, re, shutil
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

def quoted(value):
    return json.dumps(value, ensure_ascii=False)

def prepare(source):
    mod = json.loads((source / 'mod.json').read_text())
    objectives = json.loads((source / 'objectives.json').read_text())
    progress = json.loads((source / 'objectives-progress.json').read_text())
    if len(mod['duelists']) != 40 or len(objectives) != 40 or len(progress) != 40:
        raise ValueError('Expected all 40 duelists and matching objective rows')
    namespace = mod['id']
    duelists = {namespace + ':' + d['id'] for d in mod['duelists']}
    cards = {namespace + ':' + c['id'] for c in mod['cards'] if 'id' in c}
    if len(duelists) != 40 or len(cards) != sum('id' in c for c in mod['cards']):
        raise ValueError('Duplicate identity')
    passwords = []
    for card in mod['cards']:
        password = card.get('password')
        if not isinstance(password, str) or not re.fullmatch(r'[0-9]{8}', password):
            raise ValueError('Missing eight-digit password: ' + card.get('name', '?'))
        passwords.append(password)
        if card.get('art') and not (source / card['art']).is_file():
            raise ValueError('Missing card image: ' + card['art'])
    if len(set(passwords)) != len(passwords):
        raise ValueError('Duplicate mod password')
    for i, row in enumerate(progress):
        identity = namespace + ':' + mod['duelists'][i]['id']
        if objectives[i]['id'] != identity or objectives[i]['unlock'] != mod['duelists'][i]['unlock']:
            raise ValueError('Objective/manifest disagreement: ' + identity)
        if not row or len(row) > 6:
            raise ValueError('Invalid objective row')
        for condition in row:
            if condition['kind'] not in range(8) or condition['target'] <= 0:
                raise ValueError('Invalid condition')
            ref = condition['ref']
            if ref.startswith(namespace + ':'):
                known = cards if condition['kind'] in (3, 4) else duelists
                if ref not in known:
                    raise ValueError('Unknown objective reference: ' + ref)
    unresolved_retail = set()
    for folder in ('drops', 'decks'):
        for path in (source / folder).glob('*.json'):
            data = json.loads(path.read_text())
            tables = data.values() if folder == 'drops' else [data]
            for table in tables:
                for ref, weight in table.items():
                    if ref == 'replace':
                        continue
                    if not isinstance(weight, int) or isinstance(weight, bool) or weight < 0:
                        raise ValueError('Invalid weight: ' + str(path))
                    if ref.startswith(namespace + ':'):
                        base = ref[:-2] if ref.endswith(':1') else ref
                        if base not in cards:
                            raise ValueError('Unknown table card: ' + ref)
                    elif not ref.isdecimal():
                        unresolved_retail.add(ref)
    for duelist in mod['duelists']:
        for folder in ('drops', 'decks', 'portraits'):
            suffix = '.png' if folder == 'portraits' else '.json'
            if not (source / folder / (duelist['id'] + suffix)).is_file():
                raise ValueError('Missing duelist resource: ' + duelist['id'])
    for music in mod['audio']['music'].values():
        if not (source / music['file']).is_file():
            raise ValueError('Missing music: ' + music['file'])
    target = ROOT / 'Resources/Lekak'
    target.mkdir(parents=True, exist_ok=True)
    assets = []
    # Only these directories are bundled. Native hook source stays separate;
    # supplied x86 objects are never linked or advertised as iPhone binaries.
    for folder in ('art', 'portraits', 'music', 'title', 'free-duel', 'buttons', 'textures', 'drops', 'decks'):
        for file in sorted((source / folder).rglob('*')):
            if not file.is_file():
                continue
            relative = file.relative_to(source)
            destination = target / relative
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(file, destination)
            assets.append({'path': relative.as_posix(), 'sha256': hashlib.sha256(file.read_bytes()).hexdigest(), 'bytes': file.stat().st_size})
    for name in ('mod.json', 'objectives.json', 'objectives-progress.json', 'objectives-text.txt',
                 'objectives-text-en.txt', 'objectives-progress-reserve.txt', 'CREDITS-MUSIQUES.txt'):
        file = source / name
        shutil.copy2(file, target / name)
        assets.append({'path': name, 'sha256': hashlib.sha256(file.read_bytes()).hexdigest(), 'bytes': file.stat().st_size})
    lines = ['/* Generated from supplied final Windows mod; do not edit. */',
             'static const char *const duelist_ids[40] = {']
    lines += [' ' + quoted(namespace + ':' + d['id']) + ',' for d in mod['duelists']]
    lines += ['};', 'static const unsigned condition_counts[40] = {',
              ','.join(str(len(row)) for row in progress), '};',
              'static const LekakCondition conditions[40][6] = {']
    for row in progress:
        lines.append(' {' + ','.join('{' + str(c['kind']) + ',' + str(c['target']) + ',' + quoted(c['ref']) + '}' for c in row) + '},')
    lines += ['};', '']
    (ROOT / 'App/lekak_progress_data.h').write_text('\n'.join(lines))
    report = {'mod_id': namespace, 'mod_version': mod['version'], 'duelists': 40,
              'added_cards': len(cards), 'card_edits': len(mod['cards']),
              'resource_files': len(assets), 'resources': assets,
              'retail_names_require_engine_catalogue': sorted(unresolved_retail),
              'runtime_hooks_installed': False, 'saves_available': False,
              'native_hook_sources_missing': ['LekakLegacyInit (legacy notifications / Tyrant Kaiba)'],
              'engine_sdk_required': ['pc/mods/modapi.h', 'pc/cards', 'pc/free_duel', 'game symbols / address map'],
              'excluded_native_objects': sorted(p.name for p in source.glob('*.o'))}
    (target / 'import-report.json').write_text(json.dumps(report, indent=2, ensure_ascii=False) + '\n')
    return report

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('source', type=Path)
    args = parser.parse_args()
    report = prepare(args.source.resolve())
    print(f"Prepared {report['duelists']} duelists, {report['added_cards']} added cards, {report['resource_files']} files; runtime hooks pending.")
