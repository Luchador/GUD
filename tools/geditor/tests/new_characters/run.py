#!/usr/bin/env python3
"""Real native head/body round trips and character bank regression (ASan/UBSan).
ROM directory and texture image decoding are mocked; native model data,
compiler, GLB, edit store, bank export, character lookup and posing are real.
"""
import argparse
import copy
import json
import os
import runpy
import struct
from pathlib import Path
import subprocess
import tempfile
here=Path(__file__).resolve().parent
src=here.parents[1]/'src'
root=src.parents[2]
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--heads',type=Path,help='Optional extracted GoldenEye-XBLA-Bond-Geometry folder (tests heads and bodies)')
parser.add_argument('--legacy-bodies',type=Path,help='Optional pre-fix native body dumps for repair/undo/save regression')
parser.add_argument('--offset-fixtures',type=Path,help='Optional native actor head .bin files to test offset editing of textured models')
args=parser.parse_args()
if args.legacy_bodies and not args.heads:
    parser.error('--legacy-bodies requires --heads')
if args.offset_fixtures and not args.heads:
    parser.error('--offset-fixtures requires --heads')

def raw_head(path,variant='normal'):
    positions=[(100,5000,300),(300,5000,300),(200,5400,300),(200,5200,600)]
    if variant=='flat': positions=[(x,5000,z) for x,y,z in positions]
    if variant=='collapsed': positions=[positions[0]]*4
    if variant=='oversize': positions=[(x+100000,y,z) for x,y,z in positions]
    if variant=='changed': positions[2]=(260,5460,325)
    payload=bytearray();views=[];accessors=[]
    def accessor(values,components):
        start=len(payload)
        for value in values: payload.extend(struct.pack('<'+'f'*components,*value))
        views.append(dict(buffer=0,byteOffset=start,byteLength=len(payload)-start))
        accessors.append(dict(bufferView=len(views)-1,componentType=5126,count=len(values),type={2:'VEC2',3:'VEC3',4:'VEC4'}[components]))
        return len(accessors)-1
    faces=[0,1,2,0,3,1,0,2,3,1,3,2]
    attributes={'POSITION':accessor([positions[i] for i in faces],3),
                'TEXCOORD_0':accessor([(i*.1,i*.05) for i in faces],2),
                'COLOR_0':accessor([(.2+i*.1,.5,.8,1) for i in faces],4)}
    # Separate named primitives retain material membership without GUD extras.
    primitive=[]
    for part in range(2):
        attrs={}
        for name,index in attributes.items():
            a=copy.deepcopy(accessors[index]);a['byteOffset']=part*6*{'POSITION':12,'TEXCOORD_0':8,'COLOR_0':16}[name];a['count']=6
            attrs[name]=len(accessors);accessors.append(a)
        primitive.append(dict(attributes=attrs,material=part,mode=4))
    doc=dict(asset={'version':'2.0'},scene=0,scenes=[{'nodes':[0]}],
             nodes=[{'children':[1],'translation':[100,-500,20]},{'mesh':0,'scale':[2,2,2]}],
             meshes=[{'primitives':primitive}],materials=[{'name':'Actor skin'},{'name':'Actor hair'}],
             buffers=[{'byteLength':len(payload)}],bufferViews=views,accessors=accessors)
    if variant=='changed': doc['materials'][1]['name']='New hair'
    if variant=='stale': doc['scenes'][0]['extras']={'goldeneyeSourceHash':'00000000'}
    if variant=='malformed': doc['scenes'][0]['extras']={'goldeneyeSourceHash':'notahash'}
    if variant=='mixed': doc['nodes'][1]['extras']={'goldeneyeSourceHash':'00000000'}
    if variant=='skinned': doc['nodes'][1]['skin']=0
    if variant=='animated': doc['animations']=[{}]
    if variant=='blend': doc['materials'][0]['alphaMode']='BLEND'
    j=json.dumps(doc).encode();j+=b' '*(-len(j)%4)
    path.write_bytes(struct.pack('<III',0x46546c67,2,28+len(j)+len(payload))+struct.pack('<II',len(j),0x4e4f534a)+j+struct.pack('<II',len(payload),0x004e4942)+payload)

