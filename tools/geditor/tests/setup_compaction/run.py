#!/usr/bin/env python3
"""Verify native setup reachability and shrinking ROM slots. Optional: GUD ROM.

The independent reader compares every active native section, including record
order, numeric references, shared script suffixes and strings inside old tables.
"""
import importlib.util
import os
from pathlib import Path
import re
import struct
import subprocess
import sys
import tempfile

here = Path(__file__).resolve().parent
src = here.parents[1] / 'src'
root = src.parents[2]
words = {1:64, 2:2, 3:32, 4:33, 5:32, 6:59, 7:33, 8:34, 9:7, 10:64,
         11:149, 12:32, 13:54, 14:3, 17:32, 18:3, 19:4, 20:44, 21:34,
         22:4, 23:4, 24:1, 25:2, 26:2, 27:2, 28:2, 29:2, 30:4, 31:1,
         32:4, 33:5, 34:3, 35:4, 36:32, 37:10, 38:4, 39:44, 40:45,
         42:32, 43:32, 44:5, 45:56, 46:7, 47:37}
intro = [3,4,4,8,2,2,10,3,2,1]
# Repository command descriptions are the authoritative bytecode widths.
widths = [int(x) for x in re.findall(r'^\s*\{\s*"[^"]*",\s*"[^"]*",\s*(\d+)',
                                     (src / 'actionopcodes.inc').read_text(), re.M)]
assert len(widths) == 253


def signature(b):
    u = lambda a: struct.unpack_from('>I', b, a)[0]
    def string(a): return b[a:b.index(0,a)] if a else None
    def indices(a):
        result=[]
        while a:
            v=u(a);result.append(v);a+=4
            if v & 0x80000000: break
        return result
    def script(a):
        start=a
        while True:
            op=b[a];a += b.index(0,a+1)-a+1 if op==0xad else widths[op]
            if op==4: return b[start:a]
    result=[]
    for rootid in range(10):
        at=u(rootid*4);records=[]
        if not at: result.append(None);continue
        while True:
            v=u(at)
            if rootid==0:
                if v & 0x80000000:break
                records.append((v,indices(u(at+4)),b[at+8:at+16]));at+=16
            elif rootid==1:
                if not v:break
                records.append((indices(v),indices(u(at+4)),b[at+8:at+12]));at+=12
            elif rootid==2:
                if v==9:break
                n=intro[v]*4
                if v==8:
                    p=u(at+4);start=p
                    while u(p):p+=12
                    records.append((8,b[start:p+12]))
                else:records.append(b[at:at+n])
                at+=n
            elif rootid==3:
                if v&255==48:break
                n=words.get(v&255,1)*4;records.append(b[at:at+n]);at+=n
            elif rootid in (4,5):
                if not v:break
                records.append((b[at+4:at+8],indices(v) if rootid==4 else script(v)));at+=8
            elif rootid in (6,7):
                if not u(at+36):break
                n=44 if rootid==6 else 68
                records.append((b[at:at+36],string(u(at+36)),b[at+40:at+n]));at+=n
            else:
                if not v:break
                records.append(string(v));at+=4
        result.append(records)
    return result


def fixture():
    b=bytearray(4096)
    def put(at,*values):struct.pack_into('>'+'I'*len(values),b,at,*values)
    roots=[0x100,0x140,0x180,0x1c0,0x200,0x240,0x300,0x380,0x420,0x450]
    put(0,*roots);put(0x100,17,0x800,3,4);put(0x110,0xffffffff)
    put(0x140,0x800,0x820,5);put(0x180,8,0x840,0,1,0,9)
    put(0x1c0,9,0x10002,0x1000401);put(0x1dc,48)
    put(0x200,0x820,0x1020000);put(0x240,0x880,0x401,0x882,0x402)
    put(0x324,0x350);put(0x3a4,0x8c0);put(0x420,0x8c0,0x8c1,0);put(0x450,0x8c0,0)
    put(0x800,1,0xffffffff);put(0x820,0,0xffffffff);put(0x840,0x12340000)
    b[0x880:0x887]=bytes([2,1,0xad,ord('x'),0,3,4]);b[0x8c0:0x8c5]=b'p12a\0'
    b[0x900:]=b'\xcc'*(len(b)-0x900) # unreachable obsolete tables
    return bytes(b)


def main():
    spec=importlib.util.spec_from_file_location('extract',here.parent/'portal_editing/run.py')
    ex=importlib.util.module_from_spec(spec);spec.loader.exec_module(ex)
    a=(src/'romexport.c').read_text()
    code='#include <stdarg.h>\n'+re.search(r'typedef struct RomExportSlot \{.*?\} RomExportSlot;',a,re.S)[0]
    code+='\n#define ROM_EXPORT_FTBL_MAX_ROWS 1024u\n#define ROM_EXPORT_MAX_SIZE (64u*1024u*1024u)\n'
    for name in ['RomExportRead32','RomExportWrite32','RomExportCompareSlots','RomExportFindSlot',
                 'RomExportAddSize','RomExportAlignSize','RomExportUpdateFileOffsets',
                 'RomExportNextRomSize','RomExportRepackResources']:
        code+=ex.function(a,name)
    corpus=[('synthetic',fixture())]
    if len(sys.argv)>1:
        rom=Path(sys.argv[1]).read_bytes();m=rom.index(b'GUDGEDITORMANIF\0');entries={}
        for i in range(struct.unpack_from('>I',rom,m+20)[0]):
            at=m+24+i*16;entries[rom[at:at+4]]=struct.unpack_from('>III',rom,at+4)
        cm=entries[b'CMAP'];ft=entries[b'FTBL'][0];rows=[]
        while True:
            ident,name,off=struct.unpack_from('>III',rom,ft);ft+=12
            if not name:break
            n=name-cm[2]+cm[0];rows.append((rom[n:rom.index(0,n)].decode(),off))
        offsets=sorted({off for name,off in rows}|{entries[b'OBSG'][1]})
        for name,off in rows:
            if name.startswith(('Usetup','Ump_setup')):
                end=next(v for v in offsets if v>off)
                if end-off>=40:corpus.append((name,rom[off:end]))
    with tempfile.TemporaryDirectory(prefix='geditor-setup-compact-') as folder:
        work=Path(folder);(work/'repack.inc').write_text(code);binary=work/'check'
        subprocess.run([os.environ.get('CC','cc'),'-O1','-g','-std=c99','-Wall','-Wextra','-Werror',
            '-Wno-unused-parameter','-ffunction-sections','-fdata-sections','-fsanitize=address,undefined',
            f'-I{here.parent/"image_import"}',f'-I{src}',f'-I{root}',f'-I{work}',str(here/'check.c'),
            str(src/'setupload.c'),str(src/'actionblocks.c'),'-Wl,--gc-sections','-lm','-o',str(binary)],check=True)
        saved=0
        for name,b in corpus:
            before=work/'before';after=work/'after';before.write_bytes(b)
            subprocess.run([str(binary),str(before),str(after)],check=True,
                env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1'))
            packed=after.read_bytes();assert signature(b)==signature(packed),name
            saved+=len(b)-len(packed)
        print(f'PASS: {len(corpus)} setups preserve every active section; {saved} bytes reclaimed. '
              'Idempotence, invalid pointers, slot shrink, aliases, manifest end, and ASan/UBSan passed.')

if __name__=='__main__':main()
