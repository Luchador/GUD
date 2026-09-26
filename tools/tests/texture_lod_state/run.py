#!/usr/bin/env python3
"""Check production texture-command expansion, optionally against a BG and ROM.

Only allocation/uploads and host byte order are stubbed. The real expander
and texture-command writer must select the right LOD at every draw.
"""
from pathlib import Path
import argparse
import os
import re
import shlex
import struct
import subprocess
import tempfile

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]


def function(source, name):
    match = re.search(r'^(?:static )?[\w *]+\b' + name + r'\([^;]*?\)\s*\{', source, re.M)
    assert match, name
    end = source.index('{', match.start()) + 1
    depth = 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[match.start():end] + '\n'


def rom_fixture(rompath, bgpath):
    rom, bg = rompath.read_bytes(), bgpath.read_bytes()

    def word(data, offset):
        return struct.unpack_from('>I', data, offset)[0]

    marker = rom.index(b'GUDGEDITORMANIF\0\0')
    entries = dict((kind, (start, end)) for kind, start, end, flags in
                   (struct.unpack_from('>4sIII', rom, marker + 24 + i * 16)
                    for i in range(word(rom, marker + 20))))
    pos, end = entries[b'IMGS']
    levels = bytearray()
    while pos < end and rom[pos:pos + 4] == b'GUTX':
        size = word(rom, pos + 12)
        assert size >= 100 and pos + size <= end
        record = rom[pos:pos + size]
        count, fmt, width, height = record[5], record[16], record[17], record[18]
        assert count <= 7
        # Match texLoadRaw's cap on generated paletted levels.
        if not record[4] and count >= 2 and struct.unpack_from('>H', record, 8)[0]:
            def rowbytes(w):
                return (w + 7) & ~7 if fmt in (9, 11) else ((w + 15) & ~15) // 2
            total = rowbytes(width) * height
            for lod in range(1, count):
                width, height = (width + 1) // 2, (height + 1) // 2
                total += rowbytes(width) * height
                if total > 0x800:
                    count = lod
                    break
        levels.append(count)
        pos += size
    assert 0 < len(levels) <= 4096
    assert word(bg, 0) == 0
    table = word(bg, 4) & 0xffffff
    streams = []
    for room in range(1, 4096):
        row = table + room * 24
        assert row + 24 <= len(bg)
        if not word(bg, row + 4):
            break
        for layer in range(2):
            start = word(bg, row + 4 + layer * 4) & 0xffffff
            if start:
                size = word(bg, start - 4)
                assert size % 8 == 0 and start + size <= len(bg)
                streams.append(struct.pack('>III', room, layer, size) + bg[start:start + size])
    return struct.pack('>I', len(levels)) + levels + struct.pack('>I', len(streams)) + b''.join(streams)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--rom', type=Path)
    parser.add_argument('--bg', type=Path, help='Uncompressed GEditor .seg file')
    args = parser.parse_args()
    if bool(args.rom) != bool(args.bg):
        parser.error('--rom and --bg must be supplied together')
    tex = Path(os.environ.get('TEX_SOURCE', ROOT / 'src/game/tex.c')).read_text()
    source = (HERE.parent / 'render_options/harness.h').read_text().split('#define PLAYER_1')[0]
    source += (ROOT / 'src/bgtransparency.h').read_text()
    source += re.search(r'struct tex \{.*?\n};', (ROOT / 'src/game/image.h').read_text(), re.S)[0] + '\n'
    source += '#include <gbi_extension.h>\n'
    source += (HERE / 'harness.h').read_text()
    source += function(tex, 'texWriteTextureCmd')
    expander = function(tex, 'texLoadFromGdl')
    expander = expander.replace('switch (*(u8 *)in)', 'switch (in->words.w0 >> 24)')
    expander = expander.replace('((s32)out) - ((s32)dst)', '(s32)((u8 *)out - (u8 *)dst)')
    expander = re.sub(r'    s32\s+pad;\n', '', expander)
    source += expander + (HERE / 'check.c').read_text()
    with tempfile.TemporaryDirectory(prefix='gud-texture-lod-') as directory:
        work = Path(directory)
        (work / 'check.c').write_text(source)
        (work / 'PR').mkdir()
        gbi = (ROOT / 'include/PR/gbi.h').read_text()
        gbi = gbi.replace('uintptr_t w0;', 'u32 w0;').replace('uintptr_t w1;', 'u32 w1;')
        (work / 'PR/gbi.h').write_text(gbi)
        command = shlex.split(os.environ.get('CC', 'cc')) + [
            '-std=c99', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
            '-Wno-unused-parameter', '-fsanitize=address,undefined',
            '-I', str(work), '-idirafter', str(ROOT / 'include'),
            str(work / 'check.c'), '-o', str(work / 'check')]
        subprocess.run(command, check=True)
        command = [str(work / 'check')]
        if args.rom:
            fixture = work / 'bg.bin'
            fixture.write_bytes(rom_fixture(args.rom, args.bg))
            command.append(str(fixture))
        subprocess.run(command, check=True, env=dict(os.environ,
                       ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))


if __name__ == '__main__':
    main()
