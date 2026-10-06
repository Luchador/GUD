#ifndef GEDITOR_BAKEDLIGHTING_H
#define GEDITOR_BAKEDLIGHTING_H
#include "bglighting.h"
#define BAKEDLIGHTING_WM_BAKE (WM_APP + 0x2e0)
#define BAKEDLIGHTING_WM_HISTORY (WM_APP + 0x2e1)
typedef struct BakedLightingRequest {
    const DWORD *rooms;
    DWORD count;
    BgLightingSettings settings;
    BgLightingResult result;
    const char *why;
} BakedLightingRequest;
BOOL BakedLightingShow(HWND owner, const BgDocument *document, const char *level);
void BakedLightingRefresh(const BgDocument *document, const char *level);
void BakedLightingClose(void);
/* Room-table compaction invalidates the caller's selected room numbers. */
void BakedLightingResetRooms(void);
BOOL BakedLightingHandleMessage(MSG *message);
#endif
