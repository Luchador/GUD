#!/usr/bin/env python3
"""Drive the production UV canvas through model conversion, compilation and saves."""
import importlib.util
import base64
import json
import math
import os
from pathlib import Path
import re
import subprocess
import struct
import sys
import tempfile

sys.dont_write_bytecode = True
here = Path(__file__).resolve().parent
src = here.parents[1] / 'src'
root = src.parents[2]
def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    result = importlib.util.module_from_spec(spec); spec.loader.exec_module(result)
    return result
extract = module('extract', here.parent / 'portal_editing/run.py')
fixtures = module('fixtures', here.parent / 'model_vertex_uv/run.py')
with tempfile.TemporaryDirectory(prefix='geditor-model-uv-') as folder:
    work = Path(folder)
    canvas = (src / 'uvcanvas.c').read_text()
    (work / 'types.inc').write_text(''.join(re.search(
        r'typedef struct ' + n + r' \{.*?\} ' + n + ';', canvas, re.S)[0] + '\n'
        for n in ('UVCanvasNode', 'UVCanvasState')))
    (work / 'canvas.inc').write_text(''.join(extract.function(canvas, n) for n in (
        'UVCanvasNodeCompare', 'UVCanvasCornerCompare', 'UVCanvasResetTransform',
        'UVCanvasTransformST', 'UVCanvasNodeST', 'UVCanvasSelectionPosition',
        'UVCanvasNotify', 'UVCanvasUpdatePreview', 'UVCanvasCancelInteraction',
        'UVCanvasTryTransform', 'UVCanvasCommit', 'UVCanvasSetTriangles',
        'UVCanvasGetSelection', 'UVCanvasSetCoordinate', 'UVCanvasSetTransformMode', 'UVCanvasGetTransform',
        'UVCanvasApplyTransform', 'UVCanvasHasFaces', 'UVCanvasProjectFaces', 'UVCanvasProjectCylinder')))
    command = [os.environ.get('CC', 'cc'), '-std=c99', '-O1', '-g', '-Wall', '-Wextra',
        '-Werror', '-Wno-unused-parameter', '-Dfopen=TestFopen', '-ffunction-sections', '-fdata-sections',
        '-fsanitize=address,undefined', '-include', str(here / 'win_types.h'),
        f'-I{here.parent / "image_import"}', f'-I{src}', f'-I{root}', f'-I{work}',
        str(here / 'check.c'), str(here.parent / 'image_import/platform.c')]
    command += [str(src / n) for n in ('modeluv.c', 'modelload.c', 'modelmaterials.c', 'modelcompile.c',
        'modeledits.c', 'gltf.c', 'newprops.c', 'propcompile.c', 'bgmaterial.c', 'bgrender.c',
        'rotation.c', 'uvprojection.c', 'uvcylinder.c')]
    subprocess.run(command + ['-Wl,--gc-sections', '-lm', '-o', str(work / 'check')], check=True)
    assets = [root / 'assets/obseg/prop/PsevdoormetslideZ.bin', root / 'assets/obseg/prop/Pbook1Z.bin',
        root / 'assets/obseg/chr/CarmourguardZ.bin']
    for kind in ('mixed', 'normals', 'reflection'):
        path = work / f'{kind}.bin'
        if kind == 'mixed': fixtures.mixed_fixture(path)
        else: fixtures.color_fixture(path, kind)
        assets.append(path)
    env = dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1')
    positions = []
    for i in range(8):
        a, b = i * math.pi / 4, (i + 1) * math.pi / 4
        p = [(math.cos(angle), y, math.sin(angle)) for angle, y in ((a,0),(b,0),(b,1),(a,1))]
        positions += [p[k] for k in (0,1,2,0,2,3)]
    binary = b''.join(struct.pack('<fff', *p) for p in positions)
    cylinder = work / 'cylinder.gltf'
    cylinder.write_text(json.dumps({'asset':{'version':'2.0'}, 'scene':0, 'scenes':[{'nodes':[0]}],
        'nodes':[{'mesh':0}], 'meshes':[{'primitives':[{'attributes':{'POSITION':0}}]}],
        'buffers':[{'byteLength':len(binary),'uri':'data:application/octet-stream;base64,'+base64.b64encode(binary).decode()}],
        'bufferViews':[{'buffer':0,'byteLength':len(binary)}],
        'accessors':[{'bufferView':0,'componentType':5126,'count':len(positions),'type':'VEC3'}]}))
    for i, asset in enumerate(assets):
        project = work / f'project{i}'; (project / 'models/objects').mkdir(parents=True)
        subprocess.run([str(work / 'check'), str(asset), str(project), asset.stem]
            + ([str(cylinder)] if i == 0 else []), env=env, check=True)
    print('All model UV editor checks passed (ASan + UBSan).')
