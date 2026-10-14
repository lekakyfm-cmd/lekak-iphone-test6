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
    rarity_source=ROOT/'SourceMod/RaritiesNative/rarity_core.c'
    if modules.get('rarity_core_sha256')!=digest(rarity_source):
        raise RuntimeError('Stale rarity CPU rules: '+str(rarity_source))
    registry_source=ROOT/'SourceMod/RaritiesNative/rarity_registry.c'
    registry_header=ROOT/'SourceMod/RaritiesNative/rarity_base_keys.h'
    if modules.get('rarity_registry_sha256')!=digest(registry_source) or modules.get('rarity_registry_generated_sha256')!=digest(registry_header):
        raise RuntimeError('Stale rarity identity registry: '+str(registry_source))
    snapshot_source=ROOT/'SourceMod/RaritiesNative/rarity_snapshot.c'
    if modules.get('rarity_snapshot_sha256')!=digest(snapshot_source):
        raise RuntimeError('Stale rarity snapshot: '+str(snapshot_source))
    save_source=ROOT/'SourceMod/RaritiesNative/rarity_save_bridge.c'
    if modules.get('rarity_save_bridge_sha256')!=digest(save_source):
        raise RuntimeError('Stale native rarity save bridge: '+str(save_source))
    drop_source=ROOT/'SourceMod/RaritiesNative/rarity_drop_bridge.c'
    if modules.get('rarity_drop_bridge_sha256')!=digest(drop_source):
        raise RuntimeError('Stale iOS rarity drop bridge: '+str(drop_source))
    for unit in modules['units']:
        if unit['source_sha256']!=digest(ROOT/'SourceMod/Lekak'/unit['source']):
            raise RuntimeError('Stale mod source: '+unit['source'])
        if unit.get('compiled_source_sha256'):
            source=ROOT/'SourceMod/Lekak'/unit['source']
            content=source.read_bytes()
            if unit['source']=='name-colors.c':
                from build_lekak_ios_objects import patch_number_colors
                content=patch_number_colors(source.read_text()).encode()
            if hashlib.sha256(content).hexdigest()!=unit['compiled_source_sha256']:
                raise RuntimeError('Stale compiled mod source: '+unit['source'])
    from build_iphone_deps import NAMES,verify_archive
    dependencies=json.loads((deps/'ios-dependencies-report.json').read_text())
    for name,library in NAMES.items():
        archive=deps/'install/lib'/library
        if dependencies['libraries'][name]['archive_sha256']!=digest(archive):raise RuntimeError('Stale iOS dependency')
        verify_archive(archive)

