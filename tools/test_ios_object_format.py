"""Reject desktop/simulator objects before admitting them to the iOS archive."""
import struct
import tempfile
import unittest
from pathlib import Path
from build_lekak_ios_objects import check_macho_ios

def fixture(cpu=0x0100000c, platform=2, filetype=1):
    header=struct.pack('<8I',0xfeedfacf,cpu,0,filetype,1,24,0,0)
    return header+struct.pack('<6I',0x32,24,platform,15<<16,0,0)

class PlatformTests(unittest.TestCase):
    def check(self,data):
        with tempfile.TemporaryDirectory() as folder:
            path=Path(folder)/'unit.o'
            path.write_bytes(data)
            check_macho_ios(path)

    def test_accept_device(self):
        self.check(fixture())

    def test_reject_incompatible(self):
        for data in [fixture(platform=1),fixture(platform=7),
                     fixture(cpu=0x01000007),fixture(filetype=2),
                     fixture()[:-1],fixture()[:20],
                     fixture()[:36]+struct.pack('<I',4)+fixture()[40:]]:
            with self.subTest(data=data.hex()),self.assertRaises(ValueError):
                self.check(data)

    def test_accept_legacy_device_version_command(self):
        self.check(struct.pack('<8I',0xfeedfacf,0x0100000c,0,1,1,16,0,0)
                   +struct.pack('<4I',0x25,16,15<<16,0))

if __name__=='__main__':unittest.main()
