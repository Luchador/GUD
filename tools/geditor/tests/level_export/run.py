#!/usr/bin/env python3
"""Real level exporters; inspect GLB structure, materials and exact geometry."""
import argparse
import importlib.util
import json
import os
from pathlib import Path
import struct
import subprocess
import tempfile
import zlib

def png(opaque):
    def chunk(tag,data):
        return struct.pack('>I',len(data))+tag+data+struct.pack('>I',zlib.crc32(tag+data))
    pixels = bytes([32,64,128,255 if opaque else 64,32,64,128,255])
    pixels += bytes([32,64,128,255]*2)
    return b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',2,2,8,6,0,0,0))+chunk(b'IDAT',zlib.compress(b'\0'+pixels[:8]+b'\0'+pixels[8:]))+chunk(b'IEND',b'')

def read_glb(path):
    raw=path.read_bytes()
    assert struct.unpack_from('<III',raw)==(0x46546c67,2,len(raw))
    length,kind=struct.unpack_from('<II',raw,12)
    assert kind==0x4e4f534a and length%4==0
    doc=json.loads(raw[20:20+length])
    start=20+length
    size,kind=struct.unpack_from('<II',raw,start)
    binary=raw[start+8:]
    assert kind==0x004e4942 and size==len(binary) and size%4==0
    assert doc['buffers']==[{'byteLength':len(binary)}]
    assert 'goldeneyeSourceHash' not in str(doc)
    for view in doc['bufferViews']:
        assert view['buffer']==0 and view.get('byteOffset',0)%4==0
        assert view.get('byteOffset',0)+view['byteLength']<=len(binary)
    for node in doc['nodes']:
        assert node['mesh']<len(doc['meshes'])
    for accessor in doc['accessors']:
        view=doc['bufferViews'][accessor['bufferView']]
        components={'SCALAR':1,'VEC2':2,'VEC3':3,'VEC4':4}[accessor['type']]
        component_size={5121:1,5126:4}[accessor['componentType']]
        stride=view.get('byteStride',components*component_size)
        assert accessor.get('byteOffset',0)+(accessor['count']-1)*stride+components*component_size<=view['byteLength']
    for image in doc.get('images',[]):
        assert image['mimeType']=='image/png' and 'uri' not in image
        view=doc['bufferViews'][image['bufferView']]
        encoded=binary[view['byteOffset']:view['byteOffset']+view['byteLength']]
        assert encoded in (png(False),png(True))
    return doc,binary

def values(doc,binary,index):
    a=doc['accessors'][index];v=doc['bufferViews'][a['bufferView']]
    components={'VEC2':2,'VEC3':3,'VEC4':4}[a['type']]
    fmt='<'+({5121:'B',5126:'f'}[a['componentType']])*components
    start=v.get('byteOffset',0)+a.get('byteOffset',0)
    return [struct.unpack_from(fmt,binary,start+i*v['byteStride']) for i in range(a['count'])]

