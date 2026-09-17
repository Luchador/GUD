#!/usr/bin/env python3
"""Exercise production custom-prop registration with real 32-bit layouts.

Linux x86/x86-64, GCC or Clang. Uses a freestanding -m32 executable, so no
32-bit libc package is needed. Kernels without IA32 support require the Python
unicorn package. An optional pobjdata.c reproduces the old bug.
ROM transfers and the stage pool are supplied by check.c; no GPU is simulated.
"""
import os
import errno
from pathlib import Path
import re
import shlex
import subprocess
import struct
import sys
import tempfile

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]


def structure(source, name):
    return re.search(r'typedef struct ' + name + r'\s*\{.*?\}\s*' + name + r';',
                     source, re.S)[0] + '\n'


def function(source, name):
    match = re.search(r'^[\w *]+\b' + name + r'\([^;]*?\)\s*\{', source, re.M)
    assert match, name
    end = source.index('{', match.start()) + 1
    depth = 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[match.start():end] + '\n'


def execute(binary):
    try:
        subprocess.run([str(binary)], check=True)
        return
    except OSError as error:
        if error.errno != errno.ENOEXEC:
            raise
    from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_INTR
    from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EAX, UC_X86_REG_EBX
    data = binary.read_bytes()
    entry, phoff = struct.unpack_from('<II', data, 24)
    phsize, phcount = struct.unpack_from('<HH', data, 42)
    segments = [struct.unpack_from('<8I', data, phoff + i * phsize) for i in range(phcount)]
    segments = [segment for segment in segments if segment[0] == 1 and segment[5]]
    start = min(s[2] for s in segments) & ~4095
    end = (max(s[2] + s[5] for s in segments) + 4095) & ~4095
    machine = Uc(UC_ARCH_X86, UC_MODE_32)
    machine.mem_map(start, end - start)
    for s in segments:
        machine.mem_write(s[2], data[s[1]:s[1] + s[4]])
    machine.mem_map(0x70000000, 0x100000)
    machine.reg_write(UC_X86_REG_ESP, 0x700ffff0)
    status = []
    def syscall(uc, interrupt, unused):
        assert interrupt == 0x80 and uc.reg_read(UC_X86_REG_EAX) == 1
        status.append(uc.reg_read(UC_X86_REG_EBX))
        uc.emu_stop()
    machine.hook_add(UC_HOOK_INTR, syscall)
    machine.emu_start(entry, 0, count=100000000)
    assert status == [0], f'32-bit runtime check failed: exit status {status}'


source = (Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / 'src/game/pobjdata.c').read_text()
types = (ROOT / 'src/bondtypes.h').read_text()
ob = (ROOT / 'src/game/ob.h').read_text()
code = '#define _LANGUAGE_C\n#define TARGET_N64\n#include <PR/ultratypes.h>\n'
code += '#include "src/custompropformat.h"\n#include "src/macro.h"\n'
code += '#include "assets/obseg/file_resource_id_enums.h"\n'
code += 'typedef struct ModelNode ModelNode;\ntypedef struct ModelFileTextures ModelFileTextures;\n'
code += 'typedef struct ModelSkeleton { int unused; } ModelSkeleton;\n'
for name in ('ModelFileHeader', 'ItemModelFileRecord'):
    code += structure(types, name)
for name in ('fileentry', 'resource_lookup_data_entry'):
    code += structure(ob, name)
code += structure((ROOT / 'src/game/customprops.h').read_text(), 'CustomPropRomConfig')
code += structure(source, 'CustomPropRuntime')
code += '''
#define MEMPOOL_STAGE 4
#define SKELETON(name) skeleton_ ## name
static ModelSkeleton skeleton_standard_object;
static CustomPropRuntime *g_CustomProps;
static s32 g_CustomPropCount;
static CustomPropRomConfig g_CustomPropRomConfig;
static void *mempAllocBytesInBank(u32 bytes, u8 bank);
static void romCopy(void *dst, void *src, u32 bytes);
static void bzero(void *dst, u32 bytes);
static void *memcpy(void *dst, const void *src, u32 bytes);
'''
code += function(source, 'customPropsReset')
code += function(source, 'customPropsInit')
code += (HERE / 'check.c').read_text()
with tempfile.TemporaryDirectory(prefix='gud-custom-prop-dma-') as directory:
    work = Path(directory)
    (work / 'check.c').write_text(code)
    command = shlex.split(os.environ.get('CC', 'cc'))
    command += ['-m32', '-std=c99', '-O2', '-Wall', '-Wextra', '-Werror',
                '-Wno-parentheses', '-ffreestanding', '-fno-builtin', '-fno-pie', '-no-pie',
                '-fno-stack-protector', '-nostdlib', f'-I{ROOT}', f'-I{ROOT / "include"}',
                str(work / 'check.c'), '-o', str(work / 'check')]
    subprocess.run(command, check=True)
    execute(work / 'check')
print('PASS: 0-128 registered models preserve 16-byte stage allocation and subsequent setup DMA; '
      'native 140-byte record layout, descriptors, reset and malformed headers checked.')
