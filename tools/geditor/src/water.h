#ifndef GEDITOR_WATER_H
#define GEDITOR_WATER_H

#include "rom.h"

typedef struct WaterSample {
    float s, t, q, color[3];
} WaterSample;

/* Projective texture coordinates for sky.c's world-space water plane.
 * Clip the screen mesh to downward rays before drawing these samples. */
WaterSample WaterSamplePlane(const RomWater *water, const double eye[3],
    const double direction[3], const float background[3], double seconds, int width, int height);
float WaterBlend(double seconds);

#endif
