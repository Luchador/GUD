#!/usr/bin/env python3
"""Native Action Block round trips, edit safety, project IO and export tests.

Builds all stock setup sources with IDO when its compiler is available. The
standalone synthetic suite needs only a C compiler; no ROM assets or GUI.
"""
import importlib.util
import json
import sys
import os
from pathlib import Path
import re
import struct
import subprocess
import tempfile


def function(source, name):
    match = re.search(r'^static [^\n]+\b' + name + r'\([^;{}]*\)\s*\{', source, re.M)
    assert match, name
    end = source.index('{', match.start()) + 1
    depth = 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[match.start():end] + '\n'


def corpus(root, work, here):
    compiler = root / 'tools/ido5.3_recomp/cc'
    if not compiler.exists():
        print('IDO unavailable: skipping stock setup corpus; synthetic tests still run.')
        return []
    sys.dont_write_bytecode = True
    spec = importlib.util.spec_from_file_location('elf', here.parent / 'manifest_catalogs/run.py')
    elf = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(elf)
    names = []
    for path in sorted((root / 'assets/obseg/setup').glob('*.c')):
        if not re.search(r'stagesetup\s+' + path.stem + r'\s*=', path.read_text()):
            continue
        target = work / (path.stem + '.o')
        command = [str(compiler), '-c', '-non_shared', '-Olimit', '2000', '-G', '0', '-Xcpluscomm',
                   '-woff', '609,649,709,712,807,838,763', '-mips2', '-32', '-DVERSION_US', '-DTARGET_N64', '-O2']
        for include in ('build/u', '.', 'include', 'include/ultra64', 'include/PR', 'src', 'src/game', 'src/inflate'):
            command += ['-I', str(root / include)]
        subprocess.run(command + ['-o', str(target), str(path)], cwd=root, check=True, capture_output=True)
        obj = elf.Object(target)
        header = obj.named[path.stem]
        assert header[1] == 0 and header[2] == 40
        # Allocate native data sections, with the setup header first. Resolve
        # actual emitted R_MIPS_32 relocations to setup-relative offsets.
        sections = [header[3]] + [i for i, s in enumerate(obj.sections)
                                   if i != header[3] and s[1] in (1, 8) and s[2] & 2]
        bases, data = {}, bytearray()
        for i in sections:
            s = obj.sections[i]
            data.extend(bytes((-len(data)) % max(s[8], 4)))
            bases[i] = len(data)
            data.extend(bytes(s[5]) if s[1] == 8 else obj.data[s[4]:s[4]+s[5]])
        for (section, at), (name, kind) in obj.relocations.items():
            if section not in bases:
                continue
            symbol = obj.named[name]
            assert kind == 2 and symbol[3] in bases, (path.name, name, kind)
            pos = bases[section] + at
            value = struct.unpack_from('>I', data, pos)[0] + bases[symbol[3]] + symbol[1]
            struct.pack_into('>I', data, pos, value)
        (work / 'setup' / (path.stem + '.set')).write_bytes(data)
        table = struct.unpack_from('>I', data, 20)[0]
        if table and struct.unpack_from('>I', data, table)[0]:
            names.append(path.stem)
    # Compile the actual shared scripts, including legacy variable-length PRINT.
    source = (root / 'src/game/chraidata.c').read_text()
    source = re.sub(r'\bPRINT\(("(?:\\.|[^"\\])*")\)',
                    lambda m: 'AI_PRINT,' + ','.join(str(v) for v in json.loads(m[1]).encode('ascii') + b'\0') + ',', source)
    shared = work / 'shared.c'
    shared.write_text(source)
    target = work / 'shared.o'
    subprocess.run(command + ['-o', str(target), str(shared)], cwd=root, check=True, capture_output=True)
    obj = elf.Object(target)
    descriptor = obj.named['g_GedGlobalAILists']
    sections = [descriptor[3]] + [i for i, sec in enumerate(obj.sections)
                                 if i != descriptor[3] and sec[1] in (1, 8) and sec[2] & 2]
    bases, data = {}, bytearray()
    for i in sections:
        sec = obj.sections[i]
        data.extend(bytes((-len(data)) % max(sec[8], 4)))
        bases[i] = len(data)
        data.extend(bytes(sec[5]) if sec[1] == 8 else obj.data[sec[4]:sec[4]+sec[5]])
    for (section, at), (name, kind) in obj.relocations.items():
        if section not in bases:
            continue
        symbol = obj.named[name]
        assert kind == 2 and symbol[3] in bases, (name, kind)
        pos = bases[section] + at
        value = struct.unpack_from('>I', data, pos)[0] + bases[symbol[3]] + symbol[1] + 0x80000000
        struct.pack_into('>I', data, pos, value)
    (work / 'shared.bin').write_bytes(struct.pack('>II', bases[descriptor[3]]+descriptor[1], len(data)) + data)
    return names


