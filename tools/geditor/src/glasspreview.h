#ifndef GEDITOR_GLASSPREVIEW_H
#define GEDITOR_GLASSPREVIEW_H
#include "setupload.h"
#include "bgload.h"

typedef struct GlassPreview {
    BOOL active;
    float position[3]; /* Runtime object origin in world centimetres. */
    float tintdistance, opaquedistance, minimumopacity;
} GlassPreview;
int GlassPreviewAlpha(const GlassPreview *glass, const float eye[3]);
/* Same bound-pad probe, polygon test and nearest-plane rule as setup.c/bg.c.
 * The result is derived; never save it over the runtime portal cache. */
int GlassFindPortal(const SetupFile *setup, DWORD index, const BgPortalFile *portals, float levelscale);
#endif
