#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
typedef uint8_t u8;
typedef int8_t s8;
typedef uint16_t u16;
typedef int16_t s16;
typedef uint32_t u32;
typedef int32_t s32;
typedef unsigned long long u64;
typedef long long s64;
typedef float f32;
typedef double f64;
typedef s32 bool;
#define TRUE 1
#define FALSE 0
#define _LANGUAGE_C 1
#define _SHIFTL(v, s, w) ((u32)(((u32)(v) & (0xffffffffu >> (32 - (w)))) << (s)))
#include <PR/gbi.h>
#include <PR/rcp.h>
#undef IO_READ
#define IO_READ(reg) testReadRegister(reg)
#define OS_USEC_TO_CYCLES(us) (((u64)(us) * 46875000) / 1000000)
#define OS_CYCLES_TO_USEC(ticks) (((u64)(ticks) * 1000000) / 46875000)
#define OS_IM_NONE 0
#define LEVELID_TITLE 90
#define PLAYER_1 0
#define U_CBUTTONS 0x8
#define D_CBUTTONS 0x4
#define U_JPAD 0x800
#define D_JPAD 0x400
#define L_CBUTTONS 0x2
#define R_CBUTTONS 0x1
#define L_TRIG 0x20
#define R_TRIG 0x10
#define L_JPAD 0x200
#define R_JPAD 0x100
#define XOFFSET_1 64
typedef u32 OSIntMask;
typedef struct OSViMode { struct { u32 ctrl; } comRegs; } OSViMode;
typedef struct OSTask { struct { u32 type; void *output_buff; u64 *output_buff_size; } t; } OSTask;
typedef struct OSScTask { OSTask list; u32 state; } OSScTask;
typedef struct OSSched { OSScTask *curRSPTask, *curRDPTask; } OSSched;
#define OS_SC_YIELD 0x10
#define OS_SC_YIELDED 0x20
#define M_AUDTASK 2
#define M_GFXTASK 1
static u32 g_TestCounterResets;
static void osDpSetStatus(u32 flags) { (void)flags; g_TestCounterResets++; }
static void osWritebackDCacheAll(void) {}
static void osSpTaskLoad(OSTask *task) { (void)task; }
static void osSpTaskStartGo(OSTask *task) { (void)task; }
static int osDpSetNextBuffer(void *buffer, u64 size) { (void)buffer; (void)size; return 0; }
struct player { s32 pause_state; };
static struct player g_TestPlayers[4];
struct player *g_playerPointers[4] = {&g_TestPlayers[0], &g_TestPlayers[1], &g_TestPlayers[2], &g_TestPlayers[3]};
static u8 g_TestRam[1024 * 1024];
u32 osMemSize = sizeof(g_TestRam);
u8 *g_GfxBuffers[3] = {g_TestRam, g_TestRam + 0x1000, g_TestRam + 0x2000};
u8 *g_VtxBuffers[3] = {g_TestRam + 0x2000, g_TestRam + 0x3000, g_TestRam + 0x4000};
static u32 g_TestClock;
static u32 g_TestCounters[3];
static s32 g_TestStage = 1;
static s32 g_TestPlayerCount = 1;
static bool g_TestBg = TRUE;
static bool g_TestLocked;
static bool g_TestPaused;
static u32 g_TestButtons;
static s32 g_TestStick;
static s32 g_TestActive;
static u32 osGetCount(void) { return g_TestClock; }
static OSIntMask osSetIntMask(OSIntMask mask) { (void)mask; return 1; }
static s32 bossGetStageNum(void) { return g_TestStage; }
static s32 getPlayerCount(void) { return g_TestPlayerCount; }
static bool lvGetBgRenderEnabled(void) { return g_TestBg; }
static bool lvGetControlsLockedFlag(void) { return g_TestLocked; }
static bool checkGamePaused(void) { return g_TestPaused; }
static u32 testReadRegister(u32 reg) {
    switch (reg) {
    case DPC_BUFBUSY_REG: return g_TestCounters[0];
    case DPC_PIPEBUSY_REG: return g_TestCounters[1];
    case DPC_TMEM_REG: return g_TestCounters[2];
    default: assert(0); return 0;
    }
}
static u32 joyGetButtonsPressedThisFrame(s32 player, u32 mask) { (void)player; return g_TestButtons & mask; }
static bool watchShouldNavUp(void) { return g_TestStick == 1; }
static bool watchShouldNavDown(void) { return g_TestStick == -1; }
static void watchSetStickYDisabled(void) { g_TestStick = 0; }
static void watchResetItemIsActivelySelected(void) { g_TestActive = 0; }

static bool watchShouldNavLeft(void) { return g_TestStick == -2; }
static bool watchShouldNavRight(void) { return g_TestStick == 2; }
static void watchSelectGameOption(u32 *value, u32 chosen) { *value = chosen; g_TestStick = 0; }
static Gfx *draw_options_labels(Gfx *gdl, s32 x, s32 y, char *text, u32 colour, s32 outlined,
        u32 outlinecolour, s32 centre, s32 drawbg, u32 bgcolour, s32 rightalign)
{
    (void)text; (void)colour; (void)outlined; (void)outlinecolour; (void)centre;
    (void)drawbg; (void)bgcolour; (void)rightalign;
    assert(x >= 0 && x < 320 && y >= 0 && y < 240);
    return gdl;
}
