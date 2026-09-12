#ifndef _N64_RDP_CHECK_H_
#define _N64_RDP_CHECK_H_
#include <ultra64.h>

enum N64RdpCheckStatus {
    N64RDP_UNCHECKED, N64RDP_CHECKED, N64RDP_PARTIAL, N64RDP_HAZARD
};

/* Addresses are RDRAM physical addresses. The reader must reject ranges
 * outside installed RAM and read CPU-owned display lists through the cache. */
typedef s32 (*N64RdpRead)(u32 address, u32 *word0, u32 *word1);
typedef struct {
    u32 status, commands, address, word0, word1, caller;
    u32 modeHi, modeLo, texture, textureSize, colorSize, scissorLeft;
    /* Last depth-enabled GBI draw, not the RDP's current hardware state.
     * Counts are GBI draw commands (TRI4 is one), not rendered triangles. */
    u32 depthCompareDraws, depthWriteDraws;
    u32 depthImage, depthColor, depthWidth, depthMode, depthCommand, depthDraw;
    const char *reason;
} N64RdpCheck;

/* Temporary controller experiment: preserve every bit except Z_CMP in
 * complete render-mode setters emitted by the model renderer. */
u32 n64RdpWithoutDepthCompare(u32 word0, u32 word1);

void n64RdpCheckList(u32 start, u32 bytes, u32 ramSize,
        N64RdpRead read, N64RdpCheck *result);
#endif
