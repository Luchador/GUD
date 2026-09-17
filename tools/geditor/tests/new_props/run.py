#!/usr/bin/env python3
"""Static four-material prop import/save/ROM-bank regression, without a ROM.

Real glTF/compiler/native model/edit-store code; mocked ROM directory, image
lookup and PNG encoder. Existing project_rebase tests exercise the real ROM
reader/exporter separately. The Windows UI and N64 GPU are not simulated.
"""
import argparse
import base64
import copy
import json
import os
from pathlib import Path
import struct
import subprocess
import tempfile


def fixture(path, extra=False):
    data = bytearray()
    doc = dict(asset={'version': '2.0'}, scene=0, scenes=[{'nodes': [0]}],
               nodes=[{'mesh': 0}], meshes=[{'primitives': []}], accessors=[], bufferViews=[],
               images=[{'name': 'GUD Image 00D4'}, {'name': 'GUD Image 0B00'}],
               textures=[{'source': 0}, {'source': 1}],
               materials=[{'pbrMetallicRoughness': {'baseColorTexture': {'index': 0}}},
                          {'pbrMetallicRoughness': {'baseColorTexture': {'index': 1}}},
                          {'pbrMetallicRoughness': {'baseColorFactor': [0.5, 1, 1, 1]}},
                          {'alphaMode': 'BLEND', 'doubleSided': True}])

    def accessor(values, kind, fmt, ctype):
        data.extend(b'\0' * (-len(data) % 4))
        start = len(data)
        for row in values:
            data.extend(struct.pack('<' + fmt, *row))
        doc['bufferViews'].append({'buffer': 0, 'byteOffset': start, 'byteLength': len(data)-start})
        result = len(doc['accessors'])
        record = dict(bufferView=len(doc['bufferViews'])-1, componentType=ctype, count=len(values), type=kind)
        if ctype == 5121:
            record['normalized'] = True
        doc['accessors'].append(record)
        return result

    for i in range(5 if extra else 4):
        z = i * .1
        height = -2 if i == 3 else .2
        attrs = {'POSITION': accessor([(0, 0, z), (.2, 0, z), (0, height, z)], 'VEC3', 'fff', 5126),
                 'COLOR_0': accessor([(200, 160, 120, 25 if i == 3 else 255)] * 3, 'VEC4', '4B', 5121)}
        if i < 2:
            attrs['TEXCOORD_0'] = accessor([(0, 0), (1, 0), (0, 1)], 'VEC2', 'ff', 5126)
        doc['meshes'][0]['primitives'].append({'attributes': attrs, 'material': min(i, 3) if i < 4 else 2})
    for material, name in zip(doc['materials'], ('Cable', 'Metal', 'Solid', 'Shaft')):
        material['name'] = name
    write(path, doc, data)
    return doc, data


def write(path, doc, data):
    doc = copy.deepcopy(doc)
    doc['buffers'] = [{'byteLength': len(data)}]
    if path.suffix == '.gltf':
        doc['buffers'][0]['uri'] = 'data:application/octet-stream;base64,' + base64.b64encode(data).decode()
        path.write_text(json.dumps(doc))
    else:
        text = json.dumps(doc).encode()
        text += b' ' * (-len(text) % 4)
        binary = data + b'\0' * (-len(data) % 4)
        path.write_bytes(struct.pack('<III', 0x46546c67, 2, 28+len(text)+len(binary))
                         + struct.pack('<II', len(text), 0x4e4f534a) + text
                         + struct.pack('<II', len(binary), 0x004e4942) + binary)


