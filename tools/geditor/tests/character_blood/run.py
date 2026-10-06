#!/usr/bin/env python3
"""Validate imported bodies from an exported ROM without retaining its assets."""
import argparse
import os
from pathlib import Path
import struct
import subprocess
import tempfile

here=Path(__file__).resolve().parent
src=here.parents[1]/'src'
root=src.parents[2]
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('rom',type=Path)
args=parser.parse_args()
data=args.rom.read_bytes()
u=lambda at:struct.unpack_from('>I',data,at)[0]
m=data.index(b'GUDGEDITORMANIF\0')
bank=next(u(p+4) for p in range(m+24,m+24+u(m+20)*16,16) if data[p:p+4]==b'NPMD')
with tempfile.TemporaryDirectory(prefix='geditor-blood-') as folder:
    work=Path(folder)
    command=[os.environ.get('CC','cc'),'-O1','-g','-std=c99','-Wall','-Wextra','-Werror',
        '-Wno-unused-parameter','-ffunction-sections','-fdata-sections','-fsanitize=address,undefined',
        f'-I{here.parent/"image_import"}',f'-I{src}',f'-I{root}',str(here/'check.c')]
    command += [str(src/name) for name in ('modelload.c','modelmaterials.c','modelcompile.c','bgmaterial.c','bgrender.c')]
    subprocess.run(command+['-lm','-Wl,--gc-sections','-o',str(work/'check')],check=True)
    count=0
    for p in range(bank+16,bank+16+u(bank+4)*96,96):
        if u(p+84)!=1:continue
        name=data[p:p+64].split(b'\0')[0].decode('ascii')
        path=work/(name+'.bin');path.write_bytes(data[bank+u(p+64):bank+u(p+64)+u(p+68)])
        subprocess.run([str(work/'check'),str(path),str(work/(name+'-fixed.bin'))],check=True,
            env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1'))
        count+=1
    assert count,'No imported bodies in ROM'
