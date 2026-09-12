#ifndef _N64_DIAGNOSTICS_H_
#define _N64_DIAGNOSTICS_H_

#include <ultra64.h>

/* Temporary hardware investigation. Set to 0 after diagnosing the failure.
 * Makefile dependencies rebuild every consumer when this header changes. */
#ifndef N64_LOAD_DIAGNOSTICS
#define N64_LOAD_DIAGNOSTICS 1
#endif

/* 11A passed on hardware after aligning the color framebuffers. Restore
 * normal rendering by default (12F), retaining the watchdog and preflight.
 * Keep these isolation switches available until full rendering is verified. */

/* Temporary A/B test: 1 replaces clouds/water with the solid sky path.
 * Set to 0 and rebuild to restore normal rendering with diagnostics retained.
 * This is an isolation test, not a fix for the hardware stall. */
#ifndef N64_DIAG_SOLID_SKY
#define N64_DIAG_SOLID_SKY 0
#endif

/* Next isolation test: load rooms normally but omit their primary/secondary
 * display-list calls. Props, characters and HUD still render. Set to 0 to
 * restore room drawing. This is temporary and is independent of DEBUG. */
#ifndef N64_DIAG_SKIP_BG_GDLS
#define N64_DIAG_SKIP_BG_GDLS 0
#endif

/* Replace gameplay submissions with known fill-only commands: blue short
 * frames, then amber FIFO-wrap frames, then green continuing the wrap test.
 * Menus and the CPU's stage work remain active. Temporary, not a fix. */
#ifndef N64_DIAG_RDP_PROBE
#define N64_DIAG_RDP_PROBE 0
#endif

/* Next isolation test after 05P passed on hardware: submit the ordinary
 * frame setup and HUD, branching past world/effects and gun/watch drawing.
 * Their CPU functions still run, including render-time gameplay updates.
 * N64_DIAG_RESTORE_WEAPONS can re-enable the weapon/watch range separately.
 * The fill-only probe takes precedence when re-enabled. Temporary, not a fix. */
#ifndef N64_DIAG_HUD_ONLY
#define N64_DIAG_HUD_ONLY 0
#endif

/* After 06H passed: keep the world/effects bypass but draw casings, the
 * first-person weapon and the watch. Only applies to the HUD isolation mode.
 * Set to 0 to repeat the successful 06H control. Temporary, not a fix. */
#ifndef N64_DIAG_RESTORE_WEAPONS
#define N64_DIAG_RESTORE_WEAPONS 0
#endif

/* 07W draws weapons/casings but stalls on the watch controller screen.
 * Keep its model, textures and animations; use the model renderer's non-Z
 * render modes for the controller only. Applies only to the 07W isolation
 * configuration. Set to 0 to repeat 07W. Temporary test, not a fix. */
#ifndef N64_DIAG_CONTROLLER_NO_ZBUFFER
#define N64_DIAG_CONTROLLER_NO_ZBUFFER 0
#endif

/* 08Z passed on hardware. Re-enable the controller's normal Z-enabled
 * render modes, but feed the RDP a constant primitive Z/deltaZ instead of
 * interpolated triangle depth. Overrides CONTROLLER_NO_ZBUFFER in the same
 * isolation configuration. Set to 0 to repeat 08Z. Temporary test, not a fix. */
#ifndef N64_DIAG_CONTROLLER_PRIMITIVE_Z
#define N64_DIAG_CONTROLLER_PRIMITIVE_Z 0
#endif

/* 09P still stalls. Clear only Z_CMP in the controller's emitted render
 * modes, preserving Z_UPD and the primitive-depth test. Applies only with
 * PRIMITIVE_Z in the weapon/watch isolation setup. Set to 0 for 09P. */
#ifndef N64_DIAG_CONTROLLER_NO_ZCOMPARE
#define N64_DIAG_CONTROLLER_NO_ZCOMPARE 0
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
void n64DiagTask(u32 type, void *ucode, void *list, u32 size, void *out, void *end);
void n64DiagCheckGfx(void *list, u32 size);
#else
#define n64DiagInit() ((void)0)
#define n64DiagStage(stage) ((void)0)
#define n64DiagStep(phase) ((void)0)
#define n64DiagDmaStart(target, source, size) ((void)0)
#define n64DiagDmaResult(result) ((void)0)
#define n64DiagDmaDone() ((void)0)
#define n64DiagAllocation(bank, bytes, pos, end) ((void)0)
#define n64DiagFault(thread) ((void)0)
#define n64DiagTask(type, ucode, list, size, out, end) ((void)0)
#define n64DiagCheckGfx(list, size) ((void)0)
#endif

#endif
