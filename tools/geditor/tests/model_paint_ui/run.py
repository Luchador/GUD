#!/usr/bin/env python3
"""Check production model-panel layout and paint shortcut routing with Win32 stubs."""
import importlib.util
import os
from pathlib import Path
import re
import subprocess
import tempfile


def main():
    here = Path(__file__).resolve().parent
    src = here.parents[1] / 'src'
    spec = importlib.util.spec_from_file_location('extract', here.parent / 'vertex_eyedropper/run.py')
    helper = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(helper)
    picker = (src / 'colorpicker.c').read_text()
    toolbar = (src / 'tooltoolbar.c').read_text()
    editor = (src / 'modeleditor.c').read_text()
    types = '\n'.join(re.findall(r'^#define (?:COLORPICKER|TOOLTOOLBAR)_.*', picker + '\n' + toolbar, re.M))
    types += '\n' + re.search(r'typedef struct ColorPickerState \{.*?\} ColorPickerState;', picker, re.S)[0]
    types += '\n' + re.search(r'typedef struct ToolToolbarState \{.*?\} ToolToolbarState;', toolbar, re.S)[0]
    logic = helper.function(picker, 'ColorPickerLayout') + helper.function(toolbar, 'ToolToolbarHandleMessage')
    logic += helper.function(editor, 'ModelEditorPlaceControl') + helper.function(editor, 'ModelEditorLayout')
    logic += helper.function(editor, 'ModelEditorPaintKey')
    with tempfile.TemporaryDirectory(prefix='geditor-model-paint-ui-') as tmp:
        work = Path(tmp)
        (work / 'types.inc').write_text(types)
        (work / 'logic.inc').write_text(logic)
        subprocess.run([os.environ.get('CC', 'cc'), '-std=c99', '-O1', '-g', '-Wall', '-Wextra',
                        '-Werror', '-Wno-unused-parameter', '-fsanitize=address,undefined',
                        f'-I{here.parent / "image_import"}', f'-I{src}', f'-I{work}',
                        str(here / 'check.c'), '-lm', '-o', str(work / 'check')], check=True)
        subprocess.run([str(work / 'check')], check=True,
                       env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))


if __name__ == '__main__':
    main()
