#!/usr/bin/env python3
"""Check bundled imported bytes and prevent unsupported object-file inclusion."""
import hashlib,json
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
def verify():
    directory=ROOT/'Resources/Lekak'
    report=json.loads((directory/'import-report.json').read_text())
    for asset in report['resources']:
        relative=Path(asset['path'])
        assert not relative.is_absolute() and '..' not in relative.parts
        file=directory/relative
        assert file.stat().st_size==asset['bytes'],str(file)
        assert hashlib.sha256(file.read_bytes()).hexdigest()==asset['sha256'],str(file)
    assert not list(directory.rglob('*.o'))
    mod=json.loads((directory/'mod.json').read_text())
    assert len(mod['duelists'])==report['duelists']==40
    assert sum('id' in c for c in mod['cards'])==report['added_cards']==518
    assert [d['id'] for d in mod['duelists'][-3:]]==['lesub','yem','lekak']
    assert not report['runtime_hooks_installed'] and not report['saves_available']
    print(f"Lekak resources verified: {len(report['resources'])} unchanged files; runtime hooks pending.")
if __name__=='__main__':verify()
