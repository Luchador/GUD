#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../../src/n64rdpprobe.c"
#include "../../../src/n64rdpcheck.h"

static u32 physical(const void *p) { return (u32)(uintptr_t)p & 0x1fffffff; }

static s32 read_gbi(u32 address, u32 *w0, u32 *w1)
{
    Gfx *arrays[] = {g_ProbeBlock, g_ProbeLists[0], g_ProbeLists[1]};
    u32 lengths[] = {sizeof(g_ProbeBlock), sizeof(g_ProbeLists[0]), sizeof(g_ProbeLists[1])};
    unsigned i;
    for (i = 0; i < 3; i++) {
        u32 base = physical(arrays[i]);
        if (address >= base && address - base <= lengths[i] - 8 && !(address & 7)) {
            Gfx *g = arrays[i] + (address - base) / 8;
            *w0 = g->words.w0;
            *w1 = g->words.w1;
            return 1;
        }
    }
    return 0;
}

/* Walk actual emitted commands, including calls. The deliberately tiny
 * allow-list rejects textures, triangles and any unexpected state changes. */
static void inspect(Gfx *list, u32 size, int wrap, u32 colour)
{
    N64RdpCheck result;
    u32 pc = physical(list), returnpc = 0, w0, w1, op;
    unsigned fills = 0, syncs = 0, fullsyncs = 0, calls = 0, colours = 0;
    unsigned rawbytes = 0, steps = 0;
    int framebuf = 0;

    assert(sizeof(Gfx) == 8);
    assert(size == (wrap ? 0x138 : 0x78));
    assert(size + 0x140 <= sizeof(g_ProbeLists[0]));
    n64RdpCheckList(pc, size, 0x800000, read_gbi, &result);
    if (result.status != N64RDP_CHECKED) {
        fprintf(stderr, "Probe preflight: %s at %08x (%08x %08x), list %08x size %x\n",
                result.reason, result.address, result.word0, result.word1, pc, size);
    }
    assert(result.status == N64RDP_CHECKED);
    for (;;) {
        assert(++steps < 10000 && read_gbi(pc, &w0, &w1));
        op = w0 >> 24;
        pc += 8;
        if (op == 0xb8) {
            if (returnpc) { pc = returnpc; returnpc = 0; continue; }
            assert(pc == physical(list) + size);
            break;
        }
        if (op == 0x06) {
            assert(!returnpc && w0 == 0x06000000 && w1 == physical(g_ProbeBlock));
            returnpc = pc;
            pc = w1;
            calls++;
            continue;
        }
        if (op >= 0xe6) rawbytes += 8;
        switch (op) {
        case 0xbc: assert(w0 == 0xbc000006 && w1 == 0); break;
        case 0xe7: syncs++; break;
        case 0xe9: fullsyncs++; assert(pc == physical(list) + size - 8); break;
        case 0xff: assert(w0 == 0xff10013f && w1 == 0x700000); framebuf++; break;
        case 0xed: assert(w0 == 0xed000000 && w1 == 0x005003c0); break;
        case 0xf7: assert(w1 == (colours++ ? 0xffffffff : colour)); break;
        case 0xf6:
            assert(framebuf);
            assert(((w1 >> 12) & 0xfff) % 16 == 0); /* fill's left X aligned to 4 pixels */
            assert(((w0 >> 12) & 0xfff) < 320 * 4 && (w0 & 0xfff) < 240 * 4);
            assert(((w1 >> 12) & 0xfff) <= ((w0 >> 12) & 0xfff));
            assert((w1 & 0xfff) <= (w0 & 0xfff));
            fills++;
            break;
        case 0xba: case 0xb9: break; /* validated by production preflight */
        default: assert(!"unexpected probe command");
        }
    }
    assert(fullsyncs == 1 && colours == 2 && framebuf == 1);
    assert(calls == (wrap ? 24U : 0U));
    assert(fills == (wrap ? 3074U : 2U));
    assert(syncs == (wrap ? 3075U : 3U));
    assert(wrap ? rawbytes > 0xa000 : rawbytes < 0xa000);
}

int main(void)
{
    u32 frames[] = {0, 119, 120, 239, 240, 0xffffffff};
    u32 colours[] = {0x085f085f, 0x085f085f, 0xfd01fd01, 0xfd01fd01, 0x07c107c1, 0x07c107c1};
    Gfx saved[PROBE_LIST_COMMANDS + PROBE_READ_AHEAD];
    Gfx *list;
    u32 size;
    unsigned i;
    for (i = 0; i < sizeof(frames) / sizeof(frames[0]); i++) {
        list = n64RdpProbeBuild(0, (void *)(uintptr_t)0x80700000, frames[i], &size);
        inspect(list, size, frames[i] >= 120, colours[i]);
        memcpy(saved, g_ProbeLists[0], sizeof(saved));
        list = n64RdpProbeBuild(1, (void *)(uintptr_t)0x80700000, 240, &size);
        inspect(list, size, 1, 0x07c107c1);
        assert(!memcmp(saved, g_ProbeLists[0], sizeof(saved)));
    }
    puts("RDP probe phases, command safety, forced FIFO wrap, read-ahead space and separate task buffers passed");
    return 0;
}