def layouts(root, work):
    source = '''#define BYTE(n) FIELD(n,1)
#define DBYTE(n) FIELD(n,2)
#define TBYTE(n) FIELD(n,3)
#define QBYTE(n) FIELD(n,4)
#define _AI_CMD(...) COMMAND(CMDNAME, PARAMS)
#define _AI_DEBUG(...) COMMAND(CMDNAME, PARAMS)
#define _AI_CMD_POLYMORPH(...)
#define DEFINE(...)
#include "src/aicommands.def"
'''
    result = subprocess.run(['cc', '-E', '-P', '-I.', '-Iinclude', '-x', 'c', '-'],
                            input=source, text=True, cwd=root, capture_output=True, check=True).stdout
    lines = [line for line in result.splitlines() if line.startswith('COMMAND(')]
    assert len(lines) == 253
    checks = []
    for opcode, line in enumerate(lines):
        name = re.match(r'COMMAND\((\w+),', line)[1]
        fields = re.findall(r'FIELD\((\w+),\s*(\d)\)', line)
        assert len(fields) <= 8
        checks += [f'assert(!strcmp(g_ActionOpcodes[{opcode}].symbol,"{name}"));',
                   f'assert(g_ActionOpcodes[{opcode}].paramcount=={len(fields)});']
        offset = 1
        for i, (param, width) in enumerate(fields):
            checks += [f'assert(g_ActionOpcodes[{opcode}].params[{i}].width=={width});',
                       f'assert(g_ActionOpcodes[{opcode}].params[{i}].offset=={offset});']
            offset += int(width)
        checks.append(f'assert(g_ActionOpcodes[{opcode}].size=={0 if opcode == 0xad else offset});')
    (work / 'layouts.inc').write_text('\n'.join(checks))


def main():
    here = Path(__file__).resolve().parent
    root = here.parents[3]
    src = root / 'tools/geditor/src'
    shim = here.parent / 'image_import'
    with tempfile.TemporaryDirectory(prefix='geditor-actions-') as temp:
        work = Path(temp)
        (work / 'setup').mkdir()
        layouts(root, work)
        names = corpus(root, work, here)
        source = (src / 'romexport.c').read_text()
        exported = '#include <stdlib.h>\n#include <stdio.h>\n#include <stdarg.h>\n#include "setupmeta.h"\nstatic char g_RomExportError[256];\n'
        exported += function(source, 'RomExportSetError') + function(source, 'RomExportReadResource')
        exported += 'unsigned char *TestReadResource(const char *p,const char *r,DWORD *n,const char **why) {return RomExportReadResource(p,r,n,why);}\n'
        (work / 'export.c').write_text(exported)
        command = [os.environ.get('CC', 'cc'), '-std=c99', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
                   '-Wno-unused-parameter', '-ffunction-sections', '-fdata-sections', '-fsanitize=address,undefined',
                   f'-I{shim}', f'-I{src}', f'-I{root}', f'-I{work}', str(here / 'check.c'), str(work / 'export.c'),
                   str(shim / 'platform.c')]
        command += [str(src / f) for f in ('actionblocks.c', 'setupload.c', 'rotation.c', 'scaling.c', 'bghistory.c')]
        command += ['-Wl,--gc-sections', '-lm', '-o', str(work / 'check')]
        subprocess.run(command, check=True)
        subprocess.run([str(work / 'check'), str(work)] + names, check=True,
                       env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))


if __name__ == '__main__':
    main()
