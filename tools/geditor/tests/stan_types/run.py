#!/usr/bin/env python3
"""Check native stan types, transactions, persistence and property controls."""
import importlib.util
import os
from pathlib import Path
import subprocess
import sys
import tempfile

here = Path(__file__).resolve().parent
src = here.parents[1] / 'src'
sys.dont_write_bytecode = True
spec = importlib.util.spec_from_file_location('extract', here.parent / 'portal_editing/run.py')
extract = importlib.util.module_from_spec(spec)
spec.loader.exec_module(extract)
with tempfile.TemporaryDirectory(prefix='geditor-stan-types-') as temp:
    work = Path(temp)
    (work / 'stan').mkdir()
    viewport = (src / 'viewport.c').read_text()
    (work / 'selection.inc').write_text(''.join(extract.function(viewport, n) for n in
        ('ViewportStanVisible', 'ViewportGetStanSelectionCount', 'ViewportGetSelectedStanTiles')))
    (work / 'controller.inc').write_text(extract.function((src / 'geditor.c').read_text(), 'GEditorSetSelectedStanType'))
    export = (src / 'romexport.c').read_text()
    (work / 'export.inc').write_text('#include "actionblocks.h"\n#include "textbank.h"\n' + ''.join(extract.function(export, n) for n in
        ('RomExportSetError', 'RomExportEndsWith', 'RomExportSimpleResourceName', 'RomExportProjectResourcePath', 'RomExportReadResource')))
    textbank = (src / 'textbank.c').read_text()
    with (work / 'export.inc').open('a') as out:
        out.write(''.join(extract.function(textbank, n) for n in ('TextBankIsResource', 'TextBankProjectPath')))
    helpers = (here.parent / 'stan_deletion/check.c').read_text()
    (work / 'helpers.inc').write_text(''.join(extract.function(helpers, n) for n in ('Put', 'Put16', 'Get', 'Fixture', 'Same')))
    common = (here.parent / 'stan_rooms/check.c').read_text()
    (work / 'persistence.inc').write_text(''.join(extract.function(common, n) for n in
        ('ById', 'PointerId', 'Equivalent', 'Grouped', 'Persist')))
    (work / 'properties.inc').write_text(''.join(extract.function((src / 'rightpanel.c').read_text(), n) for n in
        ('RightPanelUpdateStanType', 'RightPanelApplyStanType')))
    binary = work / 'check'
    subprocess.run([os.environ.get('CC', 'cc'), '-std=c99', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
                    '-Wno-unused-parameter', '-ffunction-sections', '-fdata-sections', '-fsanitize=address,undefined',
                    f'-I{here.parent / "image_import"}', f'-I{src}', f'-I{work}', str(here / 'check.c'),
                    str(here.parent / 'image_import/platform.c'),
                    *[str(src / name) for name in ('actionblocks.c', 'stanload.c', 'stanedit.c', 'stanquery.c', 'bghistory.c')],
                    '-Wl,--gc-sections', '-Wl,--wrap=malloc', '-Wl,--wrap=calloc', '-lm', '-o', str(binary)], check=True)
    subprocess.run([str(binary), str(work)], check=True,
                   env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))
    subprocess.run([os.environ.get('CC', 'cc'), '-std=c99', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
                    '-Wno-unused-parameter', '-fsanitize=address,undefined',
                    f'-I{here.parent / "image_import"}', f'-I{src}', f'-I{work}', str(here / 'properties.c'),
                    '-o', str(work / 'properties')], check=True)
    subprocess.run([str(work / 'properties')], check=True,
                   env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))
    (work / 'context.inc').write_text(''.join(extract.function(viewport, n) for n in
        ('ViewportStanVisible', 'ViewportContextStanType', 'ViewportShowGeometryContextMenu')))
    subprocess.run([os.environ.get('CC', 'cc'), '-std=c99', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
                    '-Wno-unused-parameter', '-fsanitize=address,undefined',
                    f'-I{here.parent / "image_import"}', f'-I{src}', f'-I{work}',
                    str(here.parent / 'reverse_edge/context.c'), '-o', str(work / 'context')], check=True)
    subprocess.run([str(work / 'context')], check=True,
                   env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))
