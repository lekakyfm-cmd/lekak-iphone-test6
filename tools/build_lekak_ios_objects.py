#!/usr/bin/env python3
"""Compile the complete Lekak source chain as static iPhoneOS ARM64 objects.

Requires the full translated game's ABI/export metadata and a macOS runner.
Does not build an IPA or claim a working UIKit game backend.
"""
import argparse,hashlib,json,platform,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
SDK=ROOT/'EngineSDK'

def check_macho_ios(path):
    import struct
    data=path.read_bytes()
    if len(data)<32 or data[:4]!=b'\xcf\xfa\xed\xfe':
        raise ValueError('Not a little-endian Mach-O64 object: '+str(path))
    if struct.unpack_from('<I',data,4)[0]!=0x0100000c:
        raise ValueError('Object is not ARM64: '+str(path))
    if struct.unpack_from('<I',data,12)[0]!=1:
        raise ValueError('Mach-O is not a relocatable object: '+str(path))
    commands,bytes_=struct.unpack_from('<II',data,16)
    end=32+bytes_
    if end>len(data):raise ValueError('Truncated load commands')
    offset=32;ios=False
    for _ in range(commands):
        if offset+8>end:raise ValueError('Truncated load-command header')
        command,size=struct.unpack_from('<II',data,offset)
        if size<8 or offset+size>end:raise ValueError('Invalid load-command size')
        if command==0x32:
            if size<24:raise ValueError('Truncated build-version command')
            if struct.unpack_from('<I',data,offset+8)[0]!=2:
                raise ValueError('Build platform is not iPhoneOS')
            ios=True
        if command==0x25:
            if size<16:raise ValueError('Truncated iPhoneOS minimum command')
            ios=True
        offset+=size
    if offset!=end or not ios:raise ValueError('Missing iPhoneOS build platform')

def build(game,output):
    if platform.system()!='Darwin' or platform.machine()!='arm64':
        raise SystemExit('Run on an ARM64 macOS runner with Xcode and the pinned LLVM toolchain.')
    required=['exports.txt','mod_signatures.json','summary.json']
    for name in required:
        if not (game/name).is_file():raise SystemExit('Missing full-game ABI metadata: '+str(game/name))
    sys.path.insert(0,str(SDK/'tools/pc'))
    from llvm_guest import toolchain,normalize,process
    from build_mod_arm64 import validate_imports,mod_pins
    from guest_build import maps,HOST_LIBC,CHECKED_LIBC
    from test_lekak_sdk import verify_sources
    verify_sources()
    compiler=str(toolchain()/'bin/clang')
    sdk=subprocess.check_output(['xcrun','--sdk','iphoneos','--show-sdk-path'],text=True).strip()
    target=['-target','arm64-apple-ios15.0','-isysroot',sdk]
    flags=[*target,'-std=gnu11','-fms-extensions','-O0','-fno-strict-aliasing',
        '-ffp-contract=off','-fwrapv','-fno-stack-protector','-fno-common',
        '-DMEMORIES_PC','-DMEMORIES_TRANSLATED','-DMEMORIES_MOD',
        '-D_LANGUAGE_C','-DLANGUAGE_C','-D_DARWIN_C_SOURCE',
        '-I'+str(SDK/'src'),'-I'+str(ROOT/'SourceMod/Lekak')]
    output.mkdir(parents=True,exist_ok=True)
    summary=json.loads((game/'summary.json').read_text())
    signatures=json.loads((game/'mod_signatures.json').read_text())
    exported=set((game/'exports.txt').read_text().split())
    aliases=dict(summary['aliases'])
    aliases.update(dict(line.split() for line in (SDK/'config/pc/host_symbol_renames.txt').read_text().splitlines() if line.strip()))
    aliases.update(rand='Mods_ModRand',srand='Mods_ModSrand')
    pins=mod_pins(summary,maps().values())
    plan=json.loads((ROOT/'SourceMod/Lekak/native-units.json').read_text())
    objects=[];routines=[];entries=0;units=[]
    for index,unit in enumerate(plan['units']):
        source=ROOT/'SourceMod/Lekak'/unit['source']
        raw=output/f'{index}.raw.ll';ir=output/f'{index}.ll';obj=output/f'{index}.o'
        subprocess.run([compiler,*flags,'-S','-emit-llvm',str(source),'-o',str(raw)],check=True)
        names=dict(aliases)
        if unit.get('rename_entry'):names['MemoriesModInit']=unit['rename_entry']
        text=normalize(raw.read_text(),names)
        # Check every known game import against actual translated-game types.
        entries+=bool(validate_imports(text,signatures))
        routine=f'LekakStaticRegisterUnit_{index}'
        text=normalize(text,{name:'GuestRuntime_'+name for name in HOST_LIBC|CHECKED_LIBC|{'malloc','calloc','realloc','free'}})
        text=process('translate',text,pins=pins,registration=routine,mod_unit=True)
        ir.write_text(text)
        subprocess.run([compiler,*target,'-O2','-fno-strict-aliasing','-c',str(ir),'-o',str(obj)],check=True)
        check_macho_ios(obj)
        objects.append(obj);routines.append(routine)
        units.append({'source':unit['source'],'source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),
            'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest()})
    if entries!=1:raise ValueError('Expected exactly one final MemoriesModInit')
    registration=output/'registration.c'
    registration.write_text(''.join(f'extern void {name}(void); extern void {name}_Unregister(void);\n' for name in routines)
        +'void LekakStatic_RegisterGlobals(void) {\n'+''.join(f' {name}();\n' for name in routines)+'}\n'
        +'void LekakStatic_UnregisterGlobals(void) {\n'+''.join(f' {name}_Unregister();\n' for name in reversed(routines))+'}\n')
    reg=output/'registration.o'
    subprocess.run([compiler,*target,'-O2','-c',str(registration),'-o',str(reg)],check=True)
    check_macho_ios(reg);objects.append(reg)
    defined=set();undefined=set()
    for obj in objects:
        lines=subprocess.check_output(['nm','-g',str(obj)],text=True).splitlines()
        for line in lines:
            fields=line.split()
            if len(fields)<2:continue
            kind,name=fields[-2:];name=name[1:] if name.startswith('_') else name
            if kind=='U':undefined.add(name)
            else:defined.add(name)
    # These are native libSystem stdio calls on the host FILE returned by
    # MemoriesModHost.open_data; they are not translated game exports.
    missing=undefined-defined-exported-{'abort','dyld_stub_binder','fclose','fwrite'}
    if missing:raise ValueError('Game does not export required imports: '+', '.join(sorted(missing)))
    archive=output/'libLekak_iPhone.a'
    archive.unlink(missing_ok=True)
    subprocess.run(['ar','rcs',str(archive),*[str(p) for p in objects]],check=True)
    report={'scope':'ARM64 iPhoneOS static Lekak objects; final game backend linkage pending',
        'target':'arm64-apple-ios15.0','units':units,'entrypoint':'MemoriesModInit',
        'undefined_imports':sorted(undefined-defined),'game_summary_sha256':hashlib.sha256((game/'summary.json').read_bytes()).hexdigest(),
        'archive_sha256':hashlib.sha256(archive.read_bytes()).hexdigest(),
        'iphone_game_linked':False,'device_tested':False}
    (output/'build-report.json').write_text(json.dumps(report,indent=2)+'\n')
    print('Static iPhoneOS ARM64 archive built; final game backend and device validation remain pending.')

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--game-build',type=Path,required=True)
    parser.add_argument('--output',type=Path,default=ROOT/'build/lekak-ios')
    args=parser.parse_args();build(args.game_build.resolve(),args.output.resolve())
