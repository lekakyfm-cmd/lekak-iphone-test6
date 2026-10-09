#!/usr/bin/env python3
"""Build pinned image/font libraries for physical iOS, never use Mac archives."""
import argparse,hashlib,json,os,platform,shutil,subprocess,sys,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
SDK=ROOT/'EngineSDK'
NAMES={'zlib':'libz.a','libpng':'libpng16.a','freetype':'libfreetype.a'}

def verify_archive(archive):
    from build_lekak_ios_objects import check_macho_ios
    # Inspect all members, not just the architecture of the archive container.
    with tempfile.TemporaryDirectory() as folder:
        members=subprocess.check_output(['ar','-t',str(archive)],text=True).splitlines()
        if not members:raise RuntimeError('Empty dependency archive: '+str(archive))
        subprocess.run(['ar','-x',str(archive)],cwd=folder,check=True)
        objects=list(Path(folder).glob('*.o'))
        if not objects:raise RuntimeError('No dependency objects: '+str(archive))
        for obj in objects:check_macho_ios(obj)

def build(output,jobs):
    if platform.system()!='Darwin' or platform.machine()!='arm64':
        raise SystemExit('Requires ARM64 macOS with Xcode.')
    sys.path.insert(0,str(SDK/'tools/pc'))
    from macos_deps import source
    from dependency_archives import ARCHIVES
    from fetch_tools import ensure
    from llvm_guest import toolchain
    llvm=toolchain();cmake=ensure('cmake',pinned=True);ninja=ensure('ninja',pinned=True)
    ios=subprocess.check_output(['xcrun','--sdk','iphoneos','--show-sdk-path'],text=True).strip()
    install=output/'install';install.mkdir(parents=True,exist_ok=True)
    options={
        'zlib':['-DZLIB_BUILD_SHARED=OFF','-DZLIB_BUILD_TESTING=OFF','-DBUILD_SHARED_LIBS=OFF'],
        'libpng':['-DPNG_SHARED=OFF','-DPNG_STATIC=ON','-DPNG_TESTS=OFF','-DPNG_TOOLS=OFF','-DPNG_FRAMEWORK=OFF',
            '-DZLIB_LIBRARY='+str(install/'lib/libz.a'),'-DZLIB_INCLUDE_DIR='+str(install/'include')],
        'freetype':['-DBUILD_SHARED_LIBS=OFF',*['-DFT_DISABLE_'+x+'=ON' for x in ['ZLIB','BZIP2','PNG','HARFBUZZ','BROTLI']]]}
    environment={k:v for k,v in os.environ.items() if k not in
        ['CPATH','C_INCLUDE_PATH','CPLUS_INCLUDE_PATH','LIBRARY_PATH','CFLAGS','CXXFLAGS','LDFLAGS','SDKROOT','MACOSX_DEPLOYMENT_TARGET']}
    identity=subprocess.check_output([str(llvm/'bin/clang'),'--version'],text=True)
    report={'target':'arm64-apple-ios15.0','libraries':{}}
    for name,library in NAMES.items():
        directory=source(name);tree=output/'build'/name;stamp=tree/'complete.json'
        command=[str(cmake),'-S',str(directory),'-B',str(tree),'-G','Ninja',
            '-DCMAKE_MAKE_PROGRAM='+str(ninja),'-DCMAKE_SYSTEM_NAME=iOS','-DCMAKE_BUILD_TYPE=Release',
            '-DCMAKE_C_COMPILER='+str(llvm/'bin/clang'),'-DCMAKE_CXX_COMPILER='+str(llvm/'bin/clang++'),
            '-DCMAKE_C_COMPILER_TARGET=arm64-apple-ios15.0','-DCMAKE_CXX_COMPILER_TARGET=arm64-apple-ios15.0',
            '-DCMAKE_OSX_ARCHITECTURES=arm64','-DCMAKE_OSX_DEPLOYMENT_TARGET=15.0','-DCMAKE_OSX_SYSROOT='+ios,
            '-DCMAKE_INSTALL_PREFIX='+str(install),'-DCMAKE_PREFIX_PATH='+str(install),
            '-DCMAKE_FIND_ROOT_PATH='+str(install)+';'+ios,
            '-DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=ONLY','-DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE=ONLY',
            '-DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=ONLY','-DCMAKE_FIND_ROOT_PATH_MODE_PROGRAM=NEVER',
            '-DCMAKE_FIND_USE_CMAKE_ENVIRONMENT_PATH=OFF','-DCMAKE_FIND_USE_SYSTEM_ENVIRONMENT_PATH=OFF',
            '-DCMAKE_FIND_USE_PACKAGE_REGISTRY=OFF','-DCMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY=OFF',
            '-DCMAKE_SYSTEM_PREFIX_PATH='+str(install)+';/usr','-DPKG_CONFIG_EXECUTABLE=/usr/bin/false',
            '-DCMAKE_POLICY_VERSION_MINIMUM=3.5',*options[name]]
        fingerprint=json.dumps({'command':command,'source':ARCHIVES[name][1],'clang':identity,
            'builder':hashlib.sha256(Path(__file__).read_bytes()).hexdigest()},sort_keys=True)
        archive=install/'lib'/library
        if not(stamp.is_file() and stamp.read_text()==fingerprint and archive.is_file()):
            shutil.rmtree(tree,ignore_errors=True)
            subprocess.run(command,env=environment,check=True)
            subprocess.run([str(cmake),'--build',str(tree),'--parallel',str(jobs)],env=environment,check=True)
            subprocess.run([str(cmake),'--install',str(tree)],env=environment,check=True)
            if not archive.is_file():raise RuntimeError('Missing '+str(archive))
            stamp.write_text(fingerprint)
        verify_archive(archive)
        report['libraries'][name]={'source_sha256':ARCHIVES[name][1],
            'archive_sha256':hashlib.sha256(archive.read_bytes()).hexdigest(),'platform':'physical iOS arm64'}
    (output/'ios-dependencies-report.json').write_text(json.dumps(report,indent=2)+'\n')
    return install

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,default=SDK/'tmp/pc/ios-deps');p.add_argument('--jobs',type=int,default=3)
    a=p.parse_args();build(a.output.resolve(),a.jobs)
