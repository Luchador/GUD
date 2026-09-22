#!/usr/bin/env python3
"""Native topology replacement and edit-store tests; optional original GLTF/new GLB."""
import base64
import copy
import json
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
here=Path(__file__).resolve().parent
src=here.parents[1]/'src'
root=src.parents[2]
with tempfile.TemporaryDirectory(prefix='geditor-topology-') as folder:
    work=Path(folder);binary=work/'check'
    command=[os.environ.get('CC','cc'),'-O1','-g','-std=c99','-Wall','-Wextra','-Werror','-Wno-unused-parameter',
        '-ffunction-sections','-fdata-sections','-fsanitize=address,undefined','-Dfopen=TestFopen',
        f'-I{here.parent/"image_import"}',f'-I{src}',f'-I{root}',str(here/'check.c'),str(here.parent/'image_import/platform.c')]
    command += [str(src/name) for name in ('modelload.c','modelmaterials.c','modelcompile.c','gltf.c','modeledits.c','bgmaterial.c','bgrender.c','newprops.c','propcompile.c')]
    subprocess.run(command+['-lm','-Wl,--gc-sections','-o',str(binary)],check=True)
    env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1')
    asset=root/'assets/obseg/prop/PsevdoormetslideZ.bin'
    sequence=0
    def run(path,mode='import',native=asset):
        global sequence
        sequence+=1;project=work/f'project{sequence}'
        (project/'models/objects').mkdir(parents=True)
        subprocess.run([str(binary),str(native),str(path),str(project),mode],env=env,check=True)
    original=work/'original.gltf';run(original,'export')
    doc=json.loads(original.read_text())
    raw=base64.b64decode(doc['buffers'][0]['uri'].split(',')[1])
    def accessor(document,data,values,kind,components):
        data+=bytes(-len(data)%4);offset=len(data)
        fmt={5123:'H',5126:'f'}[kind]
        for value in values:data+=struct.pack('<'+fmt*components,*value)
        index=len(document['accessors']);view=len(document['bufferViews'])
        document['accessors'].append({'bufferView':view,'componentType':kind,'count':len(values),'type':{1:'SCALAR',2:'VEC2',3:'VEC3',4:'VEC4'}[components]})
        document['bufferViews'].append({'buffer':0,'byteOffset':offset,'byteLength':len(data)-offset})
        return index
    def write(name,document,data,glb=False):
        document=copy.deepcopy(document);document['buffers'][0]['byteLength']=len(data)
        if glb:
            document['buffers'][0].pop('uri',None);j=json.dumps(document).encode();j+=b' '*(-len(j)%4);b=bytes(data)+bytes(-len(data)%4)
            payload=struct.pack('<III',0x46546c67,2,28+len(j)+len(b))+struct.pack('<II',len(j),0x4e4f534a)+j+struct.pack('<II',len(b),0x004e4942)+b
        else:
            document['buffers'][0]['uri']='data:application/octet-stream;base64,'+base64.b64encode(data).decode();payload=json.dumps(document).encode()
        path=work/name;path.write_bytes(payload);return path
    simplified=copy.deepcopy(doc);data=bytearray(raw)
    for mesh in simplified['meshes']:
        for primitive in mesh['primitives']:
            primitive['indices']=accessor(simplified,data,[(i,) for i in [0,1,2,3,4,5,0,2,1,3,5,4]],5123,1)
    run(write('reversed.gltf',simplified,data))
    missing=copy.deepcopy(simplified)
    for mesh in missing['meshes']:
        for primitive in mesh['primitives']:primitive['attributes'].pop('_GUD_VERTEX')
    run(write('no-identities.glb',missing,data,True))
    fractional=copy.deepcopy(simplified);fractionaldata=bytearray(data)
    for mesh in fractional['meshes']:
        for primitive in mesh['primitives']:
            n=fractional['accessors'][primitive['attributes']['POSITION']]['count']
            primitive['attributes']['_GUD_VERTEX']=accessor(fractional,fractionaldata,[(i+.25,) for i in range(n)],5126,1)
    run(write('interpolated.gltf',fractional,fractionaldata))
    grown=copy.deepcopy(missing);grown_data=bytearray(data);p=grown['meshes'][0]['primitives'][0]
    positions=[v for i in range(50) for v in [(i*3,0,0),(i*3+2,0,0),(i*3,3,0)]]
    p['attributes']['POSITION']=accessor(grown,grown_data,positions,5126,3)
    p['attributes']['COLOR_0']=accessor(grown,grown_data,[(1,.5,.25,1)]*len(positions),5126,4)
    p['attributes']['TEXCOORD_0']=accessor(grown,grown_data,[(0,0),(1,0),(0,1)]*50,5126,2)
    p.pop('indices');run(write('new-vertices.glb',grown,grown_data,True))
    for kind in ['oversize','collapsed']:
        bad=copy.deepcopy(missing);bad_data=bytearray(data);p=bad['meshes'][0]['primitives'][0]
        n=bad['accessors'][p['attributes']['POSITION']]['count']
        positions=[(40000,0,0)]*n if kind=='oversize' else [(0,0,0)]*n
        p['attributes']['POSITION']=accessor(bad,bad_data,positions,5126,3)
        run(write(f'{kind}.glb',bad,bad_data,True),'reject')
    unmatched=copy.deepcopy(missing);unmatched['materials'][0]['name']='Unknown part';run(write('unknown.gltf',unmatched,data),'reject')
    stale=copy.deepcopy(missing)
    def set_hash(value,number):
        if isinstance(value,dict):
            for k,v in value.items():
                if k=='goldeneyeSourceHash':value[k]=f'{number:08X}'
                else:set_hash(v,number)
        elif isinstance(value,list):
            for v in value:set_hash(v,number)
    set_hash(stale,0);run(write('stale.gltf',stale,data),'reject')
    for kind in ['lit','linked','cache','matrix']:
        native=bytearray(asset.read_bytes())
        if kind=='lit':struct.pack_into('>II',native,0x838,0xb7000000,0x20000)
        elif kind=='linked':struct.pack_into('>I',native,0x598+8,0x05000004)
        elif kind=='cache':struct.pack_into('>II',native,0x838,0xb2000000,0)
        else:
            # This door has no matrix group 1; the import must reject its use.
            struct.pack_into('>II',native,0x890,0x01020040,0x03000040)
        path=work/f'{kind}.bin';path.write_bytes(native)
        h=2166136261
        for byte in native:h=((h^byte)*16777619)&0xffffffff
        changed=copy.deepcopy(missing);set_hash(changed,h)
        run(write(f'{kind}.gltf',changed,data),'reject',path)
    # Optional user's files are read only and never embedded in the patch.
    for path in sys.argv[1:]:
        if Path(path).suffix=='.glb':run(Path(path))
        else:
            supplied=json.loads(Path(path).read_text());assert len(supplied['meshes'])==2
            assert sum(supplied['accessors'][p['attributes']['POSITION']]['count']//3 for m in supplied['meshes'] for p in m['primitives'])==40
    print('All model topology checks passed (ASan + UBSan).')
