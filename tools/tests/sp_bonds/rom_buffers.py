"""Build transient buffer-budget fixtures from a user's ROM (no stored assets)."""
from pathlib import Path
import struct


def fixture(rompath, target):
    data = rompath.read_bytes()
    def word(at): return struct.unpack_from('>I', data, at)[0]
    def half(at): return struct.unpack_from('>H', data, at)[0]
    def string(at): return data[at:data.index(0, at)].decode('ascii')
    m = data.index(b'GUDGEDITORMANIF\0')
    entries = {data[a:a+4]: (word(a+4), word(a+8), word(a+12))
               for a in range(m+24, m+24+word(m+20)*16, 16)}
    def address(p): return p-entries[b'CMAP'][2]+entries[b'CMAP'][0]
    files = {}
    for i in range(1, 2048):
        a = entries[b'FTBL'][0]+12*i
        if not word(a+4): break
        files[string(address(word(a+4)))] = (word(a+8), word(a+20)-word(a+8))
    def modeltable(kind):
        base, count, stride, version = struct.unpack_from('>4I', data, entries[kind][0])
        return [(i, address(word(address(base)+i*stride)),
                 string(address(word(address(base)+i*stride+4))))
                for i in range(count) if word(address(base)+i*stride)]
    characters = {i: (header, name) for i, header, name in modeltable(b'CHRM')}
    selected = [(i, *characters[i]) for i in (5, 22, 23, 24, 25)]
    bank = entries[b'NPMD'][0]
    for i in range(word(bank+4)):
        a = bank+16+i*96
        if word(a+84) == 2:
            name = string(a)
            if name in ('CheadmooreZ', 'CheadconneryZ', 'CheaddaltonZ'):
                files[name] = (bank+word(a+64), word(a+68))
                selected.append((word(a+92), characters[word(a+88)][0], name))
    # Frigate begins with this held model; player-buffer tests reserve it too.
    selected.append(next((128, h, n) for i, h, n in modeltable(b'PROP') if n == 'Pchrmp5kZ'))
    output = bytearray(struct.pack('>I', len(selected)))
    for index, header, name in selected:
        at, size = files[name]
        switches, textures = half(header+12), half(header+22)
        root = switches*4+textures*12
        todo, seen, lists = [root], set(), []
        while todo:
            node = todo.pop()
            if node in seen: continue
            assert node+24 <= size
            seen.add(node)
            opcode = half(at+node)&255
            ro = word(at+node+4)&0xffffff
            for delta in (12, 20):
                p = word(at+node+delta)
                if p: todo.append(p&0xffffff)
            for delta in {8:(8,), 9:(24,28), 18:(0,)}.get(opcode, ()):
                p = word(at+ro+delta)
                if p: todo.append(p&0xffffff)
            for delta in {4:(0,4), 0x18:(0,4), 0x16:(8,)}.get(opcode, ()):
                p = word(at+ro+delta)
                if p: lists.append(p&0xffffff)
        assert lists and min(lists)%8 == 0 and size%4 == 0
        output += struct.pack('>5I', index, switches, textures, size, min(lists))
        output += data[at:at+size]
    # N64 texRawAllocationBytes, including generated mipmaps and 24 bytes of
    # descriptor/prefix storage. Pixel decoding is covered by texture_pool.
    images = entries[b'IMGS'][0]
    def levelbytes(fmt, width, height):
        if fmt in (0,2): return ((width+3)&~3)*height*4
        if fmt in (1,3,4): return ((width+3)&~3)*height*2
        if fmt in (5,7,9,11): return ((width+7)&~7)*height
        return ((width+15)&~15)*height//2
    count = word(entries[b'TXCF'][0]+4)
    output += struct.pack('>I', count)
    for i in range(count):
        assert data[images:images+4] == b'GUTX'
        palette = half(images+8)
        total = sum(word(images+24+12*j) for j in range(data[images+6]))
        fmt, width, height = data[images+16:images+19]
        if not data[images+4] and data[images+6] == 1:
            for lod in range(1, data[images+5]):
                width, height = (width+1)//2, (height+1)//2
                size = levelbytes(fmt, width, height)
                if palette and total+size > 0x800: break
                total += size
        output += struct.pack('>I', ((total+palette*2+7)&~7)+24)
        images += word(images+12)
    target.write_bytes(output)
