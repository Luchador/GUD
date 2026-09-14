#ifndef _RENDERCONFIG_H_
#define _RENDERCONFIG_H_
#include <ultra64.h>
#include <bondtypes.h>

bool renderIsAaEnabled(void);
bool renderUseOneCycle(void);
bool renderIsViFilterEnabled(void);
void renderSetAaEnabled(bool enabled);
void renderSetViFilterEnabled(bool enabled);
bool renderSettingsPending(void);
void renderApplySettings(void); /* Call only after queued graphics have drained. */
u8 renderEncodeSettings(void);
void renderDecodeSettings(u8 settings);
void renderConfigureViMode(OSViMode *mode);

void renderInvalidateDisplayListCache(void);
bool renderListIsDynamic(Gfx *gdl); /* Frame buffers must never enter persistent caches. */
/* Pure command conversion for room-load builders, independent of queued settings. */
Gfx renderGetAaOffCommand(Gfx command);
void renderRestoreDisplayListSettings(Gfx *start, Gfx *end);
bool renderApplyDisplayListSettings(Gfx *start, Gfx *end);

#endif
