#!/usr/bin/env python3
"""Check production frame loading with 32-bit pointers and real animation bytes.

Linux x86/x86-64, Python 3 and GCC/Clang. The freestanding executable needs no
32-bit libc; kernels without IA32 support use Unicorn. ROM DMA is mocked.
"""
import importlib.util
import errno
import os
from pathlib import Path
import re
import shlex
import subprocess
import struct
import sys
import tempfile

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
sys.dont_write_bytecode = True


def function(source, name):
    match = re.search(r'^(?:static )?[\w *]+\b' + name + r'\([^;]*?\)\s*\{', source, re.M)
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
    segments = [s for s in segments if s[0] == 1 and s[5]]
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
    assert status == [0], f'Runtime check failed: exit status {status}'


def stock_frames():
    spec = importlib.util.spec_from_file_location('animation_converter', ROOT / 'tools/make_animation_entries_uncompressed.py')
    converter = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(converter)
    data, header, source = converter.convert(*[
        (ROOT / ('assets/animationtable_' + name)).read_text()
        for name in ('entries.c', 'data.c', 'entries.h')])
    offsets = {name: int(offset, 16) for name, offset in re.findall(
        r'#define\s+PTR_ANIM_ENTRY_(\w+)\s+(0x[0-9a-fA-F]+)', header)}
    samples = bytearray()
    frames = []
    for match in converter.DATA_HEADER_RE.finditer(source):
        size = (int(match['layout'], 16) & 0xffff) // 8
        count = int(match['info'], 16) >> 16
        for frame in sorted({0, count // 2, count - 1}):
            offset = offsets[match['name']] + frame * size
            frames.append((len(samples), size))
            samples.extend(data[offset:offset + size])
            # Retain enough readable padding for the final aligned PI transfer.
            samples.extend(bytes(16))
    samples.extend(bytes(16))
    text = 'static const u8 stockRom[] = {\n'
    for at in range(0, len(samples), 24):
        text += ','.join(str(b) for b in samples[at:at + 24]) + ',\n'
    text += '};\nstatic const struct {u32 offset, size;} stockFrames[] = {\n'
    text += ''.join('{%d,%d},\n' % frame for frame in frames) + '};\n'
    return text, len(frames)


model = (ROOT / 'src/game/model.c').read_text()
types = (ROOT / 'src/bondtypes.h').read_text()
source = '#define _LANGUAGE_C\n#define TARGET_N64\n#include <PR/ultratypes.h>\n'
source += 'typedef struct ModelAnimBitField ModelAnimBitField;\n'
source += re.search(r'typedef struct ModelAnimation\s*\{.*?\}\s*ModelAnimation;', types, re.S)[0] + '\n'
source += re.search(r'struct ModelAnimationScratch\s*\{.*?\};',
    (ROOT / 'src/game/objecthandler.h').read_text(), re.S)[0] + '\n'
source += ''.join(line + '\n' for line in model.splitlines() if line.startswith('#define MODEL_ANIM_FRAME_CACHE_'))
source += re.search(r'typedef struct ModelAnimFrameCacheEntry \{.*?\} ModelAnimFrameCacheEntry;', model, re.S)[0] + '\n'
source += ''.join(line + '\n' for line in model.splitlines() if line.startswith('static ') and 'g_ModelAnimFrameCache' in line)
source += '''static struct ModelAnimationScratch *g_ModelAnimationScratch;
static void romCopy(void *dest, void *source, u32 size);
static void *memcpy(void *dest, const void *source, u32 size);
'''
for name in ('modelResetAnimationFrameCache', 'modelLoadCachedAnimationFrame',
             'loadAnimationFrame', 'modelResetAnimationsScratchBuffer'):
    source += function(model, name)
source += function((ROOT / 'src/game/initunk_0009E0.c').read_text(), 'initAnimationsBuffer')
fixtures, count = stock_frames()
source += fixtures + '\n#line 1 "animation_frame_cache/check.c"\n' + (HERE / 'check.c').read_text()
with tempfile.TemporaryDirectory(prefix='gud-animation-cache-') as directory:
    work = Path(directory)
    (work / 'check.c').write_text(source)
    command = shlex.split(os.environ.get('CC', 'cc')) + [
        '-m32', '-std=c99', '-O2', '-Wall', '-Wextra', '-Werror',
        '-ffreestanding', '-fno-builtin', '-fno-pie', '-no-pie',
        '-fno-stack-protector', '-nostdlib', f'-I{ROOT / "include"}',
        str(work / 'check.c'), '-o', str(work / 'check')]
    subprocess.run(command, check=True)
    execute(work / 'check')
print(f'PASS: {count} real animation-frame samples, 64-slot reuse, collision/LRU eviction, '
      'four-frame blending lifetime, odd addresses, sizes, reset, direct RAM and oversized fallback.')
print('PASS: eight simulated guards requesting four distinct frames each for 120 updates '
      'make 32 DMA calls for 3840 frame requests; this checks reuse, not N64 frame rate.')
