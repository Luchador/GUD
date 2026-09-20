#ifndef _MODELONECYCLE_H_
#define _MODELONECYCLE_H_

#include <ultra64.h>
#include <bondtypes.h>

/* Forget entries at stage initialization or during a drained render-cache
 * reclaim. The render-cache allocator owns all copies, including retired ones. */
void modelOneCycleResetCache(void);
void modelOneCycleInvalidateGdlRange(Gfx *start, Gfx *end);
/* Includes character material repair, independently of the one-cycle option.
 * Resolve authored segment-5 lists against the node's BaseAddr for CPU reads.
 * Failed conversions return the original pointer in its original format. */
Gfx *modelGetOneCycleGdl(ModelRenderData *renderdata, Gfx *primary, s32 modelType, void *baseAddr);
/* Only call with the unchanged authored vertex buffer. Blood-stained parts
 * use modelGetOneCycleGdl, preserving the original character equations. */
Gfx *modelGetUnbloodiedGdl(ModelRenderData *renderdata, Gfx *primary, s32 modelType,
        void *baseAddr, Vertex *vertices, s32 numVertices);
/* Stock body lists inherit pipeline state. Edited lists may override it. */
bool modelGdlPreservesType3Pipeline(ModelRenderData *renderdata, Gfx *primary, void *baseAddr);

#endif
