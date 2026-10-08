#ifndef GEDITOR_BGAO_H
#define GEDITOR_BGAO_H
#include "bgdocument.h"

typedef struct BgAoScene BgAoScene;
/* All primary faces, in world units. Secondary faces never cast AO. */
BOOL BgAoBuild(const BgDocument *document, BgAoScene **scene, const char **why);
void BgAoFree(BgAoScene *scene);
/* Fraction of ambient light blocked within radius, 0..1. normal is unit length. */
double BgAoOcclusion(const BgAoScene *scene, const double position[3],
    const double normal[3], double radius);
#endif