def build(core,mods,deps,output,experimental_data_catalog=False,experimental_rarity_drops=False):
    if platform.system()!='Darwin' or platform.machine()!='arm64':raise SystemExit('Requires macOS ARM64 and Xcode.')
    if experimental_rarity_drops and not experimental_data_catalog:
        raise ValueError('Rarity drops require the separate experimental catalogue IPA')
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
    panel_obj=output/'results_panel.o'
    subprocess.run([cc,*target,'-std=c11','-O2','-I'+str(ROOT/'App'),
        '-c',str(ROOT/'App/results_panel.c'),'-o',str(panel_obj)],check=True)
    check_macho_ios(panel_obj)
    payload=output/'Payload';shutil.rmtree(payload,ignore_errors=True)
    app=payload/'LekakNative.app';app.mkdir(parents=True)
    executable=app/'LekakNative'
    libraries=[deps/'install/lib'/x for x in ['libfreetype.a','libpng16.a','libz.a']]
    command=[cc,*target,str(obj),str(panel_obj),'-Wl,-force_load,'+str(core/'libMemories_iPhone.a'),
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
        CFBundleName='LekakNative',CFBundleShortVersionString='0.29',CFBundleVersion='29',
        MinimumOSVersion='15.0',UIFileSharingEnabled=True,LSSupportsOpeningDocumentsInPlace=True,
        ITSAppUsesNonExemptEncryption=False)
    (app/'Info.plist').write_bytes(plistlib.dumps(info))
    report.update(iphone_game_linked=True,executable_sha256=digest(executable),
        core_sha256=digest(core/'libMemories_iPhone.a'),mod_sha256=digest(mods/'libLekak_iPhone.a'),
        frontend_sha256=digest(source),bundled_resource_files=len(list((app/'mods/Lekak').rglob('*.*'))),
        player_disc_included=False,unlock_conditions='normal, unchanged',
        pending=['Sign and install through Sideloadly','Verify launch, controls, saves, objectives and music on a physical iPhone'])
    (app/'native-build-report.json').write_text(json.dumps(report,indent=2)+'\n')
    ipa=output/'Lekak_iPhone_29_Recompenses.ipa'
    with zipfile.ZipFile(ipa,'w',zipfile.ZIP_DEFLATED) as bundle:
        for file in sorted(payload.rglob('*')):
            if file.is_file():bundle.write(file,file.relative_to(output))
    report['ipa_sha256']=digest(ipa)
    info['LekakTestUnlockAll']=True
    info['CFBundleDisplayName']='Lekak Test Unlock'
    (app/'Info.plist').write_bytes(plistlib.dumps(info))
    test_report=dict(report,unlock_conditions='all duelists unlocked for tests')
    (app/'native-build-report.json').write_text(json.dumps(test_report,indent=2)+'\n')
    test_ipa=output/'Lekak_iPhone_29_Recompenses_Full_Unlock.ipa'
    with zipfile.ZipFile(test_ipa,'w',zipfile.ZIP_DEFLATED) as bundle:
        for file in sorted(payload.rglob('*')):
            if file.is_file():bundle.write(file,file.relative_to(output))
    report['full_unlock_ipa_sha256']=digest(test_ipa);save()
    if experimental_data_catalog:
        # Explicit third IPA; neither regular nor full-unlock IPA includes
        # the 4960 data-only variants. Never copy an offline catalogue into
        # the stable bundle. The preview has NO rarity gameplay hooks.
        from experimental_rarity_bundle import prepare_experimental_bundle
        details=prepare_experimental_bundle(ROOT, output, app, info)
        exp_report=dict(report,
            scope='UNSAFE FOR MAIN SAVES: 4960 variant card records; no rarity hooks',
            catalogue_mode='EXPERIMENTAL_DATA_ONLY',
            catalogue_source_sha256=details['source_mod_json_sha256'],
            expected_cards=6200, expected_variants=4960,
            save_testing='NOT_TESTED_ON_DEVICE',
            player_warning='Use a separate test installation and backup saves')
        (app/'native-build-report.json').write_text(json.dumps(exp_report,indent=2)+'\n')
        exp_ipa=output/'Lekak_iPhone_RarityDataOnly_EXPERIMENTAL_UNSIGNED.ipa'
        with zipfile.ZipFile(exp_ipa,'w',zipfile.ZIP_DEFLATED) as bundle:
            for file in sorted(payload.rglob('*')):
                if file.is_file():bundle.write(file,file.relative_to(output))
        report['experimental_data_catalog_ipa_sha256']=digest(exp_ipa)
        report['experimental_data_catalog_mode']='EXPLICIT_OPT_IN_ONLY'
        if experimental_rarity_drops:
            # FOURTH, separate IPA: never alter the stable or data-only test.
            from experimental_rarity_bundle import enable_experimental_drop_test
            enable_experimental_drop_test(app, info)
            drops_report=dict(exp_report,catalogue_mode='EXPERIMENTAL_RARITY_DROPS',
                gameplay='Reward rarity mapping ONLY; no stat/graphics/library hooks',
                caveat='Untested on device; separate sandbox and backup saves')
            (app/'native-build-report.json').write_text(json.dumps(drops_report,indent=2)+'\n')
            drops_ipa=output/'Lekak_iPhone_RarityDrops_EXPERIMENTAL_UNSIGNED.ipa'
            with zipfile.ZipFile(drops_ipa,'w',zipfile.ZIP_DEFLATED) as bundle:
                for file in sorted(payload.rglob('*')):
                    if file.is_file():bundle.write(file,file.relative_to(output))
            report['experimental_rarity_drops_ipa_sha256']=digest(drops_ipa)
            report['experimental_rarity_drops_mode']='OPT_IN_BETA_NOT_DEVICE_TESTED'

        save()
    print('Full native iPhone application linked and packaged. Physical-device behavior remains unverified.')
    return ipa

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--core',type=Path,default=ROOT/'build/iphone-core')
    p.add_argument('--mods',type=Path,default=ROOT/'build/lekak-ios')
    p.add_argument('--deps',type=Path,default=ROOT/'EngineSDK/tmp/pc/ios-deps')
    p.add_argument('--output',type=Path,default=ROOT/'build/native-app')
    p.add_argument('--experimental-data-catalog',action='store_true',help='Also produce an explicitly gated data-only rarity preview IPA; not safe for main saves')
    p.add_argument('--experimental-rarity-drops',action='store_true',help='Build an additional isolated experimental drops IPA (requires --experimental-data-catalog)')
    a=p.parse_args();build(a.core.resolve(),a.mods.resolve(),a.deps.resolve(),a.output.resolve(),a.experimental_data_catalog,a.experimental_rarity_drops)
