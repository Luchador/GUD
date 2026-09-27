#!/usr/bin/env python3
"""Production studio import, persistence, picking and lighting under ASan/UBSan.

Only Windows filesystem calls and unused game image lookup use test shims.
Native Win32 interaction and WIC decoding require the Windows smoke test.
"""
import base64
import copy
import json
import os
from pathlib import Path
import struct
import subprocess
import tempfile


def fixture(folder):
    data = bytearray()
    doc = dict(asset={'version': '2.0'}, scene=0, scenes=[{'nodes': [0]}],
               nodes=[{'mesh': 0}], meshes=[{'primitives': []}], accessors=[], bufferViews=[],
               materials=[{'name': name, 'pbrMetallicRoughness': {'baseColorFactor': color}}
                          for name, color in zip(('Housing "blue"', 'Réflecteur', 'Glass'),
                                                 ([.25, .5, 1, 1], [1, .5, .25, 1], [.5, 1, .25, 1]))])

    def accessor(values, kind, fmt, ctype):
        data.extend(b'\0' * (-len(data) % 4))
        start = len(data)
        for row in values:
            data.extend(struct.pack('<' + fmt, *row))
        doc['bufferViews'].append({'buffer': 0, 'byteOffset': start, 'byteLength': len(data)-start})
        doc['accessors'].append(dict(bufferView=len(doc['bufferViews'])-1, componentType=ctype,
                                     count=len(values), type=kind))
        return len(doc['accessors'])-1

    for i in range(3):
        attrs = {'POSITION': accessor([(2*i, 0, 0), (2*i+1, 0, 0), (2*i, 1, 0)], 'VEC3', 'fff', 5126),
                 'TEXCOORD_0': accessor([(.125, .25), (1.25, 0), (0, 1)], 'VEC2', 'ff', 5126)}
        if i < 2:
            attrs['NORMAL'] = accessor([(0, 0, 1) if i == 0 else (1, 1, 1)]*3, 'VEC3', 'fff', 5126)
        doc['meshes'][0]['primitives'].append(dict(attributes=attrs, material=i,
            indices=accessor([(0,), (1,), (2,)], 'SCALAR', 'H', 5123)))
    doc['buffers'] = [{'byteLength': len(data),
                       'uri': 'data:application/octet-stream;base64,'+base64.b64encode(data).decode()}]
    for name in ('Light.gltf', 'original.gltf'):
        (folder / name).write_text(json.dumps(doc))
    transformed = copy.deepcopy(doc)
    transformed['nodes'][0].update(scale=[-2, 3, 4], translation=[10, 20, 30])
    (folder / 'transformed.gltf').write_text(json.dumps(transformed))
    reordered = copy.deepcopy(doc)
    reordered['materials'] = [doc['materials'][2], doc['materials'][0], doc['materials'][1]]
    for p in reordered['meshes'][0]['primitives']:
        p['material'] = (p['material']+1) % 3
    (folder / 'reordered.gltf').write_text(json.dumps(reordered))
    external = copy.deepcopy(doc)
    external['buffers'][0]['uri'] = 'mesh.bin'
    (folder / 'external.gltf').write_text(json.dumps(external))
    (folder / 'mesh.bin').write_bytes(data)
    invalid = copy.deepcopy(doc)
    invalid['animations'] = [{}]
    (folder / 'animated.gltf').write_text(json.dumps(invalid))


def main():
    here = Path(__file__).resolve().parent
    src = here.parent.parent / 'src'
    shim = here.parent / 'project_rebase'
    with tempfile.TemporaryDirectory(prefix='geditor-studio-materials-') as tmp:
        work = Path(tmp)
        models = work / 'studio/models'
        models.mkdir(parents=True)
        (work / 'studio/scenes').mkdir()
        fixture(models)
        command = [os.environ.get('CC', 'cc'), '-O1', '-g', '-std=c99', '-Wall', '-Wextra',
                   '-Wno-unused-parameter', '-Wno-format', '-ffunction-sections', '-fdata-sections',
                   '-fsanitize=address,undefined', '-Dfopen=TestFopen', f'-I{shim}', f'-I{src}',
                   f'-I{src.parents[2]}', str(here / 'check.c'), str(shim / 'platform.c')]
        command += [str(src / n) for n in ('studiodocument.c', 'studiomath.c', 'orbitcamera.c',
                                          'gltf.c', 'gltfjson.c', 'modelmaterials.c', 'bgmaterial.c', 'bgrender.c')]
        command += ['-Wl,--gc-sections', '-lm', '-o', str(work / 'check')]
        subprocess.run(command, check=True)
        subprocess.run([str(work / 'check'), str(work)], check=True,
                       env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))
        scene = json.loads((work / 'studio/scenes/Main.rnd').read_text())
        assert scene['version'] == 1 and len(scene['objects']) == 2
        first, second = scene['objects']
        assert first['materials'][0]['name'] == 'Housing "blue"'
        assert first['materials'][1]['name'] == 'Réflecteur'
        assert first['materials'][1]['image'] == 'Paint.bmp'
        assert first['materials'][1]['shininess'] == 87
        assert first['materials'][1]['base'] != second['materials'][1]['base']
        assert not list((work / 'studio/scenes').glob('rnd*.tmp'))
        print('PASS: JSON independently parsed; per-instance settings and UTF-8 names persisted; no temporary files leaked.')


if __name__ == '__main__':
    main()
