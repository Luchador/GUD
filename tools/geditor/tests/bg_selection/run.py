#!/usr/bin/env python3
"""Exercise production BG selection commands and their keyboard routing."""
import os
from pathlib import Path
import re
import subprocess
import tempfile


def function(source, name):
    match = re.search(r'^(?:static )?(?:unsigned )?\w+ ' + name + r'\([^;{}]*\)\s*\{', source, re.M)
    if match is None:
        raise RuntimeError(f'Missing production function: {name}')
    start = source.index('{', match.start())
    depth, end = 1, start + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[match.start():end] + '\n'


def main():
    here = Path(__file__).resolve().parent
    src = here.parents[1] / 'src'
    viewport = (src / 'viewport.c').read_text()
    types = re.search(r'^#define VIEWPORT_SELECTION_GOLD .*', viewport, re.M)[0] + '\n'
    for name in ('SceneBatch', 'Vertex', 'VertexColor', 'ViewportComponent',
                 'ViewportStanComponent', 'ViewportBoxPoint', 'ViewportBoxComponent', 'ViewportBgPlane',
                 'ViewportPickRay', 'ViewportConnectedEdge'):
        types += re.search(r'typedef struct ' + name + r'\s*\{.*?\} ' + name + ';', viewport, re.S)[0] + '\n'
    types += re.search(r'typedef enum ViewportBgSelectionScope\s*\{.*?\} ViewportBgSelectionScope;', viewport, re.S)[0] + '\n'
    types += re.search(r'typedef enum ViewportSelectionDomain\s*\{.*?\} ViewportSelectionDomain;', viewport, re.S)[0] + '\n'
    logic = ''.join(function(viewport, name) for name in (
        'ViewportTriangleHidden', 'ViewportBatchIsPickable',
        'ViewportSetFullbrightColor', 'ViewportSetTriangleColor',
        'ViewportCompareObjectIds', 'ViewportObjectSelected', 'ViewportSetObjectIds',
        'ViewportClearObjectSelection', 'ViewportPortalComponentMask', 'ViewportPortalGeometryIsFirst',
        'ViewportResolveActivePortal', 'ViewportClearBgSelection', 'ViewportClearAllSelection', 'ViewportCompareBoxPoints',
        'ViewportCompareBoxComponents', 'ViewportBoxComponentKey', 'ViewportApplyBoxComponents', 'ViewportApplyInverseComponents',
        'ViewportBgSelectionPoint', 'ViewportCanSelectBackground', 'ViewportBgRoomKey',
        'ViewportChangeBgSelection', 'ViewportSelectBackground',
        'ViewportCanSelectConnected', 'ViewportCompareConnectedEdges', 'ViewportConnectedRoot',
        'ViewportRememberSelectionPoint', 'ViewportSelectConnected',
        'ViewportStanVisible', 'ViewportCompareStanIds', 'ViewportStanTileHidden',
        'ViewportStanPointRef', 'ViewportCanSelectStan', 'ViewportStanSelectionPoint',
        'ViewportInverseDomain', 'ViewportCanSelectInverse', 'ViewportInvertStanSelection',
        'ViewportInvertPortalSelection', 'ViewportInvertModelSelection', 'ViewportSelectInverse',
        'ViewportChangeStanSelection', 'ViewportCanGrowSelection', 'ViewportGrowSelection',
        'ViewportCanSelectRoom', 'ViewportSelectRoom',
        'ViewportBgFacePlane', 'ViewportGetSelectedBgPlanes', 'ViewportCanSelectCoplanar',
        'ViewportBgFaceCoplanar', 'ViewportSelectCoplanar',
        'ViewportGetSelectedBgTextures', 'ViewportCanSelectSameMaterial', 'ViewportSelectBgMaterial',
        'ViewportSelectSameMaterial', 'ViewportSelectMaterialInRoom'))
    editor = (src / 'geditor.c').read_text()
    grow = editor.split('EnableMenuItem((HMENU)wparam, ID_SELECT_GROW,')[1].split(';', 1)[0]
    assert 'ViewportCanGrowSelection(g_Viewport)' in grow
    grow = editor.split('case ID_SELECT_GROW:')[1].split('case ID_SELECT_ALL:', 1)[0]
    assert 'ViewportGrowSelection(g_Viewport)' in grow
    menu = editor.split('EnableMenuItem((HMENU)wparam, ID_SELECT_ROOM,')[1].split(';', 1)[0]
    assert 'ViewportCanSelectRoom(g_Viewport)' in menu
    menu = editor.split('EnableMenuItem((HMENU)wparam, ID_SELECT_COPLANAR,')[1].split(';', 1)[0]
    assert 'ViewportCanSelectCoplanar(g_Viewport)' in menu
    items = re.findall(r'AppendMenu\(selectmenu, MF_STRING, (ID_SELECT_\w+),', editor)
    assert items[items.index('ID_SELECT_ALL') + 1] == 'ID_SELECT_INVERSE'
    assert items[items.index('ID_SELECT_INVERSE') + 1] == 'ID_SELECT_CONNECTED'
    assert items[items.index('ID_SELECT_CONNECTED') + 1] == 'ID_SELECT_COPLANAR'
    assert 'Select &Inverse\\tCtrl+I' in editor
    assert 'Select Connected\\tL' in editor
    menu = editor.split('EnableMenuItem((HMENU)wparam, ID_SELECT_CONNECTED,')[1].split(';', 1)[0]
    assert 'ViewportCanSelectConnected(g_Viewport)' in menu
    command = editor.split('case ID_SELECT_CONNECTED:')[1].split('return 0;', 1)[0]
    assert 'ViewportSelectConnected(g_Viewport)' in command
    menu = editor.split('EnableMenuItem((HMENU)wparam, ID_SELECT_INVERSE,')[1].split(';', 1)[0]
    assert 'ViewportCanSelectInverse(g_Viewport)' in menu
    command = editor.split('case ID_SELECT_INVERSE:')[1].split('case ID_SELECT_ROOM:', 1)[0]
    assert 'ViewportSelectInverse(g_Viewport)' in command
    assert items[items.index('ID_SELECT_SAME_MATERIAL') + 1] == 'ID_SELECT_MATERIAL_IN_ROOM'
    assert 'ID_SELECT_MATERIAL_IN_ROOM, "Select Material in Room\\tAlt+M"' in editor
    menu = editor.split('EnableMenuItem((HMENU)wparam, ID_SELECT_MATERIAL_IN_ROOM,')[1].split(';', 1)[0]
    assert 'ViewportCanSelectSameMaterial(g_Viewport)' in menu
    command = editor.split('case ID_SELECT_MATERIAL_IN_ROOM:')[1].split('return 0;', 1)[0]
    assert 'ViewportSelectMaterialInRoom(g_Viewport)' in command
    hotkeys = function((src / 'geditor.c').read_text(), 'GEditorHandleSelectionHotkey')
    with tempfile.TemporaryDirectory(prefix='geditor-bg-selection-') as temp:
        temp = Path(temp)
        (temp / 'types.inc').write_text(types)
        (temp / 'logic.inc').write_text(logic)
        (temp / 'hotkeys.inc').write_text(hotkeys)
        subprocess.run([os.environ.get('CC', 'cc'), '-std=c99', '-O1', '-g', '-Wall', '-Wextra',
                        '-Werror', '-Wno-unused-parameter', '-fsanitize=address,undefined',
                        f'-I{here.parent / "image_import"}', f'-I{src}', f'-I{temp}',
                        str(here / 'check.c'), '-lm', '-o', str(temp / 'check')], check=True)
        subprocess.run([str(temp / 'check')], check=True,
                       env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))


if __name__ == '__main__':
    main()
