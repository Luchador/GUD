#include <ultra64.h>
#include <mema.h>
#include "bg.h"
#include "modelonecycle.h"
#include "rendercache.h"
#include "renderconfig.h"

/* Track allocations, rather than just cache entries: model invalidation can
 * replace an entry while an older submitted copy still needs to stay alive.
 * A 16-byte header on N64 preserves the display list's alignment. */
typedef struct RenderCacheAllocation {
    struct RenderCacheAllocation *next;
    struct RenderCacheAllocation *prev;
    s32 size;
    s32 padding;
} RenderCacheAllocation;

#define RENDER_CACHE_HEADER_SIZE ((sizeof(RenderCacheAllocation) + 15) & ~15)

static RenderCacheAllocation *g_RenderCacheAllocations;
static bool g_RenderCacheEnabled = TRUE;
static bool g_RenderCacheReclaimPending;

void renderCacheReset(void)
{
    /* mema has already discarded the previous stage's allocations. */
    g_RenderCacheAllocations = NULL;
    g_RenderCacheEnabled = TRUE;
    g_RenderCacheReclaimPending = FALSE;
}

bool renderCacheIsEnabled(void)
{
    return g_RenderCacheEnabled;
}

void *renderCacheAlloc(s32 size)
{
    RenderCacheAllocation *allocation;

    if (!g_RenderCacheEnabled || size <= 0
            || size > 0x7fffffff - RENDER_CACHE_HEADER_SIZE - 15) return NULL;
    size = ((size + 15) & ~15) + RENDER_CACHE_HEADER_SIZE;
    allocation = memaAlloc(size);
    if (!allocation) return NULL;
    allocation->size = size;
    allocation->prev = NULL;
    allocation->next = g_RenderCacheAllocations;
    if (allocation->next) allocation->next->prev = allocation;
    g_RenderCacheAllocations = allocation;
    return (u8 *)allocation + RENDER_CACHE_HEADER_SIZE;
}

void renderCacheFree(void *data)
{
    RenderCacheAllocation *allocation;

    if (!data) return;
    allocation = (RenderCacheAllocation *)((u8 *)data - RENDER_CACHE_HEADER_SIZE);
    if (allocation->prev) allocation->prev->next = allocation->next;
    else g_RenderCacheAllocations = allocation->next;
    if (allocation->next) allocation->next->prev = allocation->prev;
    memaFree(allocation, allocation->size);
}

void renderCacheRequestReclaim(void)
{
    /* Stay on original display lists until the next stage load. Rebuilding
     * optional copies immediately would recreate the same allocation failure
     * and repeatedly stall the graphics queue. Existing copies remain valid
     * through the rest of this frame and all previously submitted frames. */
    g_RenderCacheEnabled = FALSE;
    g_RenderCacheReclaimPending = g_RenderCacheAllocations != NULL;
}

bool renderCacheReclaimPending(void)
{
    return g_RenderCacheReclaimPending;
}

void renderCacheReclaim(void)
{
    RenderCacheAllocation *allocation;
    RenderCacheAllocation *next;

    if (!g_RenderCacheReclaimPending) return;
    bgClearRoomRenderCaches();
    modelOneCycleResetCache();
    for (allocation = g_RenderCacheAllocations; allocation; allocation = next)
    {
        next = allocation->next;
        memaFree(allocation, allocation->size);
    }
    g_RenderCacheAllocations = NULL;
    g_RenderCacheReclaimPending = FALSE;
    renderInvalidateDisplayListCache();
}
