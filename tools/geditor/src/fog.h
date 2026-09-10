#ifndef GEDITOR_FOG_H
#define GEDITOR_FOG_H

/* View-only N64 fog curve in editor world units. No ROM/project data changes. */
typedef struct FogCurve {
    double constant, reciprocal; /* fog amount = constant - reciprocal / depth */
    float linearstart, linearend; /* distance approximation for OpenGL 1.1 */
} FogCurve;

int FogConfigure(FogCurve *curve, double nearclip, double farclip, double renderscale, int start, int end);
float FogAmount(const FogCurve *curve, double depth);

#endif
