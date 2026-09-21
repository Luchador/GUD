#!/usr/bin/env python3
"""Create ROM dialog/preferences on a host registry and window-message shim."""
import importlib.util
import os
from pathlib import Path
import subprocess
import sys
import tempfile

here = Path(__file__).resolve().parent
src = here.parents[1] / 'src'
sys.dont_write_bytecode = True
spec = importlib.util.spec_from_file_location('extract', here.parent / 'zoom_selected/run.py')
extract = importlib.util.module_from_spec(spec)
spec.loader.exec_module(extract)
export = (src / 'romexport.c').read_text()
editor = (src / 'geditor.c').read_text()
with tempfile.TemporaryDirectory(prefix='geditor-rom-dialog-') as directory:
    work = Path(directory)
    (work / 'path_export.inc').write_text('\n'.join(extract.function(export, name) for name in (
        'RomExportAsciiLower', 'RomExportAsciiEquals', 'RomExportEndsWithNoCase',
        'RomExportReservedDeviceName', 'RomExportNameIsValid', 'RomExportBasePath',
        'RomExportBuildOutputPath', 'RomExportDestinationIsValid')))
    (work / 'dialog.inc').write_text('\n'.join(extract.function(editor, name) for name in (
        'GEditorUpdateRomExportValidity', 'GEditorCreateRomProc')))
    command = [os.environ.get('CC', 'cc'), '-std=c99', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
        '-Wno-unused-parameter', '-fsanitize=address,undefined', f'-I{here}', f'-I{src}', f'-I{work}',
        str(here / 'check.c'), '-o', str(work / 'check')]
    subprocess.run(command, check=True)
    subprocess.run([str(work / 'check')], check=True,
        env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))
