#include <ultra64.h>
#include <PR/R4300.h>
#include "bgonecycle.h"
#include "model.h"
#include "modellod.h"
#include "modelonecycle.h"
#include "renderconfig.h"
#include "rendercache.h"
#include "dyn.h"

#define MODEL_ONE_CYCLE_CACHE_SIZE 256
#define MODEL_ONE_CYCLE_BYTE_LIMIT 0x10000

typedef struct ModelOneCycleEntry {
    Gfx *source;
    Gfx *alternate;
    s32 sourceSize;
    u8 material;
    u8 valid;
    u8 pipelineSafe;
    u8 characterFixups;
} ModelOneCycleEntry;

static ModelOneCycleEntry g_ModelOneCycleCache[MODEL_ONE_CYCLE_CACHE_SIZE];
static s32 g_ModelOneCycleBytes;

void modelOneCycleResetCache(void)
{
    s32 i;
    modelLodResetCache();
    for (i = 0; i < MODEL_ONE_CYCLE_CACHE_SIZE; i++) g_ModelOneCycleCache[i].source = NULL;
    g_ModelOneCycleBytes = 0;
}

void modelOneCycleInvalidateGdlRange(Gfx *start, Gfx *end)
{
    s32 i;
    modelLodResetCache();
    for (i = 0; i < MODEL_ONE_CYCLE_CACHE_SIZE; i++) {
        ModelOneCycleEntry *entry = &g_ModelOneCycleCache[i];
        if (entry->source && (u32)start < (u32)entry->source + entry->sourceSize
                && (u32)end > (u32)entry->source) entry->valid = FALSE;
    }
    /* Retain invalidated copies until stage reset or a drained cache reclaim:
     * submitted graphics tasks can still reference them. The allocation
     * registry retains copies even after their cache entries are replaced. */
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

/* These are the ordinary material commands emitted by GEditor. Characters
 * supply blood tint and opacity at draw time; an authored SHADE/MODULATE
 * command must not replace that per-instance combiner. Other equations are
 * left alone, including special effects and explicit constant-alpha faces. */
static s32 modelCharacterCombinerKind(Gfx command)
{
    static const Gfx textured[] = {
        gsDPSetCombineMode(G_CC_MODULATEIA, G_CC_MODULATEIA),
        gsDPSetCombineMode(G_CC_TRILERP, G_CC_MODULATEIA2),
        {{0xfc26e404, 0x1f10ffff}} /* detail texture */
    };
    static const Gfx shade[] = {
        gsDPSetCombineMode(G_CC_SHADE, G_CC_SHADE),
        gsDPSetCombineMode(G_CC_SHADE, G_CC_PASS2)
    };
    s32 i;
    for (i = 0; i < 3; i++) {
        if (command.words.w0 == textured[i].words.w0
                && command.words.w1 == textured[i].words.w1) return 1;
    }
    for (i = 0; i < 2; i++) {
        if (command.words.w0 == shade[i].words.w0
                && command.words.w1 == shade[i].words.w1) return 2;
    }
    return 0;
}

static Gfx *modelResolveGdl(Gfx *source, void *baseAddr)
{
    if (((u32)source >> 24) == SPSEGMENT_MODEL_COL1) {
        u32 offset = (u32)source & 0x00ffffff;
        if (!IS_KSEG0(baseAddr) || K0_TO_PHYS(baseAddr) >= osMemSize
                || offset >= osMemSize - K0_TO_PHYS(baseAddr)) return NULL;
        source = (Gfx *)((u8 *)baseAddr + offset);
    }
    if (!IS_KSEG0(source) || K0_TO_PHYS(source) >= osMemSize) return NULL;
    return source;
}

static ModelOneCycleEntry *modelMaterialEntry(Gfx *source, u8 material)
{
    u32 slot = (((u32)source >> 3) ^ material) & (MODEL_ONE_CYCLE_CACHE_SIZE - 1);
    s32 count;
    for (count = 0; count < MODEL_ONE_CYCLE_CACHE_SIZE; count++) {
        ModelOneCycleEntry *entry = &g_ModelOneCycleCache[slot];
        if (!entry->source || (entry->source == source && entry->material == material)) return entry;
        slot = (slot + 1) & (MODEL_ONE_CYCLE_CACHE_SIZE - 1);
    }
    return NULL;
}

static ModelOneCycleEntry *modelInspectGdl(Gfx *primary, void *baseAddr, ModelOneCycleEntry *local)
{
    Gfx *source = modelResolveGdl(primary, baseAddr);
    ModelOneCycleEntry *entry;
    s32 i;
    local->source = NULL;
    local->valid = FALSE;
    local->pipelineSafe = FALSE;
    local->characterFixups = FALSE;
    if (!source) return local;
    /* Material 16 is metadata only, shared by all instances and draw modes.
     * Transient lists and a full table are inspected without retaining them. */
    entry = renderListIsDynamic(source) ? NULL : modelMaterialEntry(source, 16);
    if (!entry) entry = local;
    if (entry->source == source && entry->valid) return entry;
    entry->source = source;
    entry->material = 16;
    entry->alternate = NULL;
    entry->valid = TRUE;
    entry->pipelineSafe = FALSE;
    entry->characterFixups = FALSE;
    entry->sourceSize = modelOneCycleListSize(source);
    if (!entry->sourceSize) {
        entry->sourceSize = sizeof(Gfx);
        return entry;
    }
    entry->pipelineSafe = TRUE;
    for (i = 0; i < entry->sourceSize / sizeof(Gfx); i++) {
        Gfx command = source[i];
        u32 op = command.words.w0 >> 24;
        if (op == (u8)G_SETCOMBINE || op == (u8)G_SETOTHERMODE_L
                || op == (u8)G_RDPSETOTHERMODE
                || op == (u8)G_SETENVCOLOR || op == (u8)G_SETPRIMCOLOR || op == (u8)G_SETFOGCOLOR
                || command.words.w0 == 0xba001402 /* cycle type */
                || (op == (u8)G_MOVEWORD && (command.words.w0 & 255) == G_MW_SEGMENT)) {
            entry->pipelineSafe = FALSE;
            if (op != (u8)G_SETCOMBINE || !modelCharacterCombinerKind(command)) {
                entry->characterFixups |= 2; /* state outside the combiner repair */
            }
        }
        if (op == (u8)G_SETCOMBINE && modelCharacterCombinerKind(command)) entry->characterFixups |= 1;
    }
    return entry;
}

bool modelGdlPreservesType3Pipeline(ModelRenderData *renderdata, Gfx *primary, void *baseAddr)
{
    ModelOneCycleEntry local;
    ModelOneCycleEntry *info = modelInspectGdl(primary, baseAddr, &local);
    return info->pipelineSafe || (info->characterFixups == 1
            && (renderdata->PropType == PROP_TYPE_VIEWER + 1
                || renderdata->PropType == PROP_TYPE_EXPLOSION + 1));
}

static Gfx *modelGetCharacterGdl(ModelRenderData *renderdata, Gfx *primary, s32 modelType, void *baseAddr)
{
    ModelOneCycleEntry local;
    ModelOneCycleEntry *info = modelInspectGdl(primary, baseAddr, &local);
    ModelOneCycleEntry *entry;
    ModelRenderData setup = *renderdata;
    Gfx initial[16], combine, untextured;
    Gfx *out, *alternate;
    s32 i, size;
    bool retained = FALSE;
    bool fading = renderdata->PropType == PROP_TYPE_EXPLOSION + 1;
    if (!(info->characterFixups & 1)) return primary;
    entry = renderListIsDynamic(info->source) ? NULL : modelMaterialEntry(info->source, 32 + fading);
    if (entry && entry->source && entry->valid && entry->alternate) return entry->alternate;

    /* Only equations go into the copy. ENV colour/alpha remain in the parent
     * list, so several characters can share it at different fade amounts. */
    setup.gdl = initial;
    if (modelType == 3) modelApplyRenderModeType3(&setup, TRUE);
    else modelApplyRenderModeType4(&setup, TRUE);
    combine = initial[0];
    for (out = initial; out < setup.gdl; out++) {
        if (out->words.w0 >> 24 == (u8)G_SETCOMBINE) combine = *out;
    }
    if (fading) {
        gDPSetCombineLERP(&untextured, 1, ENVIRONMENT, SHADE_ALPHA, ENVIRONMENT,
                0, 0, 0, ENVIRONMENT, COMBINED, 0, SHADE, 0, 0, 0, 0, COMBINED);
    } else {
        gDPSetCombineLERP(&untextured, 1, ENVIRONMENT, SHADE_ALPHA, ENVIRONMENT,
                0, 0, 0, 1, COMBINED, 0, SHADE, 0, 0, 0, 0, COMBINED);
    }
    size = info->sourceSize + 2 * sizeof(Gfx);
    size = (size + 15) & ~15;
    alternate = NULL;
    if (entry && size <= MODEL_ONE_CYCLE_BYTE_LIMIT - g_ModelOneCycleBytes) {
        alternate = renderCacheAlloc(size);
        retained = alternate != NULL;
    }
    /* This is a correctness fix, including with AA enabled or optional
     * caches reclaimed. A frame-owned copy is the allocation fallback. */
    if (!alternate) alternate = dynAllocate(size);
    out = alternate;
    for (i = 0; i < info->sourceSize / sizeof(Gfx); i++) {
        Gfx command = info->source[i];
        s32 kind = modelCharacterCombinerKind(command);
        if (command.words.w0 >> 24 == (u8)G_ENDDL) {
            /* Secondary lists can inherit the primary character equation. */
            gDPPipeSync(out++);
            *out++ = combine;
        }
        *out++ = kind == 1 ? combine : kind == 2 ? untextured : command;
    }
    if (retained) {
        entry->source = info->source;
        entry->sourceSize = info->sourceSize;
        entry->material = 32 + fading;
        entry->alternate = alternate;
        entry->valid = TRUE;
        g_ModelOneCycleBytes += size;
        renderInvalidateDisplayListCache();
    }
    return alternate;
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
        return NULL;
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
    if (size <= 0) return NULL;
    size = (size + 15) & ~15;
    if (size > MODEL_ONE_CYCLE_BYTE_LIMIT - g_ModelOneCycleBytes) return NULL;
    alternate = renderCacheAlloc(size);
    if (!alternate) return NULL;
    if (gfxBuildOneCycleGdl(primary, entry->sourceSize, alternate, size,
            initial, (setup.gdl - initial) * sizeof(Gfx)) <= 0) {
        renderCacheFree(alternate);
        return NULL;
    }
    entry->alternate = alternate;
    g_ModelOneCycleBytes += size;
    renderInvalidateDisplayListCache();
    return alternate;
}

Gfx *modelGetOneCycleGdl(ModelRenderData *renderdata, Gfx *primary, s32 modelType, void *baseAddr)
{
    ModelOneCycleEntry *entry;
    Gfx *source = primary;
    Gfx *alternate;
    u32 slot;
    s32 count;
    u8 material;
    bool firstPerson;

    if (primary && (modelType == 3 || modelType == 4)
            && (renderdata->PropType == PROP_TYPE_VIEWER + 1
                || renderdata->PropType == PROP_TYPE_EXPLOSION + 1)) {
        return modelGetCharacterGdl(renderdata, primary, modelType, baseAddr);
    }

    /* PropType 9 is the ordinary world-prop material, including crate props.
     * Its low environment byte selects the damaged path. Character
     * blood tinting, fading and viewer materials use other equations. Held
     * weapons explicitly opt in; their fog blender supplies room lighting. */
    if (!primary || !renderUseOneCycle() || !renderCacheIsEnabled()
            || (modelType != 2 && modelType != 3 && modelType != 4)) return primary;

    firstPerson = renderdata->PropType == PROP_TYPE_WEAPON
            && (renderdata->flags & MODEL_RENDER_FIRST_PERSON);
    if (!firstPerson && (renderdata->PropType != PROP_TYPE_SMOKE + 1
            || ((renderdata->envcolour.word & 0xff)
                && (modelType == 2 || (renderdata->envcolour.word & 0xff) < BG_CUTOUT_THRESHOLD)))) return primary;

    /* Model lists remain segmented after loading. The CPU needs a RAM
     * address, while fallback draws must retain the authored GBI address.
     * Resolve before hashing so identical offsets in different models cannot
     * share a copy. Runtime lists such as the taser screen stay excluded. */
    if (((u32)primary >> 24) == SPSEGMENT_MODEL_COL1)
    {
        u32 offset = (u32)primary & 0x00ffffff;

        if (!IS_KSEG0(baseAddr) || K0_TO_PHYS(baseAddr) >= osMemSize) return primary;
        if (offset >= osMemSize - K0_TO_PHYS(baseAddr)) return primary;
        source = (Gfx *)((u8 *)baseAddr + offset);
    }
    if (!IS_KSEG0(source) || K0_TO_PHYS(source) >= osMemSize
            || renderListIsDynamic(source)) return primary;

    /* Type 3/4 primary setups are equivalent within each material family.
     * Keep weapon lighting separate from intact/damaged world-prop materials;
     * colours and damage levels stay per instance rather than in the copy. */
    material = (modelType == 2 ? 0 : 2) | (renderdata->zbufferenabled ? 1 : 0)
            | (firstPerson ? 8 : (renderdata->envcolour.word & 0xff) ? 4 : 0);
    slot = (((u32)source >> 3) ^ material) & (MODEL_ONE_CYCLE_CACHE_SIZE - 1);
    for (count = 0; count < MODEL_ONE_CYCLE_CACHE_SIZE; count++) {
        entry = &g_ModelOneCycleCache[slot];
        if (!entry->source || (entry->source == source && entry->material == material)) break;
        slot = (slot + 1) & (MODEL_ONE_CYCLE_CACHE_SIZE - 1);
    }
    if (count == MODEL_ONE_CYCLE_CACHE_SIZE) return primary;
    if (entry->source && entry->valid) return entry->alternate ? entry->alternate : primary;

    entry->source = source;
    entry->material = material;
    alternate = modelOneCycleBuildEntry(entry, renderdata, modelType);
    return alternate ? alternate : primary;
}
