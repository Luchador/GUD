#ifndef _N64_RDP_PROBE_H_
#define _N64_RDP_PROBE_H_

#include "n64diagnostics.h"

#if N64_LOAD_DIAGNOSTICS && N64_DIAG_RDP_PROBE
/* One private list per existing graphics-task slot; callers must preserve
 * the scheduler's ownership of each slot until its task has completed. */
Gfx *n64RdpProbeBuild(u32 slot, void *framebuffer, u32 frame, u32 *size);
#endif

#endif
