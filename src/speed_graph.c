#include <ultra64.h>
#include "frametiming.h"
#include "speedgraphrenderer.h"
#include "debugmenu.h"
#include "sched.h"

/**
* Used in speedgraphDisplayMetrics to check if speedgraphframes and g_speedGraphCountAccumulator are
* over the threshold for output.
*/
#define COUNT_REQUIRED_FOR_OUTPUT 20

/**
 * Used in speedgraphDisplayMetrics to calculate display Hz.
 */
#ifdef REFRESH_PAL
#define VICLOCK 50
#else
#define VICLOCK 60
#endif

#if defined(LEFTOVERDEBUG)
s32 gSpeedGraphMarkerFlags[3];
Gfx gSpeedGraphDisplayLists[2][266];
s32 gSpeedGraphDisplayListIndex;
u32 g_speedGraphLastOsCount;
u32 g_speedGraphCurrentOsCount;
u32 dword_CODE_bss_800604AC;
#endif

typedef struct {
    u32 markerFlags;
    u32 osCount;
} SpeedGraphMarker;

#if defined(LEFTOVERDEBUG)
SpeedGraphMarker gSpeedGraphMarkers[3][32];
s32 gSpeedGraphMarkerIndices[3];
s32 gSpeedGraphMarkerNextIndices[3];
s32 gSpeedGraphMarkerHeadIndices[3];

#endif
s32 D_800231D0 = 0;

typedef struct {
    s32 index;
    s32 color;
    s32 width;
    s32 height;
} SpeedGraphColorSettings;

#if defined(LEFTOVERDEBUG)
SpeedGraphColorSettings gSpeedGraphColorSettings[5] = {
    {0x00000000, 0x00000000, 0x00000002, 0x00000000},
    {0x00000001, 0x00000000, 0x00000002, 0x00000000},
    {0x00000002, 0xFF000000, 0x00000002, 0x00000000},
    {0x00000003, 0x00009200, 0x00000004, 0xFFFFFFFF},
    {0x00000004, 0xDB000000, 0x00000004, 0xFFFFFFFF}
};

/**
 * 80023224. Seems to accumulate speedgraphframes in speedgraphDisplayMetrics. Once above the threshold COUNT_REQUIRED_FOR_OUTPUT,
 * the value COUNT_REQUIRED_FOR_OUTPUT is repeatedly subtracted until below the threshold.
 */
u32 g_speedGraphCountAccumulator = 0;

/**
 * 80023228. Stores max value of speedgraphframes seen in speedgraphDisplayMetrics. Resets to zero once
 * output is rendered.
 */
s32 g_speedGraphMaxSeenCount = 0;

#endif


/**
 * @brief Initializes the speed graph display lists and marker indices.
 */
void speedgraphInit(void)
{
    #if defined(LEFTOVERDEBUG)
    s32 i;

    gSPEndDisplayList(gSpeedGraphDisplayLists[0]);
    gSPEndDisplayList(gSpeedGraphDisplayLists[1]);
    gSpeedGraphDisplayListIndex = 0;

    for (i = 0; i < 3; i++)
    {
        gSpeedGraphMarkerIndices[i] = 0;
        gSpeedGraphMarkerHeadIndices[i] = 1;
    }

    #endif
}


/**
 * @brief Updates the speed graph marker indices and current OS counter value.
 */
void speedgraphMarkerUpdate(void) {
    #if defined(LEFTOVERDEBUG)
    s32 i;

    g_speedGraphCurrentOsCount = osGetCount();

    for (i = 0; i < 3; i++)
    {        
        gSpeedGraphMarkerNextIndices[i] = ((gSpeedGraphMarkerHeadIndices[i] + 31) % 32);
    }
    #endif
}
