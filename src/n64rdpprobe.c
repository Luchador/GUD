#include "n64rdpprobe.h"

#if N64_LOAD_DIAGNOSTICS && N64_DIAG_RDP_PROBE

#define PROBE_SHORT_FRAMES 120
#define PROBE_GREEN_FRAME 240
#define PROBE_BLOCK_PAIRS 128
#define PROBE_BLOCK_CALLS 24
#define PROBE_LIST_COMMANDS 64
/* The microcode reads ahead in 0x140-byte input batches. */
#define PROBE_READ_AHEAD 40

static Gfx g_ProbeBlock[PROBE_BLOCK_PAIRS * 2 + 1 + PROBE_READ_AHEAD];
static Gfx g_ProbeLists[2][PROBE_LIST_COMMANDS + PROBE_READ_AHEAD];
static s32 g_ProbeBlockReady;

Gfx *n64RdpProbeBuild(u32 slot, void *framebuffer, u32 frame, u32 *size)
{
    Gfx *first;
    Gfx *gdl;
    u32 colour;
    u32 marker;
    s32 i;

    if (!g_ProbeBlockReady) {
        gdl = g_ProbeBlock;
        for (i = 0; i < PROBE_BLOCK_PAIRS; i++) {
            gDPPipeSync(gdl++);
            gDPFillRectangle(gdl++, 16, 16, 19, 19);
        }
        gSPEndDisplayList(gdl++);
        g_ProbeBlockReady = TRUE;
    }

    first = g_ProbeLists[slot & 1];
    gdl = first;
    colour = frame < PROBE_SHORT_FRAMES ? 0x085f085f
        : frame < PROBE_GREEN_FRAME ? 0xfd01fd01 : 0x07c107c1;
    marker = 16 + (frame & 63) * 4;

    /* Use the normal other-mode GBI commands so the microcode's saved state
     * remains correct if an audio task yields and resumes this graphics task.
     * FILL has no image reads, Z comparisons, Z writes or texture loads. */
    gDPPipeSync(gdl++);
    gSPSegment(gdl++, 0, 0);
    gDPSetRenderMode(gdl++, G_RM_NOOP, G_RM_NOOP2);
    gDPSetCycleType(gdl++, G_CYC_FILL);
    gDPSetDepthSource(gdl++, G_ZS_PRIM);
    gDPSetColorImage(gdl++, G_IM_FMT_RGBA, G_IM_SIZ_16b, 320,
            (u32)framebuffer & 0x1fffffff);
    gDPSetScissor(gdl++, G_SC_NON_INTERLACE, 0, 0, 320, 240);
    gDPSetFillColor(gdl++, colour);
    gDPFillRectangle(gdl++, 0, 0, 319, 239);
    gDPPipeSync(gdl++);
    gDPSetFillColor(gdl++, 0xffffffff);

    if (frame >= PROBE_SHORT_FRAMES) {
        /* 24 * 128 * (sync + fill) * 8 = 49,152 RDP bytes. This exceeds the
         * existing 40 KiB FIFO in one task without using scene assets. Calls
         * return before the next call, so display-list stack depth is one. */
        for (i = 0; i < PROBE_BLOCK_CALLS; i++) {
            gSPDisplayList(gdl++, (u32)g_ProbeBlock & 0x1fffffff);
        }
    }

    gDPPipeSync(gdl++);
    gDPFillRectangle(gdl++, marker, 208, marker + 3, 215);
    gDPFullSync(gdl++);
    gSPEndDisplayList(gdl++);
    *size = (u8 *)gdl - (u8 *)first;
    return first;
}

#endif
