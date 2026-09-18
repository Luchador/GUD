#!/usr/bin/env python3
"""Stan bridge geometry, traversal, grouped save/ROM data and editor history."""
import importlib.util
import os
from pathlib import Path
import subprocess
import tempfile

here = Path(__file__).resolve().parent
src = here.parents[1] / 'src'
spec = importlib.util.spec_from_file_location('extract', here.parent / 'portal_editing/run.py')
extract = importlib.util.module_from_spec(spec)
spec.loader.exec_module(extract)
with tempfile.TemporaryDirectory(prefix='geditor-stan-bridge-') as directory:
    work = Path(directory)
    (work / 'stan').mkdir()
    deletion = (here.parent / 'stan_deletion/check.c').read_text()
    topology = (here.parent / 'stan_topology/check.c').read_text()
    (work / 'fixture.inc').write_text(''.join(extract.function(deletion, n) for n in ('Put', 'Put16', 'Get', 'Same'))
        + ''.join(extract.function(topology, n) for n in ('Fixture', 'SetLink')))
    viewport = (src / 'viewport.c').read_text()
    names = ('ViewportStanVisible', 'ViewportCompareStanIds', 'ViewportStanTileHidden',
             'ViewportCompareStanRefs', 'ViewportStanPointRef',
             'ViewportClearStanSelection', 'ViewportGetStanSelectionCount', 'ViewportGetSelectedStanTiles',
             'ViewportSelectStanTiles', 'ViewportFindStanComponent', 'ViewportGetSelectedStanEdges',
             'ViewportSetStanVertex', 'ViewportRefreshStanOverlay', 'ViewportSetStanTiles')
    (work / 'viewport.inc').write_text(''.join(extract.function(viewport, n) for n in names))
    harness = topology[topology.index('typedef void *HWND;'):topology.index('static void Visibility(')]
    (work / 'viewport_harness.inc').write_text(harness)
    editor = (src / 'geditor.c').read_text()
    (work / 'controller.inc').write_text(extract.function(editor, 'GEditorBridgeSelectedStanEdges'))
    dispatch = editor.split('case ID_GEOMETRY_BRIDGE_EDGES:')[1].split('return 0;')[0]
    assert 'GEditorBridgeSelectedStanEdges(hwnd)' in dispatch and 'GEditorBridgeSelectedBgEdges(hwnd)' in dispatch
    export = (src / 'romexport.c').read_text()
    (work / 'export.inc').write_text('#include "actionblocks.h"\n' + ''.join(extract.function(export, n) for n in
        ('RomExportSetError', 'RomExportEndsWith', 'RomExportSimpleResourceName', 'RomExportProjectResourcePath', 'RomExportReadResource')))
    binary = work / 'check'
    subprocess.run([os.environ.get('CC', 'cc'), '-std=c99', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
                    '-Wno-unused-parameter', '-ffunction-sections', '-fdata-sections', '-fsanitize=address,undefined',
                    f'-I{here.parent / "image_import"}', f'-I{src}', f'-I{work}', str(here / 'check.c'),
                    str(here.parent / 'image_import/platform.c'),
                    *[str(src / n) for n in ('actionblocks.c', 'stanload.c', 'stantopology.c', 'stanedit.c',
                                           'standelete.c', 'stanquery.c', 'bghistory.c')],
                    '-Wl,--gc-sections', '-Wl,--wrap=malloc', '-Wl,--wrap=calloc', '-lm', '-o', str(binary)], check=True)
    subprocess.run([str(binary), str(work)], check=True,
                   env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))
