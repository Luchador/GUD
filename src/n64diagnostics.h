#ifndef _N64_DIAGNOSTICS_H_
#define _N64_DIAGNOSTICS_H_

#include <ultra64.h>

/* Temporary hardware investigation. Set to 0 after diagnosing the failure.
 * Makefile dependencies rebuild every consumer when this header changes. */
#ifndef N64_LOAD_DIAGNOSTICS
#define N64_LOAD_DIAGNOSTICS 1
#endif

enum N64DiagPhase {
    N64DIAG_BOOT,
    N64DIAG_STAGE_BEGIN,
    N64DIAG_POOLS,
    N64DIAG_PLAYERS,
    N64DIAG_GFX_MEMORY,
    N64DIAG_STAGE_LOAD,
    N64DIAG_TEXTURES,
    N64DIAG_FONTS,
    N64DIAG_BG_HEADER,
    N64DIAG_BG_DATA,
    N64DIAG_STAN,
    N64DIAG_BG_PROCESS,
    N64DIAG_STAGE_STATE,
    N64DIAG_GUARDS,
    N64DIAG_SETUP,
    N64DIAG_FX,
    N64DIAG_PATHS,
    N64DIAG_BOND,
    N64DIAG_INTRO,
    N64DIAG_VIDEO,
    N64DIAG_WAIT_FRAME,
    N64DIAG_TICK,
    N64DIAG_VIEW,
    N64DIAG_RENDER,
    N64DIAG_SUBMIT,
    N64DIAG_UNLOAD,
    N64DIAG_PHASE_COUNT
};

#if N64_LOAD_DIAGNOSTICS
void n64DiagInit(void);
void n64DiagStage(s32 stage);
void n64DiagStep(enum N64DiagPhase phase);
void n64DiagDmaStart(void *target, void *source, u32 size);
void n64DiagDmaResult(s32 result);
void n64DiagDmaDone(void);
void n64DiagAllocation(u32 bank, u32 bytes, void *pos, void *end);
void n64DiagFault(OSThread *thread);
#else
#define n64DiagInit() ((void)0)
#define n64DiagStage(stage) ((void)0)
#define n64DiagStep(phase) ((void)0)
#define n64DiagDmaStart(target, source, size) ((void)0)
#define n64DiagDmaResult(result) ((void)0)
#define n64DiagDmaDone() ((void)0)
#define n64DiagAllocation(bank, bytes, pos, end) ((void)0)
#define n64DiagFault(thread) ((void)0)
#endif

#endif
