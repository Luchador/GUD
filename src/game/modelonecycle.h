#ifndef _MODELONECYCLE_H_
#define _MODELONECYCLE_H_

#include <ultra64.h>
#include <bondtypes.h>

/* Forget entries at stage initialization or during a drained render-cache
 * reclaim. The render-cache allocator owns all copies, including retired ones. */
void modelOneCycleResetCache(void);
void modelOneCycleInvalidateGdlRange(Gfx *start, Gfx *end);
/* Resolve authored segment-5 lists against the node's BaseAddr for CPU reads.
 * Failed conversions return the original pointer in its original format. */
Gfx *modelGetOneCycleGdl(ModelRenderData *renderdata, Gfx *primary, s32 modelType, void *baseAddr);

#endif
