#!/usr/bin/env python3
"""Exercise actual marker parsing and GLB decoding under ASan/UBSan."""
import json
import math
import os
from pathlib import Path
import re
import struct
import subprocess
import tempfile


def main():
    here=Path(__file__).resolve().parent
    editor=here.parent.parent
    src=editor/'src'
    shim=here.parent/'image_import'
    vertices=struct.pack('<9f',1,0,0,0,1,0,0,0,1)
    normals=struct.pack('<9f', *([math.sqrt(.5),math.sqrt(.5),0]*3))
    colors=struct.pack('<12H', *([65535,32768,0,65535]*3))
    binary=vertices+normals+colors
    scene={'asset':{'version':'2.0'},'scene':0,'scenes':[{'nodes':[0]}],
           'nodes':[{'scale':[2,3,4],'translation':[10,20,30],'children':[1]},
                    {'name':'marker','rotation':[0,math.sqrt(.5),0,math.sqrt(.5)],'mesh':0}],
           'meshes':[{'primitives':[{'attributes':{'POSITION':0,'NORMAL':1,'COLOR_0':2}}]}],
           'buffers':[{'byteLength':len(binary)}],
           'bufferViews':[{'buffer':0,'byteOffset':0,'byteLength':36},
                          {'buffer':0,'byteOffset':36,'byteLength':36},
                          {'buffer':0,'byteOffset':72,'byteLength':24}],
           'accessors':[{'bufferView':0,'componentType':5126,'count':3,'type':'VEC3'},
                        {'bufferView':1,'componentType':5126,'count':3,'type':'VEC3'},
                        {'bufferView':2,'componentType':5123,'normalized':True,'count':3,'type':'VEC4'}]}
    with tempfile.TemporaryDirectory(prefix='geditor-setup-markers-') as temp:
        temp=Path(temp)
        text=json.dumps(scene).encode();text+=b' '*(-len(text)%4)
        glb=struct.pack('<III',0x46546c67,2,28+len(text)+len(binary))
        glb+=struct.pack('<II',len(text),0x4e4f534a)+text+struct.pack('<II',len(binary),0x004e4942)+binary
        fixture=temp/'transform.glb';fixture.write_bytes(glb)
        command=[os.environ.get('CC','cc'),'-O1','-g','-std=c99','-Wall','-Wextra',
                 '-ffunction-sections','-fdata-sections','-fsanitize=address,undefined',
                 f'-I{shim}',f'-I{src}',f'-I{editor.parent.parent}',str(here/'check.c'),str(shim/'platform.c')]
        command += [str(src/name) for name in ['setupload.c','gltf.c','bgrender.c','rotation.c','scaling.c','bghistory.c']]
        command += [str(here/'edits.c')]
        command += ['-Wl,--gc-sections','-lm','-o',str(temp/'check')]
        subprocess.run(command,check=True)
        models=[str(editor/'geditorassets'/name) for name in ['start.glb','intro_camera.glb','outro_camera.glb','intro_spline_point.glb']]
        # Exercise the actual authored fixed-point records as well as the
        # synthetic cases. No ROM or proprietary extracted project is needed.
        missions=[]
        for source in sorted((editor.parent.parent/'assets/obseg/setup').glob('Usetup*.c')):
            records=re.findall(r'/\* Type = SwirlCam;.*?\*/\s*_mkword\(0, _mkshort\(0, 3\)\),([^\n]+)',source.read_text())
            if not records:
                continue
            data=bytearray(40);struct.pack_into('>I',data,8,40)
            for record in records:
                words=[3]+[int(word.strip(),0)&0xffffffff for word in record.split(',') if word.strip()]
                assert len(words)==8
                data+=struct.pack('>8I',*words)
            data+=struct.pack('>I',9)
            mission=temp/(source.stem+'.set');mission.write_bytes(data);missions.append(str(mission))
        assert len(missions)==20
        subprocess.run([str(temp/'check'),*models,str(fixture),str(temp),*missions],check=True,
                       env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1'))


if __name__=='__main__':
    main()
