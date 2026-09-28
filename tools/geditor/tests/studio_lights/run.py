#!/usr/bin/env python3
"""Production studio lights, persistence and preview math under ASan/UBSan."""
import copy
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import tempfile


def extract(source, name):
    marker = source.index(name + '(')
    start = source.rfind('\n', 0, marker) + 1
    end = source.index('{', marker) + 1
    depth = 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]



def main():
    here = Path(__file__).resolve().parent
    src = here.parent.parent / 'src'
    shim = here.parent / 'project_rebase'
    spec = importlib.util.spec_from_file_location('fixture', here.parent / 'studio_materials/run.py')
    fixtures = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(fixtures)
    with tempfile.TemporaryDirectory(prefix='geditor-studio-lights-') as tmp:
        work = Path(tmp)
        (work / 'studio/models').mkdir(parents=True)
        scenes = work / 'studio/scenes'
        scenes.mkdir()
        fixtures.fixture(work / 'studio/models')
        point = dict(type='point', position=[0, 5, 0], color=[1, 1, 1], intensity=1, radius=10)
        spot = dict(type='spotlight', position=[0, 5, 0], color=[1, 1, 1], intensity=1,
                    direction=[0, -1, 0], inner=20, outer=30)
        bad = [[spot]*2, [point]*3, [spot, point, point, point]]
        for base, field, value in ((spot, 'direction', [0, 0, 0]), (spot, 'direction', [0, 1]),
                                   (spot, 'outer', 91), (spot, 'inner', 31), (spot, 'inner', -1),
                                   (spot, 'outer', 0), (point, 'radius', 0), (point, 'radius', -10),
                                   (point, 'radius', 1e99), (point, 'intensity', -1),
                                   (point, 'intensity', 10001), (point, 'color', [1, 1, 1.1]),
                                   (point, 'position', [1e99, 0, 0]), (point, 'intensity', float('nan')),
                                   (point, 'position', [0, float('inf'), 0]), (point, 'type', 'sun'),
                                   (point, 'radius', 'ten'), (point, 'position', [1, 2])):
            item = copy.deepcopy(base)
            item[field] = value
            bad.append([item])
        for base in (spot, point):
            for field in base:
                item = copy.deepcopy(base)
                del item[field]
                bad.append([item])
        for index, lights in enumerate(bad):
            (scenes / f'Bad{index}.rnd').write_text(json.dumps(
                dict(format='GEditor Render Studio', version=3, objects=[], lights=lights)))
        # Missing or malformed lights arrays are also rejected transactionally.
        bad_docs = [dict(format='GEditor Render Studio', version=3, objects=[]),
                    dict(format='GEditor Render Studio', version=3, objects=[], lights={})]
        for doc in bad_docs:
            (scenes / f'Bad{len(bad)}.rnd').write_text(json.dumps(doc))
            bad.append(None)
        for version in (1, 2):
            obj = dict(model='Light.gltf', position=[1, 2, 3], materials=[])
            if version == 2:
                obj.update(rotation=[0, 23, 0], scale=[1, 1, 2.5])
            (scenes / f'Legacy{version}.rnd').write_text(json.dumps(
                dict(format='GEditor Render Studio', version=version, objects=[obj])))
        command = [os.environ.get('CC', 'cc'), '-O1', '-g', '-std=c99', '-Wall', '-Wextra', '-Werror',
                   '-Wno-unused-parameter', '-Wno-format', '-ffunction-sections', '-fdata-sections',
                   '-fsanitize=address,undefined', '-Dfopen=TestFopen', f'-I{shim}', f'-I{src}',
                   f'-I{src.parents[2]}', str(here / 'check.c'), str(shim / 'platform.c')]
        command += [str(src / n) for n in ('studiodocument.c', 'studiomath.c', 'orbitcamera.c', 'rotation.c',
                                          'gltf.c', 'gltfjson.c', 'modelmaterials.c', 'bgmaterial.c', 'bgrender.c')]
        command += ['-Wl,--gc-sections', '-lm', '-o', str(work / 'check')]
        subprocess.run(command, check=True)
        subprocess.run([str(work / 'check'), str(work), str(len(bad))], check=True,
                       env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))
        saved = json.loads((scenes / 'Lights.rnd').read_text())
        assert saved['version'] == 3 and len(saved['objects']) == 1
        assert [light['type'] for light in saved['lights']] == ['spotlight', 'point', 'point']
        assert saved['lights'][0]['inner'] == 12.75 and saved['lights'][0]['direction'] == [1, -1, 0]
        assert saved['lights'][2]['radius'] == 1/3
        for version in (1, 2):
            migrated = json.loads((scenes / f'Legacy{version}.rnd').read_text())
            assert migrated['version'] == 3 and migrated['lights'] == []
        assert not list(scenes.glob('rnd*.tmp'))
        print(f'PASS: independent JSON verification, {len(bad)} invalid light documents, legacy migration, no leaked temporary files.')
        source = (src / 'renderstudio.c').read_text()
        document = (src / 'studiodocument.c').read_text()
        (work / 'ui.inc').write_text('\n'.join(extract(document, name) for name in (
            'StudioLightValid', 'StudioSceneLightSlot', 'StudioSceneAddLight')) + '\n' + '\n'.join(
                extract(source, name) for name in ('RenderStudioMaterial', 'RenderStudioSelection',
                'RenderStudioLight', 'RenderStudioLightField', 'RenderStudioProperties', 'RenderStudioSelect',
                'RenderStudioOutliner', 'RenderStudioCommitLight', 'RenderStudioLightCommand', 'RenderStudioAddLight')))
        subprocess.run([os.environ.get('CC', 'cc'), '-std=c99', '-Wall', '-Wextra', '-Werror',
                        '-Wno-unused-parameter', '-fsanitize=address,undefined', f'-I{shim}', f'-I{src}', f'-I{work}',
                        str(here / 'ui.c'), '-lm', '-o', str(work / 'ui')], check=True)
        subprocess.run([str(work / 'ui')], check=True,
                       env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))



if __name__ == '__main__':
    main()
