#!/usr/bin/env python3
"""Cross-compile the complete translated core for iOS without modifying EngineSDK.

Produces a static archive and compiler logs, NOT an IPA. UIKit integration,
iOS dependency libraries, final linkage and physical-device testing follow.
"""
import argparse,hashlib,json,platform,shutil,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
SDK=ROOT/'EngineSDK'

def replace_once(path,before,after):
    text=path.read_text()
    if text.count(before)!=1:raise ValueError('Upstream patch context changed: '+str(path))
    path.write_text(text.replace(before,after,1))

def stage_sources(stage):
    # Only copy authoritative source roots, never recursively copy tmp/build.
    for folder in ['src','config','tools']:
        shutil.copytree(SDK/folder,stage/folder,dirs_exist_ok=True)
    for file in ['native_platform.c','native_platform.h','native_entry.c','native_entry.h']:
        shutil.copyfile(ROOT/'App'/file,stage/'src/pc/platform'/file)
    mods=stage/'src/pc/mods/mods.c'
    replace_once(mods,'#if defined(__APPLE__) && defined(__aarch64__) && defined(MEMORIES_TRANSLATED)\n    char error[STATUS_MAX];',
        '#if defined(MEMORIES_IOS)\n'
        '    extern int MemoriesModInit(const MemoriesModHost *, MemoriesMod *);\n'
        '    extern void LekakStatic_RegisterGlobals(void);\n'
        '    static int registered;\n'
        '    (void)path;\n'
        '    if (strcmp(mod->id, "Lekakmod")) { note(mod, "External code mods are unsupported on iPhone"); return NULL; }\n'
        '    if (!registered) { LekakStatic_RegisterGlobals(); registered = 1; }\n'
        '    union { MemoriesModEntry entry; void *pointer; } native = { .entry = MemoriesModInit };\n'
        '    return native.pointer;\n'
        '#elif defined(__APPLE__) && defined(__aarch64__) && defined(MEMORIES_TRANSLATED)\n    char error[STATUS_MAX];')
    replace_once(mods,'    if (mod->object.image || mod->object.native_handle) return 1;',
        '#ifdef MEMORIES_IOS\n    if (mod->initialized) return 1;\n#endif\n'
        '    if (mod->object.image || mod->object.native_handle) return 1;')
    replace_once(stage/'src/pc/sdk/libetc.c','        exit(0);',
        '        extern void LekakNative_StopGame(void);\n        LekakNative_StopGame();')
    replace_once(stage/'src/pc/platform/platform_common.c',
        'int Platform_HasDesktopGL(void) { return 1; }',
        'int Platform_HasDesktopGL(void) { return 0; }')
    return {str(p.relative_to(stage)):hashlib.sha256(p.read_bytes()).hexdigest()
        for p in [mods,stage/'src/pc/sdk/libetc.c',stage/'src/pc/platform/platform_common.c']}

