#!/usr/bin/env python3
"""Bridge authoring/serialization/history plus toolbar routing and edge queries."""
import importlib.util
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile


def main():
    here = Path(__file__).resolve().parent
    src = here.parents[1] / 'src'
    shim = here.parent / 'image_import'
    sys.dont_write_bytecode = True
    spec = importlib.util.spec_from_file_location('extract', here.parent / 'bg_primitives/run.py')
    helpers = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(helpers)
    with tempfile.TemporaryDirectory(prefix='geditor-geometry-toolbar-') as temp:
        work = Path(temp)
        fixture = (here.parent / 'bg_primitives/check.c').read_text()
        (work / 'fixture.inc').write_text(''.join(helpers.function(fixture, n) for n in
            ('Put', 'Float', 'Fixture', 'RoundTrip')))
        command = [os.environ.get('CC', 'cc'), '-O1', '-g', '-std=c99', '-Wall', '-Wextra',
                   '-Werror', '-Wno-unused-parameter', '-ffunction-sections', '-fdata-sections',
                   '-fsanitize=address,undefined', f'-I{shim}', f'-I{src}', f'-I{work}']
        env = dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1')
        editor = (src / 'geditor.c').read_text()
        (work / 'bridge_editor.inc').write_text(helpers.function(editor, 'GEditorBridgeSelectedBgEdges'))
        sources = ('bgload.c', 'bgdocument.c', 'bgcompile.c', 'bgmaterial.c', 'bgrender.c', 'bghistory.c')
        subprocess.run(command + [str(here / 'bridge.c'), str(shim / 'platform.c')]
                       + [str(src / n) for n in sources]
                       + ['-Wl,--gc-sections', '-lm', '-o', str(work / 'bridge')], check=True)
        subprocess.run([str(work / 'bridge'), str(work)], check=True, env=env)

        viewport = (src / 'viewport.c').read_text()
        names = ('ViewportCompareVertexRefs', 'ViewportCompareFaceRefs', 'ViewportTriangleHidden',
                 'ViewportCornerVisible', 'ViewportResolveBgEdge', 'ViewportGetSelectedBgEdges',
                 'ViewportPrepareEdgeExtrusion', 'ViewportSelectBgEdges',
                 'ViewportCancelTransform', 'ViewportEndTransform')
        (work / 'viewport.inc').write_text(''.join(helpers.function(viewport, n) for n in names))
        harness = (here.parent / 'edge_extrusion/viewport.c').read_text()
        harness = harness.replace('int scenecount,batchcount,', 'int stancomponentcount;\n    int scenecount,batchcount,')
        # The shared fixture already includes the portal cancellation stubs.
        harness = harness.replace('    Begin(&s,TRUE,25);', '''    BgDocumentEdgeRef selected;
    assert(ViewportGetSelectedBgEdges(&s, &selected, 1) && selected.face.faceid == 11 && selected.corner == 0);
    assert(!ViewportGetSelectedBgEdges(&s, &selected, 2));
    s.stancomponentcount = 1; assert(!ViewportGetSelectedBgEdges(&s, &selected, 1)); s.stancomponentcount = 0;
    hidden[1] = 1; assert(!ViewportGetSelectedBgEdges(&s, &selected, 1)); hidden[1] = 0;
    s.tool = EDITOR_TOOL_VERTEX_SELECT; assert(!ViewportGetSelectedBgEdges(&s, &selected, 1));
    s.tool = EDITOR_TOOL_EDGE_SELECT;
    Begin(&s,TRUE,25);''', 1)
        (work / 'viewport.c').write_text(harness)
        subprocess.run(command + [str(work / 'viewport.c'), '-Wl,--gc-sections', '-lm',
                                  '-o', str(work / 'viewport')], check=True)
        subprocess.run([str(work / 'viewport')], check=True, env=env)

        toolbar = (src / 'tooltoolbar.c').read_text()
        enum = re.search(r'enum \{\s*ID_FILE_NEW_PROJECT.*?\n\};', editor, re.S)[0]
        (work / 'input.inc').write_text(helpers.function(editor, 'GEditorHandleBridgeEdgesHotkey'))
        (work / 'menu.inc').write_text(enum + '\n' + helpers.function(editor, 'GEditorCanFlipSelectedBgFaces')
            + helpers.function(editor, 'GEditorShowGeometryMenu'))
        constants = '\n'.join(re.findall(r'^#define TOOLTOOLBAR_(?:BUTTON_SIZE|MARGIN|BUTTON_COUNT|MENU_WIDTH|CORRECT_WIDTH) .*', toolbar, re.M))
        (work / 'layout.inc').write_text(constants + '\n' + helpers.function(toolbar, 'ToolToolbarLayout')
            + helpers.function(toolbar, 'ToolToolbarGetHeight'))
        subprocess.run(command + [str(here / 'menu.c'), str(shim / 'platform.c'), '-Wl,--gc-sections', '-o', str(work / 'menu')], check=True)
        subprocess.run([str(work / 'menu')], check=True, env=env)


if __name__ == '__main__':
    main()
