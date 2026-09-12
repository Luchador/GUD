#include <assert.h>
#include <string.h>
#include "n64rdpcheck.h"

static u32 ram[0x3000 / 4];
static u32 next;
static N64RdpCheck result;

static s32 read_command(u32 address, u32 *a, u32 *b)
{
    assert(!(address & 7));
    if (address < 0x400 || address > sizeof(ram) - 8) return 0;
    *a = ram[address / 4];
    *b = ram[address / 4 + 1];
    return 1;
}

static void emit(u32 a, u32 b)
{
    ram[next / 4] = a;
    ram[next / 4 + 1] = b;
    next += 8;
}

static void begin(void)
{
    memset(ram, 0, sizeof(ram));
    next = 0x1000;
    emit(0xbc000006, 0); /* physical segment */
}

static void run(u32 status, const char *reason)
{
    emit(0xb8000000, 0);
    n64RdpCheckList(0x1000, next - 0x1000, sizeof(ram), read_command, &result);
    assert(result.status == status);
    if (reason) assert(strcmp(result.reason, reason) == 0);
}

void test_rdp_preflight(void)
{
    u32 i, rootEnd;
    const u32 badFill[] = {0x40, 0x10, 0x20};
    const char *fillReason[] = {"FILL WITH IMAGE READ", "FILL WITH Z COMPARE", "FILL WITH PIXEL Z UPDATE"};

    /* 10R's actual framebuffer address: valid RDRAM, but only 16-byte
     * aligned. This must be caught even before a Z-enabled draw appears. */
    begin(); emit(0xff10013f, 0x803da150);
    run(N64RDP_HAZARD, "COLOR IMAGE NOT 64 BYTE ALIGNED");
    assert(result.address == 0x1008 && result.word1 == 0x803da150);
    begin(); emit(0xff10013f, 0x803da180);
    run(N64RDP_CHECKED, NULL);
    begin(); emit(0xfe000000, 0x8023e550);
    run(N64RDP_HAZARD, "DEPTH IMAGE NOT 64 BYTE ALIGNED");
    begin(); emit(0xfe000000, 0x8023e540);
    run(N64RDP_CHECKED, NULL);
    /* Validate the resolved address, including alignment contributed by a
     * segment base. Unknown segments remain unknown, not false hazards. */
    begin(); emit(0xbc000406, 0x1810); emit(0xff10013f, 0x01000000);
    run(N64RDP_HAZARD, "COLOR IMAGE NOT 64 BYTE ALIGNED");
    begin(); emit(0xbc000406, 0x1800); emit(0xfe000000, 0x01000010);
    run(N64RDP_HAZARD, "DEPTH IMAGE NOT 64 BYTE ALIGNED");
    begin(); emit(0xff10013f, 0x01000010);
    run(N64RDP_CHECKED, NULL);

    /* The 10R edit is exactly one bit, including the actual captured 07W
     * and 09P modes. Never change Z_UPD, primitive source, blending, or a
     * different command whose payload happens to contain bit 4. */
    assert(n64RdpWithoutDepthCompare(0xb900031d, 0xc41049d8) == 0xc41049c8);
    assert(n64RdpWithoutDepthCompare(0xb900031d, 0x0c19287c) == 0x0c19286c);
    assert(n64RdpWithoutDepthCompare(0xef99ec2f, 0x0c19287c) == 0x0c19286c);
    assert(n64RdpWithoutDepthCompare(0xb9000201, 4) == 4);
    assert(n64RdpWithoutDepthCompare(0xb1003322, 0x45040310) == 0x45040310);
    assert(n64RdpWithoutDepthCompare(0xfe000000, 0x8023e250) == 0x8023e250);
    assert(n64RdpWithoutDepthCompare(0xb900031d, 0x0c19286c) == 0x0c19286c);

    /* Snapshot depth state at the draw, following segmented/cached image
     * addresses. Later non-Z HUD state must not erase the useful values. */
    begin();
    emit(0xbc000406, 0x1800);
    emit(0xfe000000, 0x01000040);
    emit(0xff10013f, 0x80002000);
    emit(0xef100000, 0x0c19287c);
    emit(0xb1003322, 0x45040310);
    emit(0xb900031d, n64RdpWithoutDepthCompare(0xb900031d, 0x0c192878));
    emit(0xb1003322, 0x45040310);
    emit(0xb900031d, 0);
    emit(0xff10003f, 0x2800);
    emit(0xfe000000, 0x2c00);
    emit(0xb1000000, 0);
    run(N64RDP_CHECKED, NULL);
    assert(result.depthCompareDraws == 1 && result.depthWriteDraws == 2);
    assert(result.depthImage == 0x1840 && result.depthColor == 0x2000);
    assert(result.depthWidth == 320 && result.depthMode == 0x0c19286c);
    assert(result.depthCommand == 0x1010 && result.depthDraw == 0x1038);

    begin(); emit(0xef100000, 0x34); emit(0xb1000000, 0);
    run(N64RDP_CHECKED, NULL);
    assert(result.depthImage == 0xffffffff && result.depthWidth == 0xffffffff);
    begin(); emit(0xef100000, 0); emit(0xb1000000, 0);
    run(N64RDP_CHECKED, NULL);
    assert(!result.depthCompareDraws && !result.depthWriteDraws);
    assert(result.depthDraw == 0xffffffff && result.depthImage == 0xffffffff);

    /* The game's normal Z-clear sequence: fill into RGBA16, NOOP render mode. */
    begin();
    emit(0xff10013f, 0x2000);
    emit(0xb900031d, 0);
    emit(0xba001402, 0x300000);
    emit(0xf64fc3bc, 0);
    emit(0xba001402, 0x100000);
    emit(0xb900031d, 0xc8102878);
    emit(0xb1000321, 0x21436587);
    run(N64RDP_CHECKED, NULL);

    for (i = 0; i < 3; i++) {
        begin(); emit(0xef300000, badFill[i]); emit(0xf6000000, 0);
        run(N64RDP_HAZARD, fillReason[i]);
        assert(result.address == 0x1010 && result.word0 == 0xf6000000);
    }
    begin(); emit(0xef300000, 0x24); emit(0xf6000000, 0);
    run(N64RDP_CHECKED, NULL); /* primitive Z is allowed */
    begin(); emit(0xef300000, 0); emit(0xff00013f, 0x2000); emit(0xf6000000, 0);
    run(N64RDP_HAZARD, "FILL ON 4 BIT FRAMEBUFFER");
    begin(); emit(0xef200000, 0); emit(0xff18013f, 0x2000); emit(0xe4000000, 0);
    run(N64RDP_HAZARD, "COPY ON 32 BIT FRAMEBUFFER");
    begin(); emit(0xef200000, 0); emit(0xed004000, 0x00500300); emit(0xe4000000, 0);
    run(N64RDP_HAZARD, "COPY WITH LEFT SCISSOR");

    /* A real rectangle consumes two half words even if its S/T payload looks
     * like a raw triangle command. */
    begin(); emit(0xef000000, 0); emit(0xe4000000, 0);
    emit(0xb4000000, 0xce000000); emit(0xb3000000, 0x04000400);
    run(N64RDP_CHECKED, NULL);
    begin(); emit(0xe4000000, 0); emit(0xe7000000, 0);
    run(N64RDP_HAZARD, "BROKEN TEXTURE RECTANGLE");

    /* Complete sky SHADE_TXTR triangle: 8 edge + 16 shade + 16 texture words. */
    begin(); emit(0xb4000000, 0xce000100);
    for (i = 1; i < 40; i++) emit(i == 39 ? 0xb3000000 : 0xb2000000, i);
    run(N64RDP_CHECKED, NULL);
    begin(); emit(0xb4000000, 0xce000100); emit(0xb3000000, 0);
    run(N64RDP_HAZARD, "WRONG RAW TRIANGLE LENGTH");
    begin(); emit(0xb4000000, 0xce000100);
    run(N64RDP_HAZARD, "UNTERMINATED RAW TRIANGLE");

    begin(); emit(0xfd000000, 0x2000); emit(0xf4000000, 0x070fc0fc);
    run(N64RDP_HAZARD, "4 BIT TILE OR PALETTE LOAD");
    begin(); emit(0xfd100000, 0x2000); emit(0xf0008000, 0x07004000);
    run(N64RDP_HAZARD, "REVERSED PALETTE LOAD");
    begin(); emit(0xfd100000, 0x2004); emit(0xf3000000, 0x0703f000);
    run(N64RDP_HAZARD, "MISALIGNED TEXTURE LOAD");
    begin(); emit(0xfd100000, 0x2008); emit(0xf3000000, 0x0703f000);
    run(N64RDP_CHECKED, NULL);
    begin(); emit(0xfd000000, 0x2004); emit(0xf3000000, 0x0703f000);
    run(N64RDP_CHECKED, NULL); /* load-block 4b exception */

    /* Follow pushed and branch lists with segment translation, retain shared
     * render state and report the calling GBI address. */
    begin(); emit(0xbc000406, 0x1800); emit(0x06000000, 0x01000000);
    emit(0xb8000000, 0); rootEnd = next;
    next = 0x1800; emit(0x06010000, 0x01000040);
    next = 0x1840; emit(0xef300000, 0x40); emit(0xf6000000, 0); emit(0xb8000000, 0);
    n64RdpCheckList(0x1000, rootEnd - 0x1000, sizeof(ram), read_command, &result);
    assert(result.status == N64RDP_HAZARD && result.caller == 0x1010 && result.address == 0x1848);
    ram[0x1844 / 4] = 0;
    n64RdpCheckList(0x1000, rootEnd - 0x1000, sizeof(ram), read_command, &result);
    assert(result.status == N64RDP_CHECKED);

    begin(); emit(0x04f00100, 0x2004);
    run(N64RDP_HAZARD, "INVALID RSP DMA ADDRESS");
    begin(); emit(0x01000040, 0x2fe0);
    run(N64RDP_HAZARD, "INVALID RSP DMA ADDRESS");
    begin(); emit(0x06000000, 0x3000);
    run(N64RDP_HAZARD, "INVALID DISPLAY LIST ADDRESS");
    begin(); emit(0xba001f02, 0);
    run(N64RDP_HAZARD, "INVALID OTHERMODE MASK");
    begin(); emit(0xbe000000, 0);
    run(N64RDP_PARTIAL, "CONDITIONAL CULL LIST");
    begin(); emit(0x06000000, 0x01000000);
    run(N64RDP_PARTIAL, "UNKNOWN DL SEGMENT");
    begin(); emit(0x06010000, 0x1000);
    run(N64RDP_PARTIAL, "COMMAND LIMIT");
    assert(result.commands == 200000);
    begin();
    n64RdpCheckList(0x1000, 8, sizeof(ram), read_command, &result);
    assert(result.status == N64RDP_HAZARD && !strcmp(result.reason, "MASTER LIST WITHOUT END"));
}
