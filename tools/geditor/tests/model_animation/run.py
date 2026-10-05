#!/usr/bin/env python3
"""Native packed/expanded animation decoding, posing and malformed-bank checks."""
import os
from pathlib import Path
import re
import runpy
import struct
import subprocess
import tempfile

here = Path(__file__).resolve().parent
src = here.parents[1] / 'src'
root = src.parents[2]
convert = runpy.run_path(str(root / 'tools/make_animation_entries_uncompressed.py'))
entries = (root / 'assets/animationtable_entries.c').read_text()
headers = (root / 'assets/animationtable_data.c').read_text()
entry_header = (root / 'assets/animationtable_entries.h').read_text()
expanded, expanded_header, expanded_data = convert['convert'](entries, headers, entry_header)


def bank(data_source, defines):
    offsets = dict((name, int(value, 16)) for name, value in
                   re.findall(r'#define\s+(PTR_ANIM_ENTRY_\w+)\s+(0x\w+)', defines))
    out = bytearray()
    expected = dict((name, int(value, 16)) for name, value in re.findall(
        r'#define PTR_ANIM_(\w+)\s+(0x\w+)', (root / 'assets/animationtable_data.h').read_text()))
    for name, body in re.findall(r'u32 ANIM_DATA_(\w+)\[\]\s*=\s*\{(.*?)\};', data_source, re.S):
        if name != 'empty':
            assert len(out) == expected[name], name
        for token in re.findall(r'PTR_ANIM_ENTRY_\w+|0x[\da-fA-F]+', body):
            out += struct.pack('>I', offsets[token] if token in offsets else int(token, 16))
    return out


with tempfile.TemporaryDirectory(prefix='geditor-animation-') as temp:
    work = Path(temp)
    (work / 'headers.bin').write_bytes(bank(expanded_data, expanded_header))
    (work / 'frames.bin').write_bytes(expanded)
    (work / 'packed-headers.bin').write_bytes(bank(headers, entry_header))
    (work / 'packed-frames.bin').write_bytes(b''.join(data for _, data in convert['parse_entry_arrays'](entries)))
    command = [os.environ.get('CC', 'cc'), '-std=c99', '-O1', '-g', '-Wall', '-Wextra',
               '-Werror', '-Wno-unused-parameter', '-ffunction-sections', '-fdata-sections',
               '-fsanitize=address,undefined', f'-I{here.parent / "image_import"}', f'-I{src}', f'-I{root}',
               str(here / 'check.c')]
    command += [str(src / name) for name in ('modelanimation.c', 'modelload.c', 'modelmaterials.c',
                                           'modelcompile.c', 'bgmaterial.c', 'bgrender.c')]
    subprocess.run(command + ['-Wl,--gc-sections', '-lm', '-o', str(work / 'check')], check=True)
    subprocess.run([str(work / 'check'), str(root), str(work)], check=True,
                   env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))
    extract = runpy.run_path(str(here.parent / 'model_name_completion/run.py'))['function']
    editor = (src / 'modeleditor.c').read_text()
    viewport = (src / 'viewport.c').read_text()
    logic = extract(viewport, 'ViewportSetModelPose')
    logic += ''.join(extract(editor, 'ModelEditorAnimation' + name) for name in
                     ('Controls', 'Stop', 'Clear', 'Draw', 'Load', 'Play'))
    timer = editor.split('case WM_TIMER:', 1)[1].split('case WM_MEASUREITEM:', 1)[0]
    logic += 'static BOOL Tick(HWND hwnd, unsigned wparam) {' + timer.rsplit('break;', 1)[0] + 'return FALSE;}\n'
    commands = editor.split('case WM_COMMAND:', 1)[1].split('if (LOWORD(wparam) == IDC_MODEL_UV)', 1)[0]
    logic += 'static BOOL Command(HWND hwnd, uintptr_t wparam) {' + commands + 'return FALSE;}\n'
    (work / 'logic.inc').write_text(logic)
    subprocess.run(command[:command.index(str(here / 'check.c'))] + [f'-I{work}', str(here / 'ui.c'),
                    '-lm', '-o', str(work / 'ui')], check=True)
    subprocess.run([str(work / 'ui')], check=True,
                   env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))