def aa_check(root, work):
    # Use the game's actual AA-off command rewrite and native GBI constants.
    source = (root / 'src/game/renderconfig.c').read_text()
    def function(name):
        start = source.index('static void ' + name + '(')
        brace = source.index('{', start)
        end, depth = brace + 1, 1
        while depth:
            depth += (source[end] == '{') - (source[end] == '}')
            end += 1
        return source[start:end]
    text = '#define _LANGUAGE_C\ntypedef __PTRDIFF_TYPE__ ptrdiff_t;\n#include <PR/gbi.h>\n#define assert(x) do { if (!(x)) return __LINE__; } while (0)\n'
    text += '\n'.join(line for line in source.splitlines() if line.startswith('#define AA_')) + '\n'
    start = source.index('static const u32 g_AaOpaqueModes')
    text += source[start:source.index('};', start)+2] + '\n'
    text += function('renderDisableAaCommand') + '\n' + function('renderRestoreAaCommand')
    text += r"""
int main(void) {
    Gfx command;
    const u32 mode=G_RM_PASS | G_RM_AA_ZB_XLU_SURF2;
    assert(mode==0x0c1849d8u);
    assert((mode & FORCE_BL) && (mode & Z_CMP));
    assert(!(mode & (CVG_X_ALPHA | ALPHA_CVG_SEL | Z_UPD)));
    command.words.w0=0xb900031d;command.words.w1=mode;
    renderDisableAaCommand(&command);assert(command.words.w1==mode);
    renderRestoreAaCommand(&command);assert(command.words.w1==mode);
    return 0;
}
"""
    (work / 'aa.c').write_text(text)
    subprocess.run([os.environ.get('CC', 'cc'), '-std=c99', f'-I{root / "include"}',
                    str(work / 'aa.c'), '-o', str(work / 'aa')], check=True)
    subprocess.run([str(work / 'aa')], check=True)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--pendant", type=Path, help="Also validate the supplied 80-triangle PpendantZ GLB")
    args = parser.parse_args()
    here = Path(__file__).resolve().parent
    src = here.parent.parent / 'src'
    root = src.parents[2]
    shim = here.parent / 'image_import'
    with tempfile.TemporaryDirectory(prefix='geditor-new-props-') as tmp:
        work = Path(tmp)
        (work / 'models/objects').mkdir(parents=True)
        aa_check(root, work)
        command = [os.environ.get('CC', 'cc'), '-O1', '-g', '-std=c99', '-Wall', '-Wextra', '-Werror',
                   '-Wno-unused-parameter', '-ffunction-sections', '-fdata-sections', '-fsanitize=address,undefined',
                   '-Dfopen=TestFopen', f'-I{shim}', f'-I{src}', f'-I{root}', str(here / 'check.c'), str(shim / 'platform.c')]
        command += [str(src / n) for n in ('newprops.c', 'propcompile.c', 'modelload.c', 'modelmaterials.c', 'modelcompile.c',
                                          'modeledits.c', 'gltf.c', 'bgmaterial.c', 'bgrender.c', 'objectshade.c')]
        command += ['-Wl,--gc-sections', '-lm', '-o', str(work / 'check')]
        subprocess.run(command, check=True)
        doc, data = fixture(work / 'pendant.glb')
        fixture(work / 'changed.gltf', True)
        invalid = copy.deepcopy(doc)
        invalid['images'][1]['name'] = 'GUD Image 0BAD'
        write(work / 'missing.glb', invalid, data)
        for label, modify in (
            ('animation', lambda d: d.update(animations=[{}])),
            ('skin', lambda d: d['nodes'][0].update(skin=0)),
            ('mask', lambda d: d['materials'][0].update(alphaMode='MASK')),
            ('collapse', lambda d: d['nodes'][0].update(scale=[.000001]*3)),
            ('range', lambda d: d['nodes'][0].update(translation=[40, 0, 0])),
            ('line', lambda d: d['meshes'][0]['primitives'][0].update(mode=1)),
        ):
            invalid = copy.deepcopy(doc)
            modify(invalid)
            write(work / f'{label}.glb', invalid, data)
        mirrored = copy.deepcopy(doc)
        mirrored['nodes'][0]['scale'] = [-1, 1, 1]
        write(work / 'mirror.glb', mirrored, data)
        env = dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1')
        subprocess.run([str(work / 'check'), str(work)], env=env, check=True)
        if args.pendant:
            actual = work / 'actual'
            (actual / 'models/objects').mkdir(parents=True)
            subprocess.run([str(work / 'check'), str(actual), str(args.pendant.resolve())], env=env, check=True)
        print('New-prop import, native materials, stable IDs, topology, save/reload and ROM-bank tests passed (ASan + UBSan).')


if __name__ == '__main__':
    main()
