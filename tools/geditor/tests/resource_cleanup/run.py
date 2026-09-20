#!/usr/bin/env python3
"""Save/export cleanup regressions. Optional argument: a local GUD ROM."""
import os
import importlib.util
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

here = Path(__file__).resolve().parent
src = here.parents[1] / 'src'
root = src.parents[2]

def resources(data):
    u = lambda at: struct.unpack_from('>I', data, at)[0]
    manifest = data.index(b'GUDGEDITORMANIF\0')
    entries = {data[at:at+4]: struct.unpack_from('>III', data, at+4)
               for at in range(manifest+24, manifest+24+16*u(manifest+20), 16)}
    cmap = entries[b'CMAP']; at = entries[b'FTBL'][0]; rows = []
    while u(at+4):
        name = u(at+4)-cmap[2]+cmap[0]
        rows.append((data[name:data.index(0,name)].decode(),u(at+8))); at += 12
    offsets = sorted({off for name,off in rows if name} | {entries[b'OBSG'][1]})
    return {name: data[off:offsets[offsets.index(off)+1]] for name,off in rows if name}

with tempfile.TemporaryDirectory(prefix='geditor-cleanup-') as folder:
    work = Path(folder)
    for sub in ('bg','stan','setup'): (work/sub).mkdir()
    binary = work/'check'
    command = [os.environ.get('CC','cc'),'-O1','-g','-std=c99','-Wall','-Wextra','-Werror',
        '-Wno-unused-parameter','-Wno-format','-ffunction-sections','-fdata-sections',
        '-fsanitize=address,undefined',f'-I{here.parent/"project_rebase"}',f'-I{src}',f'-I{root}',
        str(here/'check.c'),str(here.parent/'project_rebase/platform.c')]
    command += [str(src/name) for name in ('rom.c','setupload.c','setupstan.c','actionblocks.c','levelissues.c',
        'stanload.c','stanquery.c','bgload.c','bgdocument.c','bgcompile.c','bgmaterial.c','bgrender.c')]
    subprocess.run(command+['-Wl,--gc-sections','-lm','-o',str(binary)],check=True)
    subprocess.run([str(binary),str(work)]+sys.argv[1:],check=True,
        env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1'))
    if len(sys.argv)>1:
        original=resources(Path(sys.argv[1]).read_bytes())
        first=resources((work/'pass1.z64').read_bytes())
        second=resources((work/'pass2.z64').read_bytes())
        assert first==second, 'Repeated export must be byte-identical in every resource slot'
        assert original.keys()==first.keys()
        spec=importlib.util.spec_from_file_location('setup_check',here.parent/'setup_compaction/run.py')
        setup_check=importlib.util.module_from_spec(spec);spec.loader.exec_module(setup_check)
        for name,old in original.items():
            if not name.startswith(('bg/','Tbg_','Usetup','Ump_setup')):
                assert old==first[name], name
            elif name.startswith(('Usetup','Ump_setup')) and len(old)>=40:
                a,b=setup_check.signature(old),setup_check.signature(first[name])
                for section in (6,7):
                    # Only the derived tile-name cache may change. All pad
                    # positions, orientations, bounds, markers and indices stay.
                    if a[section] is not None:
                        a[section]=[(p[0],p[2]) for p in a[section]]
                        b[section]=[(p[0],p[2]) for p in b[section]]
                assert a==b,name
        for name in ('bg/bg_depo_all_p.seg','Tbg_depo_all_p_stanZ','UsetupdepoZ'):
            print(f'{name}: {len(original[name])} -> {len(first[name])} bytes')
        print(f'PASS: {len(first)} resource names; repeat export identical; unrelated assets unchanged.')
