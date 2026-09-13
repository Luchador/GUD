#include <ultra64.h>
#include <PR/rcp.h>
#include <bondconstants.h>
#include <boss.h>
#include "bondview.h"
#include "lv.h"
#include "mpmenu.h"
#include "player.h"
#include "renderconfig.h"
#include "renderprofile.h"
#include "rsp.h"

#define RENDER_PROFILE_WINDOW 60
#define RENDER_PROFILE_WARMUP 30
/* All DP counters are 24-bit. Reject long tasks rather than report wrapped
 * counts as fast frames. 250 ms is below the 268 ms wrap at 62.5 MHz. */
#define RENDER_PROFILE_MAX_TASK_US 250000

static bool g_RenderProfileEnabled = FALSE;
static bool g_RenderProfileWasActive = FALSE;
static s32 g_RenderProfileStage = -1;
static u8 g_RenderProfileSettings;
static u32 g_RenderProfileEpoch = 1;
static RenderProfileStats g_RenderProfileStats;
static u32 g_RenderProfileSums[RENDER_PROFILE_METRICS];
static u32 g_RenderProfileMax[RENDER_PROFILE_METRICS];

static void renderProfileReset(void)
{
    s32 i;
    g_RenderProfileEpoch++;
    for (i = 0; i < RENDER_PROFILE_METRICS; i++) {
        g_RenderProfileSums[i] = 0;
        g_RenderProfileMax[i] = 0;
        g_RenderProfileStats.average[i] = 0;
        g_RenderProfileStats.maximum[i] = 0;
    }
    g_RenderProfileStats.samples = 0;
    g_RenderProfileStats.collected = 0;
    g_RenderProfileStats.warming = RENDER_PROFILE_WARMUP;
    g_RenderProfileStats.rejected = 0;
    g_RenderProfileStats.aaError = FALSE;
}

bool renderProfileEnabled(void) { return g_RenderProfileEnabled; }

void renderProfileSetEnabled(bool enabled)
{
    OSIntMask mask = osSetIntMask(OS_IM_NONE);
    g_RenderProfileEnabled = enabled != FALSE;
    g_RenderProfileWasActive = FALSE;
    renderProfileReset();
    osSetIntMask(mask);
}

bool renderProfileGameplayActive(void)
{
    s32 i;
    if (bossGetStageNum() == LEVELID_TITLE || !lvGetBgRenderEnabled() || lvGetControlsLockedFlag()
            || checkGamePaused()) return FALSE;
    for (i = 0; i < getPlayerCount(); i++) {
        if (g_playerPointers[i]->pause_state != 0) return FALSE;
    }
    return getPlayerCount() > 0;
}

void renderProfilePrepareTask(OSScTask *task, u32 aaCpuCount, bool aaOk)
{
    RenderProfileTask *sample = &((struct GfxInfo_s *)task)->renderProfile;
    bool active = g_RenderProfileEnabled && renderProfileGameplayActive() && !renderSettingsPending();
    u8 settings = renderEncodeSettings();
    OSIntMask mask = osSetIntMask(OS_IM_NONE);
    if (active != g_RenderProfileWasActive || g_RenderProfileStage != bossGetStageNum()
            || g_RenderProfileSettings != settings) {
        renderProfileReset();
        g_RenderProfileWasActive = active;
        g_RenderProfileStage = bossGetStageNum();
        g_RenderProfileSettings = settings;
    }
    g_RenderProfileStats.aaError = !aaOk;
    sample->epoch = g_RenderProfileEpoch;
    sample->aaCpuCount = aaCpuCount;
    sample->eligible = active && aaOk;
    osSetIntMask(mask);
}

void renderProfileTaskStart(OSScTask *task)
{
    /* Called once, before the first RSP start; never on audio-yield resumes. */
    ((struct GfxInfo_s *)task)->renderProfile.startCount = osGetCount();
}

void renderProfileTaskDone(OSScTask *task)
{
    RenderProfileTask *sample = &((struct GfxInfo_s *)task)->renderProfile;
    u32 values[RENDER_PROFILE_METRICS];
    s32 i;
    if (!sample->eligible || sample->epoch != g_RenderProfileEpoch || !g_RenderProfileEnabled) return;

    values[RENDER_PROFILE_GFX] = osGetCount() - sample->startCount;
    values[RENDER_PROFILE_CMD] = IO_READ(DPC_BUFBUSY_REG) & 0xffffff;
    values[RENDER_PROFILE_PIPE] = IO_READ(DPC_PIPEBUSY_REG) & 0xffffff;
    values[RENDER_PROFILE_TMEM] = IO_READ(DPC_TMEM_REG) & 0xffffff;
    values[RENDER_PROFILE_AA_CPU] = sample->aaCpuCount;
    if (values[RENDER_PROFILE_GFX] >= OS_USEC_TO_CYCLES(RENDER_PROFILE_MAX_TASK_US)) {
        g_RenderProfileStats.rejected++;
        return;
    }
    if (g_RenderProfileStats.warming) {
        g_RenderProfileStats.warming--;
        return;
    }
    for (i = 0; i < RENDER_PROFILE_METRICS; i++) {
        g_RenderProfileSums[i] += values[i];
        if (values[i] > g_RenderProfileMax[i]) g_RenderProfileMax[i] = values[i];
    }
    if (++g_RenderProfileStats.collected == RENDER_PROFILE_WINDOW) {
        for (i = 0; i < RENDER_PROFILE_METRICS; i++) {
            g_RenderProfileStats.average[i] = g_RenderProfileSums[i] / RENDER_PROFILE_WINDOW;
            g_RenderProfileStats.maximum[i] = g_RenderProfileMax[i];
            g_RenderProfileSums[i] = 0;
            g_RenderProfileMax[i] = 0;
        }
        g_RenderProfileStats.samples = RENDER_PROFILE_WINDOW;
        g_RenderProfileStats.collected = 0;
    }
}

void renderProfileRead(RenderProfileStats *stats)
{
    /* Scheduler publishes; main thread reads a consistent snapshot. */
    OSIntMask mask = osSetIntMask(OS_IM_NONE);
    *stats = g_RenderProfileStats;
    osSetIntMask(mask);
}

u32 renderProfileToUsec(u32 ticks, s32 metric)
{
    if (metric == RENDER_PROFILE_GFX || metric == RENDER_PROFILE_AA_CPU) {
        return OS_CYCLES_TO_USEC((u64)ticks);
    }
    /* RDP counters use the 62.5 MHz RCP clock; osGetCount uses 46.875 MHz. */
    return (ticks * 2 + 62) / 125;
}
