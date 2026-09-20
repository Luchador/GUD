#!/usr/bin/env python3
"""Frustum fit, timed movement, real selection bounds and scoped Z routing."""
import os
from pathlib import Path
import re
import subprocess
import tempfile


def function(source, name):
    match = re.search(r'^[\w *]+\b' + name + r'\([^;{}]*\)\s*\{', source, re.M)
    if not match:
        raise RuntimeError(f'Missing function: {name}')
    depth, end = 1, source.index('{', match.start()) + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[match.start():end] + '\n'


def main():
    here = Path(__file__).resolve().parent
    src = here.parents[1] / 'src'
    viewport = (src / 'viewport.c').read_text()
    editor = (src / 'geditor.c').read_text()
    types = ''
    for name in ('Vertex', 'SceneBatch', 'ViewportComponent', 'ViewportStanComponent', 'ViewportPad'):
        types += re.search(r'typedef struct ' + name + r' \{.*?\} ' + name + ';', viewport, re.S)[0] + '\n'
    for name in ('BOX_VERTICES', 'MARKER_MODEL_SCALE', 'OBJECT_NONE', 'FOV_Y', 'NEAR_Z', 'ZOOM_TIMER'):
        types += re.search(r'^#define VIEWPORT_' + name + r' .*', viewport, re.M)[0] + '\n'
    names = ('ViewportMarkerAt', 'ViewportSelectedMarker', 'ViewportSelectedPadIndex', 'ViewportPadVisible',
             'ViewportTriangleHidden', 'ViewportCornerVisible', 'ViewportStanVisible',
             'ViewportCompareStanIds', 'ViewportStanTileHidden', 'ViewportCompareStanRefs',
             'ViewportStanPointRef', 'ViewportFindStanComponent', 'ViewportPortalComponentMask',
             'ViewportPortalPointMask', 'ViewportExtendSelectionBounds', 'ViewportSelectionBounds',
             'ViewportCanZoomToSelected', 'ViewportCancelZoom', 'ViewportZoomFrame', 'ViewportZoomToBounds', 'ViewportZoomToSelected',
             'ViewportRevealStanTile')
    # The cancellation prefix is compiled with stubbed window calls below.
    wnd = function(viewport, 'ViewportWndProc')
    cancel = wnd[wnd.index('    if (msg == WM_SIZE'):wnd.index('    switch (msg)')]
    assert editor.index('AppendMenu(viewmenu, MF_STRING, ID_VIEW_ZOOM_SELECTED') < editor.index('AppendMenu(viewmenu, MF_STRING, ID_VIEW_BACKFACE_CULLING')
    assert editor.count('!GEditorHandleZoomSelectedHotkey(hwnd, &msg)') == 2
    assert 'ViewportCancelZoom(hwnd, state);' in function(viewport, 'ViewportSetScene')
    assert 'ViewportCancelZoom(hwnd, state);' in function(viewport, 'ViewportMoveCameraToSpawn')
    with tempfile.TemporaryDirectory(prefix='geditor-zoom-') as directory:
        work = Path(directory)
        (work / 'types.inc').write_text(types)
        (work / 'logic.inc').write_text(''.join(function(viewport, n) for n in names))
        (work / 'cancel.inc').write_text(cancel)
        (work / 'hotkey.inc').write_text(function(editor, 'GEditorHandleZoomSelectedHotkey'))
        subprocess.run([os.environ.get('CC', 'cc'), '-std=c99', '-O1', '-g', '-Wall', '-Wextra',
                        '-Werror', '-Wno-unused-parameter', '-fsanitize=address,undefined',
                        f'-I{here.parent / "image_import"}', f'-I{src}', f'-I{work}',
                        str(here / 'check.c'), str(src / 'cameraframe.c'), '-lm', '-o', str(work / 'check')], check=True)
        subprocess.run([str(work / 'check')], check=True,
                       env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))


if __name__ == '__main__':
    main()
