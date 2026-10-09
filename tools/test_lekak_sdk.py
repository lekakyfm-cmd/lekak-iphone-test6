#!/usr/bin/env python3
"""Real roster module + supplied Lekak manifest; host contracts, not iOS proof."""
import json,hashlib,os,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
SDK=ROOT/'EngineSDK'
def verify_sources():
    provenance=json.loads((SDK/'source-provenance.json').read_text())
    for item in provenance['files']:
        file=SDK/item['path']
        assert hashlib.sha256(file.read_bytes()).hexdigest()==item['sha256'],str(file)
def main():
    verify_sources()
    with tempfile.TemporaryDirectory(prefix='lekak-sdk-',dir=ROOT/'tools') as folder:
        binary=Path(folder)/'contracts'
        subprocess.run([os.environ.get('LEKAK_SDK_HOST_CC','gcc'),'-std=gnu11','-O2','-Wall','-Wextra','-Werror',
            '-DMEMORIES_PC','-I'+str(SDK/'src'),str(ROOT/'tools/test_lekak_sdk.c'),
            str(ROOT/'SourceMod/Lekak/legacy-native.c'),str(SDK/'src/pc/free_duel/duelists.c'),
            str(SDK/'src/pc/mods/json.c'),'-o',str(binary)],check=True)
        subprocess.run([str(binary),str(ROOT/'Resources/Lekak/mod.json')],cwd=folder,check=True)
    print('Engine SDK source hashes and Lekak host contracts passed; ARM64/iPhone validation pending.')
if __name__=='__main__':main()
