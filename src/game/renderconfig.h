#ifndef _RENDERCONFIG_H_
#define _RENDERCONFIG_H_
#include <ultra64.h>
#include <bondtypes.h>

enum RenderAaStyle {
    RENDER_AA_FULL,
    RENDER_AA_REDUCED,
    RENDER_AA_OFF,
    RENDER_AA_COUNT
};

enum RenderViFilter {
    RENDER_VI_SMOOTH,
    RENDER_VI_EDGES,
    RENDER_VI_OFF,
    RENDER_VI_COUNT
};

s32 renderGetAaStyle(void);
s32 renderGetViFilter(void);
void renderSetAaStyle(s32 style);
void renderSetViFilter(s32 filter);
bool renderSettingsPending(void);
void renderApplySettings(void); /* Call only after queued graphics have drained. */
u8 renderEncodeSettings(void);
void renderDecodeSettings(u8 settings);
void renderConfigureViMode(OSViMode *mode);

void renderInvalidateAaCache(void);
void renderRestoreAaGdl(Gfx *start, Gfx *end);
bool renderApplyAa(Gfx *start, Gfx *end);

#endif