def check(work):
    doc,binary=read_glb(work/'background.glb')
    assert [n['name'] for n in doc['nodes']]==['Room 1 primary','Room 1 secondary','Room 2 primary']
    assert [len(m['primitives']) for m in doc['meshes']]==[2,1,1]
    first=doc['meshes'][0]['primitives'][0]
    assert values(doc,binary,first['attributes']['POSITION'])==[(2.5,4.,-6.),(4.,4.,-6.),(2.,6.,-6.)]
    assert values(doc,binary,first['attributes']['TEXCOORD_0'])[0]==(1.,.5)
    assert values(doc,binary,first['attributes']['COLOR_0'])==[(128,64,32,64),(128,64,32,128),(128,64,32,255)]
    assert [m.get('alphaMode','OPAQUE') for m in doc['materials']]==['OPAQUE','OPAQUE','MASK','BLEND']
    assert [m['doubleSided'] for m in doc['materials']]==[False,False,True,False]
    assert len(doc['images'])==2
    assert all(s['wrapS']==33071 and s['wrapT']==33648 for s in doc['samplers'])
    doc,binary=read_glb(work/'stans.glb')
    assert [n['name'] for n in doc['nodes']]==['Room 1 stans','Room 2 stans']
    assert 'images' not in doc
    ps=[m['primitives'][0] for m in doc['meshes']]
    assert [doc['accessors'][p['attributes']['POSITION']]['count'] for p in ps]==[6,3]
    assert values(doc,binary,ps[0]['attributes']['POSITION'])==[(2.5,4.,-6.),(4.,4.,-6.),(4.,6.,-6.),(2.5,4.,-6.),(4.,6.,-6.),(2.5,6.,-6.)]
    assert values(doc,binary,ps[1]['attributes']['COLOR_0'])==[(68,85,102,255)]*3
    assert all(m['doubleSided'] and m.get('alphaMode','OPAQUE')=='OPAQUE' for m in doc['materials'])
    legacy=json.loads((work/'model.gltf').read_text())
    assert len(legacy['nodes'])==len(legacy['meshes'])==1
    assert legacy['buffers'][0]['uri'].startswith('data:')
    assert not list(work.glob('*.tmp'))
    for name in ('native-bg.glb','native-stans.glb'):
        if (work/name).exists(): read_glb(work/name)
    print('PASS: GLB chunks/accessors/embedded PNGs, room/layer meshes, UVs, RGBA, scale, wrapping, culling, alpha modes and legacy JSON export.')

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--bg',type=Path)
    parser.add_argument('--stan',type=Path)
    parser.add_argument('--output',type=Path)
    args=parser.parse_args()
    here=Path(__file__).resolve().parent;src=here.parents[1]/'src';shim=here.parent/'image_import'
    spec=importlib.util.spec_from_file_location('extract',here.parent/'zoom_selected/run.py')
    extract=importlib.util.module_from_spec(spec);spec.loader.exec_module(extract)
    editor=(src/'geditor.c').read_text()
    menu=extract.function(editor,'GEditorCreateMenuBar')
    assert menu.index('(UINT_PTR)importmenu, "&Import..."') < menu.index('(UINT_PTR)exportmenu, "&Export..."')
    for item in ('BACKGROUND','STANS'):
        assert f'EnableMenuItem((HMENU)wparam, ID_FILE_EXPORT_{item}' in editor
        assert f'case ID_FILE_EXPORT_{item}:' in editor
    with tempfile.TemporaryDirectory(prefix='geditor-level-export-') as temp:
        work=args.output.resolve() if args.output else Path(temp)
        work.mkdir(parents=True,exist_ok=True)
        (work/'png.inc').write_text('\n'.join('static const unsigned char '+name+'_png[] = {'+','.join(map(str,png(opaque)))+'};' for name,opaque in [('alpha',False),('opaque',True)]))
        cmd=[os.environ.get('CC','cc'),'-std=c99','-O1','-g','-Wall','-Wextra','-Werror','-Wno-unused-parameter','-ffunction-sections','-fdata-sections','-fsanitize=address,undefined',f'-I{shim}',f'-I{src}',f'-I{work}',str(here/'check.c'),str(shim/'platform.c')]
        cmd += [str(src/name) for name in ('levelexport.c','gltf.c','gltfjson.c','bgdocument.c','bgload.c','bgmaterial.c','bgrender.c','stanload.c')]
        subprocess.run(cmd+['-Wl,--gc-sections','-lm','-o',str(work/'check')],check=True)
        env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1')
        native=[str(args.bg.resolve()),str(args.stan.resolve())] if args.bg and args.stan else []
        subprocess.run([str(work/'check'),str(work),*native],check=True,env=env)
        check(work)
        (work/'ui.inc').write_text(''.join(extract.function(editor,n) for n in ('GEditorCanExportLevel','GEditorExportLevel')))
        subprocess.run([os.environ.get('CC','cc'),'-std=c99','-Wall','-Wextra','-Werror','-Wno-unused-parameter',f'-I{shim}',f'-I{src}',f'-I{work}',str(here/'ui.c'),'-o',str(work/'ui')],check=True)
        subprocess.run([str(work/'ui')],check=True)

if __name__=='__main__': main()
