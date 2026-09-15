#ifndef GEDITOR_CLOUDS_H
#define GEDITOR_CLOUDS_H

#include "rom.h"
#include "texload.h"

typedef struct CloudSample {
    float s, t, opacity;
} CloudSample;

/* Approximate sky.c's cloud plane. direction need not be normalized. */
CloudSample CloudsSample(const RomClouds *clouds, const double eye[3],
    const double direction[3], double seconds, int width, int height);
/* Bake (cloud shade - sky) * texture + sky into RGB; opacity supplies the
 * horizon fade. The game's cloud combiner ignores the texture's alpha. */
void CloudsTint(TexPixel *pixels, DWORD count, const RomClouds *clouds,
    const float background[3]);

#endif
