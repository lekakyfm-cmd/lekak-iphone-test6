import subprocess,tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='native-platform-',dir=root/'tools') as folder:
    exe=Path(folder)/'test'
    subprocess.run(['gcc','-std=c11','-Wall','-Wextra','-Werror','-Wno-misleading-indentation',
        '-I'+str(root/'App'),'-I'+str(root/'EngineSDK/src'),
        str(root/'App/native_platform.c'),str(root/'tools/test_native_platform.c'),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
print('Native platform: cropped VRAM/RGB24/HD pictures, pad release and service lifecycle passed.')
