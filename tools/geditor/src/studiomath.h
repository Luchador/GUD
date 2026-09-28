#ifndef GEDITOR_STUDIOMATH_H
#define GEDITOR_STUDIOMATH_H
#include "studioscene.h"
#include "orbitcamera.h"
#include "rotation.h"
#define STUDIO_FOV 45.0
/* Column-major OpenGL transform, plus inverse-transpose normal transform. */
typedef struct StudioMatrix { double m[16], normal[9]; } StudioMatrix;
void StudioMatrixBuild(const StudioTransform *transform, StudioMatrix *matrix);
void StudioPoint(const StudioMatrix *matrix, const double in[3], double out[3]);
BOOL StudioProject(const OrbitCamera *camera,int width,int height,const double world[3],double screen[2]);
BOOL StudioRayTriangle(const double origin[3],const double direction[3],const double vertices[3][3],double *distance);
/* View bounds include lights; model bounds stay separate for model placement. */
BOOL StudioViewBounds(const StudioScene *scene, int selected, double lower[3], double upper[3]);
BOOL StudioBounds(const StudioScene *scene, int selected, double lower[3], double upper[3]);
BOOL StudioRay(const OrbitCamera *camera, int width, int height, double x, double y, double origin[3], double direction[3]);
int StudioPick(const StudioScene *scene, const double origin[3], const double direction[3], int *material);
/* Return the light's attenuated intensity and the unit surface-to-light vector. */
double StudioLightSample(const StudioLight *light, BOOL spotlight, const double point[3], double direction[3]);
/* Separate diffuse and Phong specular terms so a base image never tints the highlight. */
void StudioShade(const StudioScene *scene, const StudioMaterial *material, const BgVertex *vertex, const StudioMatrix *matrix,
    const double eye[3], float diffuse[3], float specular[3]);
#endif
