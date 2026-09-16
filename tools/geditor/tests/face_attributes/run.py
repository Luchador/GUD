#!/usr/bin/env python3
"""Native UV correction and layer transfers, history, persistence and Depot."""
import importlib.util
import os
from pathlib import Path
import re
import struct
import subprocess
import sys
import tempfile
sys.dont_write_bytecode = True
here = Path(__file__).resolve().parent
src = here.parents[1] / 'src'
def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module
extract = load('extract', here.parent / 'portal_editing/run.py')
native = load('native', here.parent / 'bg_transparency/run.py')
with tempfile.TemporaryDirectory(prefix='geditor-face-attributes-') as temp:
    work = Path(temp)
    fixture = (here.parent / 'bg_transparency/edit.c').read_text()
    (work / 'fixture.inc').write_text(''.join(extract.function(fixture, n) for n in
        ('Put', 'Fixture', 'Refs', 'Equivalent', 'RoundTrip')).replace('assert(!memcmp(x.tags,y.tags,x.facecount*sizeof(*x.tags)));', 'for(DWORD i=0;i<x.facecount;i++)if(x.tags[i]!=y.tags[i]){fprintf(stderr,"tag difference %u room %u/%u tag %04x/%04x\\n",i,x.facerefs[i].room,y.facerefs[i].room,x.tags[i],y.tags[i]);abort();}'))
    common = (here.parent / 'bg_disconnect/check.c').read_text()
    (work / 'common.inc').write_text(extract.function(common, 'Same').replace('assert(!memcmp(x->vertices', 'if(x->vertexcount)assert(!memcmp(x->vertices').replace('assert(!memcmp(x->faces', 'if(x->facecount)assert(!memcmp(x->faces'))
    controller = (here.parent / 'face_rooms/check.c').read_text()
    controller = controller[controller.index('/* Run the real editor'):controller.index('int main(')]
    controller = controller.replace('GEditorMoveSelectedFacesToRoom', 'GEditorSetSelectedFaceLayer')
    controller = controller.replace('Change BG Room', 'Change BG Layer').replace('(HWND)1,2)', '(HWND)1,BG_GEOMETRY_SECONDARY)')
    controller = controller.replace('selection[0].room == 2 && selection[1].room == 2',
        'selection[0].layer == 1 && selection[1].layer == 1')
    (work / 'controller.inc').write_text(controller)
    (work / 'editor.inc').write_text(extract.function((src / 'geditor.c').read_text(), 'GEditorSetSelectedFaceLayer'))
    depot = (here.parents[3] / 'assets/obseg/bg/bg_depo_all_p.c').read_text()
    data = native.jungle_fixture(depot)
    for room, x, y, z in re.findall(r'\{&point_table_binary_(\d+),[^\n]+?, (-?[\d.]+), (-?[\d.]+), (-?[\d.]+)\}', depot):
        struct.pack_into('>fff', data, 32 + int(room)*24 + 12, float(x), float(y), float(z))
    (work / 'depot.seg').write_bytes(data)
    command = [os.environ.get('CC', 'cc'), '-std=c99', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
        '-Wno-unused-parameter', '-ffunction-sections', '-fdata-sections', '-fsanitize=address,undefined',
        f'-I{here.parent / "image_import"}', f'-I{src}', f'-I{work}']
    sources = ('bgload.c', 'bgcompile.c', 'bgmaterial.c', 'bgrender.c', 'bghistory.c')
    subprocess.run(command + [str(here / 'check.c'), str(here.parent / 'image_import/platform.c')]
        + [str(src / n) for n in sources] + ['-Wl,--gc-sections', '-lm', '-o', str(work / 'check')], check=True)
    subprocess.run([str(work / 'check'), str(work), str(work / 'depot.seg')], check=True,
        env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))
