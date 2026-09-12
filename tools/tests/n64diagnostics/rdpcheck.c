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
