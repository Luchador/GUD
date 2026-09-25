#!/usr/bin/env python3
"""Exercise production gizmo geometry, picking and scale drag/commit on a host."""
import importlib.util
import json
import os
from pathlib import Path
import re
import struct
import subprocess
import sys
import tempfile

here = Path(__file__).resolve().parent
src = here.parents[1] / 'src'
spec = importlib.util.spec_from_file_location('extract', here.parent / 'portal_editing/run.py')
extract = importlib.util.module_from_spec(spec)
sys.dont_write_bytecode = True
spec.loader.exec_module(extract)
viewport = (src / 'viewport.c').read_text()

# Flatten the shipped scale handle, including its node transform and the same
# maximum-X normalization used by ViewportLoadHandle. This asset has one node.
data = (src.parent / 'geditorassets/gizmo_scale.glb').read_bytes()
length = struct.unpack_from('<I', data, 12)[0]
gltf = json.loads(data[20:20 + length])
binary = data[28 + length:]
node, = gltf['nodes']
assert 'matrix' not in node and 'rotation' not in node and 'scale' not in node
translation = node.get('translation', [0, 0, 0])

def accessor(index):
    a = gltf['accessors'][index]
    view = gltf['bufferViews'][a['bufferView']]
    fmt = '<' + {5126: 'f', 5123: 'H'}[a['componentType']] * {'VEC3': 3, 'SCALAR': 1}[a['type']]
    stride = view.get('byteStride', struct.calcsize(fmt))
    offset = view.get('byteOffset', 0) + a.get('byteOffset', 0)
    return [struct.unpack_from(fmt, binary, offset + i * stride) for i in range(a['count'])]

vertices = []
for primitive in gltf['meshes'][node['mesh']]['primitives']:
    positions = accessor(primitive['attributes']['POSITION'])
    vertices.extend(tuple(positions[i][k] + translation[k] for k in range(3))
                    for i, in accessor(primitive['indices']))
size = max(v[0] for v in vertices)
with tempfile.TemporaryDirectory(prefix='geditor-uniform-scale-') as temp:
    work = Path(temp)
    types = ('Vertex', 'ViewportComponent', 'ViewportState', 'ViewportPickRay')
    (work / 'types.inc').write_text(''.join(re.search(
        r'typedef struct ' + name + r' \{.*?\} ' + name + ';', viewport, re.S)[0] + '\n' for name in types))
    defines = ('VIEWPORT_UNIFORM_SCALE_AXIS', 'VIEWPORT_PICK_EPSILON', 'VIEWPORT_PICK_BARY_EPSILON')
    names = ('ViewportObjectCount', 'ViewportCompareVertexRefs', 'ViewportGetMoveVertices', 'ViewportRayTriangleDistance',
             'ViewportArrowVertex', 'ViewportDrawGizmoHandles', 'ViewportPickGizmo',
             'ViewportShouldExtrudeEdges', 'ViewportPreviewEdgeExtrusion',
             'ViewportBeginTransform', 'ViewportDragTransform', 'ViewportCancelTransform',
             'ViewportFinishPortalDuplicate', 'ViewportEndTransform', 'ViewportGetScaling', 'ViewportPreviewGuidePoint',
             'ViewportEnvironmentCoordinates')
    (work / 'functions.inc').write_text(''.join(re.search(r'^#define ' + name + r' .*$', viewport, re.M)[0] + '\n'
        for name in defines) + extract.function((src / 'bgrender.c').read_text(), 'BgRenderTriangleNormal')
        + ''.join(extract.function(viewport, name) for name in names))
    (work / 'asset.inc').write_text('static BgVertex handle[] = {\n' + ''.join(
        '{.x=%.12g,.y=%.12g,.z=%.12g},\n' % tuple(n / size for n in v) for v in vertices) + '};\n')
    executable = work / 'check'
    subprocess.run([os.environ.get('CC', 'cc'), '-std=c99', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
                    '-Wno-unused-parameter', '-fsanitize=address,undefined',
                    f'-I{here.parent / "image_import"}', f'-I{src}', f'-I{work}',
                    str(here / 'check.c'), str(src / 'rotation.c'), str(src / 'scaling.c'),
                    '-lm', '-o', str(executable)], check=True)
    subprocess.run([str(executable)], check=True,
                   env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))
