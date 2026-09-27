#ifndef GEDITOR_STUDIOMATH_H
#define GEDITOR_STUDIOMATH_H
#include "studioscene.h"
#include "orbitcamera.h"
#define STUDIO_FOV 45.0
BOOL StudioBounds(const StudioScene *scene, int selected, double lower[3], double upper[3]);
BOOL StudioRay(const OrbitCamera *camera, int width, int height, double x, double y, double origin[3], double direction[3]);
int StudioPick(const StudioScene *scene, const double origin[3], const double direction[3], int *material);
/* Separate diffuse and Phong specular terms so a base image never tints the highlight. */
void StudioShade(const StudioMaterial *material, const BgVertex *vertex, const double position[3],
    const double eye[3], float diffuse[3], float specular[3]);
#endif
