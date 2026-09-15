#!/usr/bin/env python3
"""Compare the optimized traversal to its predecessor on native model trees."""
from pathlib import Path
import os
import re
import shlex
import struct
import subprocess
import tempfile
import zlib

HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[2]


def function(source,name):
    match=re.search(r'^(?:static )?[\w *]+\b'+name+r'\([^;]*?\)\s*\{',source,re.M)
    assert match,name
    end=source.index('{',match.start())+1; depth=1
    while depth:
        depth+=(source[end]=='{')-(source[end]=='}'); end+=1
    return source[match.start():end]+'\n'


def fixtures():
    out=[]; names=[]
    for path in sorted((ROOT/'assets/obseg/chr').glob('C*Z.bin')):
        header=path.parent/path.stem[1:-1]/'modelFileHeader.inc.c'
        if not header.exists(): continue
        args=re.search(r'MODELFILEHEADER\((.*)\)',header.read_text())[1].split(',')
        data=path.read_bytes()
        if data[:2]==b'\x11\x72': data=zlib.decompress(data[2:],-15)
        word=lambda o:struct.unpack_from('>I',data,o)[0]
        half=lambda o:struct.unpack_from('>H',data,o)[0]
        ptr=lambda o:word(o)&0xffffff
        root=int(args[4],0)*4+int(args[8],0)*12
        nodes=[]; pending=[root]
        while pending:
            n=pending.pop()
            if n in nodes: continue
            assert n+24<=len(data)
            nodes.append(n)
            for offset in (12,20):
                if word(n+offset): pending.append(ptr(n+offset))
        indexes={p:i for i,p in enumerate(nodes)}
        index=lambda p:indexes[p] if p else -1
        rows=[]
        attached=(half(root)&255) not in (1,2,3,21)
        if attached: rows.append('{1,-1,-1,-1,1,0,-1,0,0}')
        for n in nodes:
            op=half(n); ro=ptr(n+4); typ=op&255; matrix=0; affects=-1; lo=hi=0.
            if typ==1: matrix=half(ro+2)
            elif typ in (2,3): matrix=half(ro+14)
            elif typ==21: matrix=half(ro+12)
            elif typ==8:
                lo,hi=struct.unpack_from('>ff',data,ro); affects=index(ptr(ro+8))
            elif typ==18: affects=index(ptr(ro))
            links=[index(ptr(n+8)),index(ptr(n+12)),index(ptr(n+16)),index(ptr(n+20))]
            if attached:
                links=[v+1 if v>=0 else -1 for v in links]
                if links[0]<0: links[0]=0
                if affects>=0: affects+=1
            rows.append('{'+','.join(map(str,[op,*links,matrix,affects]))
                        +','+lo.hex()+'f,'+hi.hex()+'f}')
        name='asset'+str(len(names)); out.append('static const NodeSpec '+name+'[]={\n'+',\n'.join(rows)+'\n};\n')
        names.append('{"'+path.stem+'",'+name+','+str(len(rows))+','+str(0 if attached else index(root))+'}')
    out.append('static const Asset assets[]={\n'+',\n'.join(names)+'\n};\n')
    return ''.join(out)


def main():
    source=(ROOT/'src/game/model.c').read_text()
    constants=(ROOT/'src/bondconstants.h').read_text()
    text='#include <stdint.h>\n'+re.search(r'typedef enum MODELNODE_OPCODE\s*\{.*?\}\s*\w+;',constants,re.S)[0]+'\n'
    text+=(HERE/'harness.h').read_text()
    text+=re.search(r'typedef struct ModelDistanceCache \{.*?\} ModelDistanceCache;',source,re.S)[0]+'\n'
    for name in ('modelFindNodeMtxIndex','modelFindNodeMtx','modelGetNodeRwData','modelUpdateDistanceRelations',
                 'modelApplyToggleRelations','modelApplyHeadRelations','modelUpdateDistanceRelationsCached','modelUpdateMatrices'):
        f=function(source,name)
        if name=='modelFindNodeMtxIndex':
            f=f.replace('s32 index;','s32 index; matrixlookups++;').replace('node = node->Parent;','matrixparents++; node = node->Parent;')
        if name=='modelGetNodeRwData':
            f=f.replace('s32 index  = 0;','s32 index  = 0; rwlookups++;').replace('root = root->Parent;','rwparents++; root = root->Parent;')
        if name=='modelUpdateMatrices': f=f.replace('u32 type = node->Opcode & 0xff;','u32 type = node->Opcode & 0xff; Trace(node);')
        text+=f
    text+=(HERE/'reference.c').read_text().replace('u32 type = node->Opcode & 0xff;','u32 type = node->Opcode & 0xff; Trace(node);')
    text+=fixtures()+(HERE/'check.c').read_text()
    with tempfile.TemporaryDirectory(prefix='gud-character-lod-') as tmp:
        work=Path(tmp); (work/'check.c').write_text(text)
        cmd=shlex.split(os.environ.get('CC','cc'))+['-std=c99','-O2','-g','-Wall','-Wextra','-Werror',
            '-Wno-unused-variable','-Wno-unused-parameter','-Wno-incompatible-pointer-types','-fno-strict-aliasing','-ffp-contract=off']
        cmd+=shlex.split(os.environ.get('TEST_CFLAGS',''))
        subprocess.run(cmd+[str(work/'check.c'),'-lm','-o',str(work/'check')],check=True)
        env=dict(os.environ)
        env.setdefault('ASAN_OPTIONS','detect_leaks=0')
        env.setdefault('UBSAN_OPTIONS','halt_on_error=1')
        subprocess.run([str(work/'check')],check=True,env=env)


if __name__=='__main__': main()
