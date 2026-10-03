#ifndef GEDITOR_BGLIGHTING_H
#define GEDITOR_BGLIGHTING_H
#include "bgdocument.h"

typedef struct BgLightingSettings {
    unsigned char ambient[3], directional[3];
    double ambientIntensity, directionalIntensity;
    double direction[3]; /* World vector toward the light; +Y up. */
    double smoothAngle; /* Connected shared edges, degrees, 0..180. */
} BgLightingSettings;
typedef struct BgLightingResult {
    DWORD rooms, faces, vertices, splits;
} BgLightingResult;
extern const BgLightingSettings g_BgLightingDefaults;
BOOL BgLightingValidate(const BgLightingSettings *settings, const char **why);
/* Atomic RGB replacement on both BG layers. Preserve alpha, UVs, materials,
 * face IDs and unselected rooms. Smooth only existing shared vertex fans;
 * duplicate a vertex where a crease needs a different color. No shadows. */
BOOL BgDocumentBakeLighting(BgDocument *document, const DWORD *rooms, DWORD count,
    const BgLightingSettings *settings, BgLightingResult *result, const char **why);
#endif