with tempfile.TemporaryDirectory(prefix='geditor-characters-') as tmp:
    work=Path(tmp)
    (work/'models/characters').mkdir(parents=True)
    runpy.run_path(str(here.parent/'new_props/run.py'))['fixture'](work/'prop.glb')
    for variant in ('normal','flat','collapsed','oversize','stale','skinned','animated','blend','malformed','mixed','changed'):
        raw_head(work/f'raw-{variant}.glb',variant)
    # Real native frames exercise root/limb rotations and weapon/head attachments.
    convert=runpy.run_path(str(root/'tools/make_animation_entries_uncompressed.py'))
    arrays=dict(convert['parse_entry_arrays']((root/'assets/animationtable_entries.c').read_text()))
    headers={m['name']:m for m in convert['DATA_HEADER_RE'].finditer((root/'assets/animationtable_data.c').read_text())}
    poses=[]
    for name in ('walking','running','sprinting','death_forward_face_down','death_backward_fall_face_up1'):
        info=int(headers[name]['info'],16);bits=int(headers[name]['layout'],16)&65535
        width=(info>>8)&255;count=info>>16
        assert bits%8==0 and bits//width==45
        for frame in (0,count//2,count-1):
            data=arrays[name][frame*(bits//8):(frame+1)*(bits//8)]
            poses.append([convert['read_packed_value'](data,c*width,width)<<(16-width) for c in range(45)])
    (work/'bodyposes.inc').write_text('static const unsigned short bodyposes[][45]={\n'+
        ',\n'.join('{'+','.join(map(str,p))+'}' for p in poses)+'\n};\n')
    command=[os.environ.get('CC','cc'),'-std=c99','-O1','-g','-Wall','-Wextra','-Werror',
        '-Wno-unused-parameter','-ffunction-sections','-fdata-sections','-fsanitize=address,undefined',
        '-Dfopen=TestFopen',f'-I{here.parent/"image_import"}',f'-I{src}',f'-I{root}',f'-I{work}',
        str(here/'check.c'),str(here.parent/'image_import/platform.c')]
    command += [str(src/name) for name in ('newprops.c','propcompile.c','modelcompile.c','modelload.c',
        'modeledits.c','modelmaterials.c','gltf.c','gltfjson.c','bgmaterial.c','bgrender.c',
        'characterload.c','objectload.c','objectshade.c')]
    subprocess.run(command+['-Wl,--gc-sections','-lm','-o',str(work/'check')],check=True)
    heads=[str(args.heads/name/'head.glb') for name in ('Roger_Moore','Sean_Connery','Timothy_Dalton')] if args.heads else []
    subprocess.run([str(work/'check'),str(work),str(root)]+heads,check=True,
        env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1',
                 GEDITOR_LEGACY_BODIES=str(args.legacy_bodies.resolve()) if args.legacy_bodies else '',
                 GEDITOR_OFFSET_FIXTURES=str(args.offset_fixtures.resolve()) if args.offset_fixtures else ''))

    runtime=(root/'src/game/pobjdata.c').read_text()
    runtime=runtime[runtime.index('CustomPropRomConfig g_CustomPropRomConfig'):runtime.index('ExplosionDetailsRecord *propExplosionGet')]
    runtime=runtime.replace('(u32)scratch','(uintptr_t)scratch')
    (work/'runtime.inc').write_text(runtime)
    subprocess.run([os.environ.get('CC','cc'),'-std=c99','-O1','-g','-Wall','-Wextra',
        '-Wno-unused-parameter','-Wno-int-to-pointer-cast','-fsanitize=address,undefined',
        f'-I{work}',f'-I{root}',str(here/'runtime.c'),'-o',str(work/'runtime')],check=True)
    subprocess.run([str(work/'runtime')],check=True,
        env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1'))
