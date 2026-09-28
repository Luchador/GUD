#!/usr/bin/env python3
"""Production studio lights, persistence and preview math under ASan/UBSan."""
import copy
import importlib.util
import json
import os
import re
from pathlib import Path
import subprocess
import tempfile


def extract(source, name):
    definition = re.search(r'^\w[^;\n]*\b' + re.escape(name) + r'\([^;]*?\)\s*\n\{', source, re.M)
    assert definition, name
    start = definition.start()
    end = definition.end()
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
        bad += [[dict(point, slot=0)], [dict(spot, slot=1)], [dict(point, slot=3)],
                [dict(point, slot=-1)], [dict(point, slot=1.5)], [dict(point, slot=1)]*2]
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
        assert saved['version'] == 6 and len(saved['objects']) == 1
        assert [light['type'] for light in saved['lights']] == ['spotlight', 'point', 'point']
        assert saved['lights'][0]['inner'] == 12.75 and saved['lights'][0]['direction'] == [1, -1, 0]
        assert saved['lights'][2]['radius'] == 1/3
        for version in (1, 2):
            migrated = json.loads((scenes / f'Legacy{version}.rnd').read_text())
            assert migrated['version'] == 6 and migrated['lights'] == []
        assert [light['slot'] for light in json.loads((scenes / 'Sparse.rnd').read_text())['lights']] == [0, 2]
        assert [light['slot'] for light in json.loads((scenes / 'Reused.rnd').read_text())['lights']] == [0, 1, 2]
        assert json.loads((scenes / 'Empty.rnd').read_text())['lights'] == []
        assert not list(scenes.glob('rnd*.tmp'))
        print(f'PASS: independent JSON verification, {len(bad)} invalid light documents, legacy migration, no leaked temporary files.')
        globals_doc = dict(format='GEditor Render Studio', version=4, objects=[], lights=[],
                           ambient=dict(color=[1, 1, 1], intensity=.2),
                           directional=dict(color=[1, 1, 1], intensity=1, direction=[0, -1, 0]))
        invalid = []
        for key in ('ambient', 'directional'):
            for field, value in (('color', [1, -1, 0]), ('color', [1, 1, 1.00000001]),
                                 ('color', [1, 1]), ('color', [1, float('nan'), 1]),
                                 ('intensity', -1), ('intensity', 10001), ('intensity', 'one'),
                                 ('intensity', float('inf'))):
                doc = copy.deepcopy(globals_doc)
                doc[key][field] = value
                invalid.append(doc)
            for field in globals_doc[key]:
                doc = copy.deepcopy(globals_doc)
                del doc[key][field]
                invalid.append(doc)
            for value in (None, [], 1):
                doc = copy.deepcopy(globals_doc)
                doc[key] = value
                invalid.append(doc)
            doc = copy.deepcopy(globals_doc)
            del doc[key]
            invalid.append(doc)
        for direction in ([0, 0, 0], [0, 1], [1e10, 0, 0], [0, float('nan'), 0]):
            doc = copy.deepcopy(globals_doc)
            doc['directional']['direction'] = direction
            invalid.append(doc)
        invalid.append(dict(globals_doc, version=7))
        for index, doc in enumerate(invalid):
            (scenes / f'BadGlobal{index}.rnd').write_text(json.dumps(doc))
        for index, version in enumerate((1, 2, 3, 3, 3)):
            doc = dict(format='GEditor Render Studio', version=version, objects=[])
            if version == 3:
                doc['lights'] = [] if index == 2 else [dict(point, intensity=0 if index == 4 else 1)]
            (scenes / f'GlobalLegacy{index}.rnd').write_text(json.dumps(doc))
        global_command = [arg.replace(str(here / 'check.c'), str(here / 'globals.c'))
                          .replace(str(work / 'check'), str(work / 'globals')) for arg in command]
        subprocess.run(global_command, check=True)
        subprocess.run([str(work / 'globals'), str(work), str(len(invalid))], check=True,
                       env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))
        saved_globals = json.loads((scenes / 'Globals.rnd').read_text())
        assert saved_globals['ambient'] == dict(color=[.125, .25, 1], intensity=0)
        assert saved_globals['directional'] == dict(color=[1, 1, .75], intensity=2.5, direction=[7, -3, .125])
        for index in range(5):
            migrated = json.loads((scenes / f'GlobalLegacy{index}.rnd').read_text())
            assert migrated['version'] == 6 and migrated['directional']['intensity'] == (1 if index < 3 else 0)
        assert not list(scenes.glob('rnd*.tmp'))
        print(f'PASS: independent global-light JSON verification and {len(invalid)} malformed documents.')
        icons = (src / 'studiolightview.c').read_text()
        (work / 'icons.inc').write_text('\n'.join(extract(icons, name) for name in ('StudioLightIconRect', 'StudioLightIconPick')))
        subprocess.run([os.environ.get('CC', 'cc'), '-O1', '-g', '-std=c99', '-Wall', '-Wextra', '-Werror',
                        '-Wno-unused-parameter', '-Wno-format', '-ffunction-sections', '-fdata-sections',
                        '-fsanitize=address,undefined', f'-I{shim}', f'-I{src}', f'-I{work}',
                        str(here / 'interaction.c'), str(src / 'studiodrag.c'), str(src / 'studiomath.c'),
                        str(src / 'studiodocument.c'), str(src / 'orbitcamera.c'), str(src / 'rotation.c'),
                        '-Wl,--gc-sections', '-lm', '-o', str(work / 'interaction')], check=True)
        subprocess.run([str(work / 'interaction')], check=True,
                       env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))
        source = (src / 'renderstudio.c').read_text()
        document = (src / 'studiodocument.c').read_text()
        (work / 'ui.inc').write_text('\n'.join(line for line in document.splitlines() if line.startswith('const StudioGlobalLight ')) + '\n' + '\n'.join(extract(document, name) for name in (
            'StudioAssetFilename', 'StudioMaterialValid', 'StudioSceneDefaultLighting', 'StudioGlobalLightValid', 'StudioTransformValid', 'StudioLightValid', 'StudioSceneLightSlot', 'StudioSceneAddLight')) + '\n' +
            extract((src / 'studiodrag.c').read_text(), 'StudioLightToolAllowed') + '\n' + '\n'.join(
                extract(source, name) for name in ('RenderStudioMaterial', 'RenderStudioSelection',
                'RenderStudioLight', 'RenderStudioGlobalLight', 'RenderStudioLightField', 'RenderStudioSceneSelected',
                'RenderStudioEnvironmentAvailable', 'RenderStudioProperties', 'RenderStudioEnvironmentCommand', 'RenderStudioImageChoices',
                'RenderStudioCommitMaterial', 'RenderStudioFinishMetalness', 'RenderStudioMetalnessCommand', 'RenderStudioMaterialCommand', 'RenderStudioObject',
                'RenderStudioTransformField', 'RenderStudioTransformPanel', 'RenderStudioTool',
                'RenderStudioCommitTransform', 'RenderStudioTransformProc', 'RenderStudioSelect',
                'RenderStudioOutliner', 'RenderStudioCommitGlobalLight', 'RenderStudioGlobalLightCommand', 'RenderStudioCommitLight', 'RenderStudioLightCommand', 'RenderStudioAddLight',
                'RenderStudioDeleteLight')))
        subprocess.run([os.environ.get('CC', 'cc'), '-std=c99', '-Wall', '-Wextra', '-Werror',
                        '-Wno-unused-parameter', '-fsanitize=address,undefined', f'-I{shim}', f'-I{src}', f'-I{work}',
                        str(here / 'ui.c'), '-lm', '-o', str(work / 'ui')], check=True)
        subprocess.run([str(work / 'ui')], check=True,
                       env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))



if __name__ == '__main__':
    main()
