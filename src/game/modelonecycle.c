#include <ultra64.h>
#include <PR/R4300.h>
#include <mema.h>
#include "bgonecycle.h"
#include "model.h"
#include "modelonecycle.h"
#include "renderconfig.h"

#define MODEL_ONE_CYCLE_CACHE_SIZE 256
#define MODEL_ONE_CYCLE_BYTE_LIMIT 0x10000

typedef struct ModelOneCycleEntry {
    Gfx *source;
    Gfx *alternate;
    s32 sourceSize;
    u8 material;
    u8 valid;
} ModelOneCycleEntry;

static ModelOneCycleEntry g_ModelOneCycleCache[MODEL_ONE_CYCLE_CACHE_SIZE];
static s32 g_ModelOneCycleBytes;

void modelOneCycleResetCache(void)
{
    s32 i;
    for (i = 0; i < MODEL_ONE_CYCLE_CACHE_SIZE; i++) g_ModelOneCycleCache[i].source = NULL;
    g_ModelOneCycleBytes = 0;
}

void modelOneCycleInvalidateGdlRange(Gfx *start, Gfx *end)
{
    s32 i;
    for (i = 0; i < MODEL_ONE_CYCLE_CACHE_SIZE; i++) {
        ModelOneCycleEntry *entry = &g_ModelOneCycleCache[i];
        if (entry->source && (u32)start < (u32)entry->source + entry->sourceSize
                && (u32)end > (u32)entry->source) entry->valid = FALSE;
    }
    /* Retain invalidated copies until stage reset: submitted graphics tasks
     * can still reference them. Their memory remains charged to the budget. */
}

static s32 modelOneCycleListSize(const Gfx *source)
{
    u32 address = K0_TO_PHYS(source);
    s32 limit;
    s32 i;
    if (!IS_KSEG0(source) || (address & 7) || address >= osMemSize) return 0;
    limit = osMemSize - address;
    if (limit > MODEL_ONE_CYCLE_BYTE_LIMIT) limit = MODEL_ONE_CYCLE_BYTE_LIMIT;
    for (i = 0; i < limit / sizeof(Gfx); i++) {
        u32 op = source[i].words.w0 >> 24;
        /* Nested/branching lists can inherit materials and alter segments.
         * Leave those special models on their original pipeline. */
        if (op == (u8)G_DL) return 0;
        if (op == (u8)G_ENDDL) return (i + 1) * sizeof(Gfx);
    }
    return 0;
}

static Gfx *modelOneCycleBuildEntry(ModelOneCycleEntry *entry,
        ModelRenderData *renderdata, s32 modelType)
{
    ModelRenderData setup;
    Gfx initial[16];
    Gfx *primary = entry->source;
    Gfx *alternate;
    s32 size;

    entry->alternate = NULL;
    entry->valid = TRUE;
    entry->sourceSize = modelOneCycleListSize(primary);
    if (!entry->sourceSize) {
        /* A later replacement of this list must still invalidate a rejection. */
        entry->sourceSize = sizeof(Gfx);
        return primary;
    }

    /* Obtain the exact setup from the existing renderer. Only material state
     * is used by the converter; colours remain per instance in the real GDL. */
    setup = *renderdata;
    setup.gdl = initial;
    if (modelType == 2) modelApplyRenderModeType2(&setup);
    else if (modelType == 3) modelApplyRenderModeType3(&setup, TRUE);
    else modelApplyRenderModeType4(&setup, TRUE);
    size = gfxBuildOneCycleGdl(primary, entry->sourceSize, NULL, 0,
            initial, (setup.gdl - initial) * sizeof(Gfx));
    if (size <= 0) return primary;
    size = (size + 15) & ~15;
    if (size > MODEL_ONE_CYCLE_BYTE_LIMIT - g_ModelOneCycleBytes) return primary;
    alternate = memaAlloc(size);
    if (!alternate) return primary;
    if (gfxBuildOneCycleGdl(primary, entry->sourceSize, alternate, size,
            initial, (setup.gdl - initial) * sizeof(Gfx)) <= 0) {
        memaFree(alternate, size);
        return primary;
    }
    entry->alternate = alternate;
    g_ModelOneCycleBytes += size;
    renderInvalidateDisplayListCache();
    return alternate;
}

Gfx *modelGetOneCycleGdl(ModelRenderData *renderdata, Gfx *primary, s32 modelType)
{
    ModelOneCycleEntry *entry;
    u32 slot;
    s32 count;
    u8 material;

    /* PropType 9 is the ordinary world-prop material, including crate props.
     * Its low environment byte selects the damaged/cutout path. Character
     * blood tinting, fading and weapon/viewer materials use other equations. */
    if (!primary || !renderUseOneCycle() || renderdata->PropType != PROP_TYPE_SMOKE + 1
            || (renderdata->envcolour.word & 0xff)
            || (modelType != 2 && modelType != 3 && modelType != 4)
            || renderListIsDynamic(primary)) return primary;

    /* Type 3/4 opaque prop setups are equivalent. Z buffering is per instance. */
    material = (modelType == 2 ? 0 : 2) | (renderdata->zbufferenabled ? 1 : 0);
    slot = (((u32)primary >> 3) ^ material) & (MODEL_ONE_CYCLE_CACHE_SIZE - 1);
    for (count = 0; count < MODEL_ONE_CYCLE_CACHE_SIZE; count++) {
        entry = &g_ModelOneCycleCache[slot];
        if (!entry->source || (entry->source == primary && entry->material == material)) break;
        slot = (slot + 1) & (MODEL_ONE_CYCLE_CACHE_SIZE - 1);
    }
    if (count == MODEL_ONE_CYCLE_CACHE_SIZE) return primary;
    if (entry->source && entry->valid) return entry->alternate ? entry->alternate : primary;

    entry->source = primary;
    entry->material = material;
    return modelOneCycleBuildEntry(entry, renderdata, modelType);
}
