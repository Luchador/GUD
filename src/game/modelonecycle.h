#ifndef _MODELONECYCLE_H_
#define _MODELONECYCLE_H_

#include <ultra64.h>
#include <bondtypes.h>

/* Reset only at stage initialization, after the old graphics queue has drained
 * and mema's heap has been reset. Old copies may still be queued otherwise. */
void modelOneCycleResetCache(void);
void modelOneCycleInvalidateGdlRange(Gfx *start, Gfx *end);
Gfx *modelGetOneCycleGdl(ModelRenderData *renderdata, Gfx *primary, s32 modelType);

#endif
