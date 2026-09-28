#!/usr/bin/env python3
"""Production camera math/persistence under ASan and UBSan; filesystem calls shimmed."""
import copy
import json
import os
from pathlib import Path
import subprocess
import tempfile

here = Path(__file__).resolve().parent
src = here.parent.parent / 'src'
shim = here.parent / 'project_rebase'
with tempfile.TemporaryDirectory(prefix='geditor-studio-camera-') as tmp:
    work = Path(tmp)
    scenes = work / 'studio/scenes'
    scenes.mkdir(parents=True)
    doc = dict(format='GEditor Render Studio', version=8, objects=[], lights=[], environment='',
               ambient=dict(color=[1, 1, 1], intensity=.2),
               directional=dict(color=[1, 1, 1], intensity=1, direction=[0, -1, 0]),
               camera=dict(position=[0, 3, 8], rotation=[-20, 0, 0], size=10))
    invalid = []
    for field, values in [('position', [[0, 0], [0, 0, 1e10], [0, float('nan'), 0]]),
                          ('rotation', [[0, 0], [float('inf'), 0, 0], [0, 1e10, 0]]),
                          ('size', [0, -1, 1e10, float('nan'), float('inf'), 'ten'])]:
        for value in values:
            bad = copy.deepcopy(doc)
            bad['camera'][field] = value
            invalid.append(bad)
        bad = copy.deepcopy(doc)
        del bad['camera'][field]
        invalid.append(bad)
    for value in [None, [], 1, 'camera']:
        invalid.append(dict(doc, camera=value))
    missing = copy.deepcopy(doc)
    del missing['camera']
    invalid.append(missing)
    invalid.append(dict(doc, version=10))
    for field in ('width', 'height'):
        for value in (0, -1, 256, 1.5, float('nan'), float('inf'), '64', None):
            invalid.append(dict(doc, version=9, render=dict(width=64, height=64) | {field: value}))
        bad = dict(doc, version=9, render=dict(width=64, height=64))
        del bad['render'][field]
        invalid.append(bad)
    for value in (None, [], 'render', 1):
        invalid.append(dict(doc, version=9, render=value))
    invalid.append(dict(doc, version=9))
    (scenes / 'Legacy8.rnd').write_text(json.dumps(doc))
    for i, bad in enumerate(invalid):
        (scenes / f'BadCamera{i}.rnd').write_text(json.dumps(bad))
    for version in range(1, 8):
        (scenes / f'Legacy{version}.rnd').write_text(json.dumps(dict(missing, version=version)))
    command = [os.environ.get('CC', 'cc'), '-O1', '-g', '-std=c99', '-Wall', '-Wextra', '-Werror',
               '-Wno-unused-parameter', '-Wno-format', '-ffunction-sections', '-fdata-sections',
               '-fsanitize=address,undefined', '-Dfopen=TestFopen', f'-I{shim}', f'-I{src}',
               f'-I{src.parents[2]}', str(here / 'check.c'), str(shim / 'platform.c')]
    command += [str(src / n) for n in ('studiocamera.c', 'studiodocument.c', 'studiomath.c', 'orbitcamera.c',
                                      'rotation.c', 'gltf.c', 'gltfjson.c', 'modelmaterials.c', 'bgmaterial.c', 'bgrender.c')]
    command += ['-Wl,--gc-sections', '-lm', '-o', str(work / 'check')]
    subprocess.run(command, check=True)
    subprocess.run([str(work / 'check'), str(work), str(len(invalid))], check=True,
                   env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))
    saved = json.loads((scenes / 'Camera.rnd').read_text())
    assert saved['version'] == 9 and saved['camera'] == dict(position=[1.25, -2.5, 8], rotation=[-12, 35, 17], size=3.125)
    assert saved['render'] == dict(width=127, height=63)
    for version in range(1, 9):
        saved = json.loads((scenes / f'Legacy{version}.rnd').read_text())
        assert saved['version'] == 9 and saved['camera'] == doc['camera']
        assert saved['render'] == dict(width=64, height=64)
    print('PASS: independently parsed camera persistence and legacy upgrades.')
