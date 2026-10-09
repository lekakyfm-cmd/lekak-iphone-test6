#!/usr/bin/env python3
"""Link UIKit + genuine iOS engine/mod/dependencies and package an unsigned IPA.

Link success is a build result, not proof of correct physical-device behavior.
The player imports their own USA BIN; no disc is distributed in this package.
"""
import argparse,hashlib,json,platform,plistlib,shutil,subprocess,zipfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]

def digest(path):return hashlib.sha256(path.read_bytes()).hexdigest()

def validate_inputs(core,mods,deps):
    from verify_lekak import verify
    verify()
    for archive,report_file in [(core/'libMemories_iPhone.a',core/'ios-core-report.json'),
            (mods/'libLekak_iPhone.a',mods/'build-report.json')]:
        data=json.loads(report_file.read_text())
        if data.get('target')!='arm64-apple-ios15.0' or data.get('archive_sha256')!=digest(archive):
            raise RuntimeError('Archive/report mismatch: '+str(archive))
        if data.get('emit_failures') or data.get('transform_failures'):raise RuntimeError('Incomplete core build')
    # Verify source-to-module linkage instead of silently accepting an old mod.
    modules=json.loads((mods/'build-report.json').read_text())
    for unit in modules['units']:
        if unit['source_sha256']!=digest(ROOT/'SourceMod/Lekak'/unit['source']):
            raise RuntimeError('Stale mod source: '+unit['source'])
    from build_iphone_deps import NAMES,verify_archive
    dependencies=json.loads((deps/'ios-dependencies-report.json').read_text())
    for name,library in NAMES.items():
        archive=deps/'install/lib'/library
        if dependencies['libraries'][name]['archive_sha256']!=digest(archive):raise RuntimeError('Stale iOS dependency')
        verify_archive(archive)

def build(core,mods,deps,output):
    if platform.system()!='Darwin' or platform.machine()!='arm64':raise SystemExit('Requires macOS ARM64 and Xcode.')
    output.mkdir(parents=True,exist_ok=True)
    report={'target':'arm64-apple-ios15.0','iphone_game_linked':False,'device_tested':False,
        'scope':'Full translated engine and static Lekak mod with UIKit frontend'}
    report_path=output/'native-app-report.json'
    def save():report_path.write_text(json.dumps(report,indent=2)+'\n')
    save();validate_inputs(core,mods,deps)
    sdk=subprocess.check_output(['xcrun','--sdk','iphoneos','--show-sdk-path'],text=True).strip()
    cc=subprocess.check_output(['xcrun','--sdk','iphoneos','--find','clang'],text=True).strip()
    target=['-target','arm64-apple-ios15.0','-isysroot',sdk]
    obj=output/'native_main.o';source=ROOT/'App/native_main.m'
    command=[cc,*target,'-fobjc-arc','-fblocks','-O2','-Wall','-Wextra','-Werror=implicit-function-declaration',
        '-I'+str(ROOT/'App'),'-I'+str(ROOT/'EngineSDK/src'),'-c',str(source),'-o',str(obj)]
    report['frontend_command']=command;save();subprocess.run(command,check=True)
    from build_lekak_ios_objects import check_macho_ios
    check_macho_ios(obj)
    payload=output/'Payload';shutil.rmtree(payload,ignore_errors=True)
    app=payload/'LekakNative.app';app.mkdir(parents=True)
    executable=app/'LekakNative'
    libraries=[deps/'install/lib'/x for x in ['libfreetype.a','libpng16.a','libz.a']]
    command=[cc,*target,str(obj),'-Wl,-force_load,'+str(core/'libMemories_iPhone.a'),
        '-Wl,-force_load,'+str(mods/'libLekak_iPhone.a'),*[str(p) for p in libraries],'-liconv',
        '-Wl,-map,'+str(output/'native-link.map'),'-o',str(executable)]
    for framework in ['UIKit','Foundation','UniformTypeIdentifiers','AVFoundation','AudioToolbox','CoreText','CoreGraphics','QuartzCore']:
        command+=['-framework',framework]
    report['link_command']=command;save();subprocess.run(command,check=True)
    check_macho_ios(executable,file_type=2)
    executable.chmod(0o755)
    # External object loading is intentionally absent; the manifest's hooks
    # resolve to the verified static mod archive in the full engine loader.
    shutil.copytree(ROOT/'Resources/Lekak',app/'mods/Lekak')
    info=plistlib.loads((ROOT/'App/Info.plist').read_bytes())
    info.update(CFBundleDisplayName='Lekak',CFBundleExecutable='LekakNative',
        CFBundleName='LekakNative',CFBundleShortVersionString='0.17',CFBundleVersion='17',
        MinimumOSVersion='15.0',UIFileSharingEnabled=True,LSSupportsOpeningDocumentsInPlace=True,
        ITSAppUsesNonExemptEncryption=False)
    (app/'Info.plist').write_bytes(plistlib.dumps(info))
    report.update(iphone_game_linked=True,executable_sha256=digest(executable),
        core_sha256=digest(core/'libMemories_iPhone.a'),mod_sha256=digest(mods/'libLekak_iPhone.a'),
        frontend_sha256=digest(source),bundled_resource_files=len(list((app/'mods/Lekak').rglob('*.*'))),
        player_disc_included=False,unlock_conditions='normal, unchanged',
        pending=['Sign and install through Sideloadly','Verify launch, controls, saves, objectives and music on a physical iPhone'])
    (app/'native-build-report.json').write_text(json.dumps(report,indent=2)+'\n')
    ipa=output/'Lekak_iPhone_17_Native.ipa'
    with zipfile.ZipFile(ipa,'w',zipfile.ZIP_DEFLATED) as bundle:
        for file in sorted(payload.rglob('*')):
            if file.is_file():bundle.write(file,file.relative_to(output))
    report['ipa_sha256']=digest(ipa)
    info['LekakTestUnlockAll']=True
    info['CFBundleDisplayName']='Lekak Test Unlock'
    (app/'Info.plist').write_bytes(plistlib.dumps(info))
    test_report=dict(report,unlock_conditions='all duelists unlocked for tests')
    (app/'native-build-report.json').write_text(json.dumps(test_report,indent=2)+'\n')
    test_ipa=output/'Lekak_iPhone_17_Full_Unlock.ipa'
    with zipfile.ZipFile(test_ipa,'w',zipfile.ZIP_DEFLATED) as bundle:
        for file in sorted(payload.rglob('*')):
            if file.is_file():bundle.write(file,file.relative_to(output))
    report['full_unlock_ipa_sha256']=digest(test_ipa);save()
    print('Full native iPhone application linked and packaged. Physical-device behavior remains unverified.')
    return ipa

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--core',type=Path,default=ROOT/'build/iphone-core')
    p.add_argument('--mods',type=Path,default=ROOT/'build/lekak-ios')
    p.add_argument('--deps',type=Path,default=ROOT/'EngineSDK/tmp/pc/ios-deps')
    p.add_argument('--output',type=Path,default=ROOT/'build/native-app')
    a=p.parse_args();build(a.core.resolve(),a.mods.resolve(),a.deps.resolve(),a.output.resolve())