def build(output,jobs):
    if platform.system()!='Darwin' or platform.machine()!='arm64':
        raise SystemExit('Use the ARM64 macOS Actions runner with Xcode.')
    from test_lekak_sdk import verify_sources
    verify_sources();output.mkdir(parents=True,exist_ok=True)
    stage=output/'sdk';stage.mkdir(exist_ok=True)
    patches=stage_sources(stage)
    # Host LLVM translator/dependencies stay macOS. Only game output targets iOS.
    cache=stage/'tmp';cache.symlink_to((SDK/'tmp').resolve(),target_is_directory=True) if not cache.exists() else None
    sys.path.insert(0,str(stage/'tools/pc'))
    import build_arm64 as engine
    from macos_deps import ensure
    from llvm_guest import toolchain
    deps=ensure(jobs=jobs);cc=str(toolchain()/'bin/clang')
    ios=subprocess.check_output(['xcrun','--sdk','iphoneos','--show-sdk-path'],text=True).strip()
    target=['-target','arm64-apple-ios15.0','-isysroot',ios]
    args=argparse.Namespace(jobs=jobs,optimize=True,instrument_softgpu=False)
    groups=engine.game_sources();descriptor=engine.TARGETS['macos']
    natives=engine.native_sources(descriptor,backend=None)
    excluded={'src/pc/guest/main.c','src/pc/mods/object_loader.c'}
    natives=[s for s in natives if s not in excluded]
    ordinary={s for s in natives if s in engine.ORDINARY or '/translated_' in s}
    ordinary.update({'src/pc/render/soft_gpu.c','src/pc/platform/native_platform.c','src/pc/platform/native_entry.c'})
    jobs_list=[(s,g) for g,ss in groups.items() for s in ss]+[(s,'native') for s in natives]
    for kind in ['raw','ir','obj','logs']:(output/kind).mkdir(exist_ok=True)
    def path_for(kind,source,suffix):return engine.unit_path(output,kind,source,suffix)
    flags=[*target,'-std=gnu11','-fms-extensions','-O0','-fno-strict-aliasing','-ffp-contract=off','-fwrapv',
        '-fno-stack-protector','-fcommon','-DMEMORIES_PC','-DMEMORIES_TRANSLATED','-DMEMORIES_IOS',
        '-D_LANGUAGE_C','-DLANGUAGE_C','-D_DARWIN_C_SOURCE','-I'+str(stage/'src'),
        '-I'+str(stage/'src/pc/platform'),'-I'+str(deps/'include'),'-I'+str(deps/'include/freetype2'),
        '-Wno-incompatible-pointer-types','-Wno-int-conversion','-Wno-implicit-function-declaration']
    import os
    os.chdir(stage)
    failures=engine.compile_sources(jobs_list,ordinary,path_for,cc,flags,['-include','src/pc/compat/pgxp_game.h'],args)
    report={'target':'arm64-apple-ios15.0','sources':len(jobs_list),'stage_patch_hashes':patches,
        'emit_failures':failures,'iphone_game_linked':False,'device_tested':False,
        'pending':['UIKit service controller','iOS FreeType/PNG dependency archives','final native game linkage','physical-device test']}
    (output/'ios-core-report.json').write_text(json.dumps(report,indent=2)+'\n')
    if failures:raise SystemExit('iOS frontend failures collected in logs: '+str(len(failures)))
    prepared=engine.prepare_guest_units(jobs_list,groups,natives,ordinary,path_for,engine.maps())
    raw,pins,storage,aliases,renames,rows,linked,host,native_defs=prepared
    # Override only this build driver's code-generation flags. Host translator
    # retains macOS SDK via its own llvm_guest/macos_deps modules.
    engine.darwin_flags=lambda sdk:target
    registrations,failures=engine.compile_guest_units(jobs_list,ordinary,raw,pins,path_for,cc,ios,args)
    report['transform_failures']=failures
    (output/'ios-core-report.json').write_text(json.dumps(report,indent=2)+'\n')
    if failures:raise SystemExit('iOS transforms failed: '+str(len(failures)))
    tables=engine.generate_function_tables(raw,linked,host,rows,pins,registrations,'lekak-ios-source-snapshot')
    table=output/'tables.c';table.write_text(tables)
    obj=output/'tables.o'
    subprocess.run([cc,*flags,'-c',str(table),'-o',str(obj)],check=True)
    from build_lekak_ios_objects import check_macho_ios
    exports=engine.generate_mod_exports(output,jobs_list,path_for,raw,aliases,linked,pins,cc,flags)
    objects=[path_for('obj',s,'.o') for s,g in jobs_list]+[obj,exports]
    for p in objects:check_macho_ios(p)
    archive=output/'libMemories_iPhone.a';archive.unlink(missing_ok=True)
    subprocess.run(['ar','rcs',str(archive),*[str(p) for p in objects]],check=True)
    report['archive_sha256']=hashlib.sha256(archive.read_bytes()).hexdigest()
    defined=set();undefined=set()
    for p in objects:
        for line in subprocess.check_output(['nm','-g',str(p)],text=True).splitlines():
            fields=line.split()
            if len(fields)<2:continue
            kind,name=fields[-2:];name=name[1:] if name.startswith('_') else name
            (undefined if kind=='U' else defined).add(name)
    report['unresolved_symbols']=sorted(undefined-defined)
    report['desktop_imports_remaining']=sorted((undefined-defined)&{'fork','execv','execvp','system','dlopen','exit'})
    report['scope']='Complete translated iOS core static archive; final app linkage pending'
    (output/'ios-core-report.json').write_text(json.dumps(report,indent=2)+'\n')
    print(report['scope'])

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,default=ROOT/'build/iphone-core');p.add_argument('--jobs',type=int,default=3)
    a=p.parse_args();build(a.output.resolve(),a.jobs)
