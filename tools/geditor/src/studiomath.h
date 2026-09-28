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
/* View bounds include lights and the camera; model bounds stay separate for model placement. */
BOOL StudioViewBounds(const StudioScene *scene, int selected, double lower[3], double upper[3]);
BOOL StudioBounds(const StudioScene *scene, int selected, double lower[3], double upper[3]);
BOOL StudioRay(const OrbitCamera *camera, int width, int height, double x, double y, double origin[3], double direction[3]);
int StudioPick(const StudioScene *scene, const double origin[3], const double direction[3], int *material);
int StudioPickDistance(const StudioScene *scene,const double origin[3],const double direction[3],int *material,double *distance);
/* Return the light's attenuated intensity and the unit surface-to-light vector. */
double StudioLightSample(const StudioLight *light, BOOL spotlight, const double point[3], double direction[3]);
/* World-aligned reflection UVs for a triangle. Image top is +Y, center is +Z;
 * U wraps across the panorama seam. Coordinates may exceed 1 after unwrapping. */
void StudioEnvironmentCoordinates(const BgVertex vertices[3],const StudioMatrix *matrix,
    const double eye[3],double uv[3][2]);
/* Diffuse and metallic highlights are modulated by the base image; additive
 * contains untextured dielectric highlights plus emission. metallic may be NULL. */
void StudioShade(const StudioScene *scene, const StudioMaterial *material, const BgVertex *vertex, const StudioMatrix *matrix,
    const double eye[3], float diffuse[3], float additive[3], float metallic[3]);
/* Parallel views pass a surface-to-camera direction instead of an eye position. */
void StudioShadeView(const StudioScene *scene,const StudioMaterial *material,const BgVertex *vertex,const StudioMatrix *matrix,
    const double eye[3],BOOL parallel,float diffuse[3],float additive[3],float metallic[3]);
void StudioEnvironmentCoordinatesView(const BgVertex vertices[3],const StudioMatrix *matrix,const double eye[3],BOOL parallel,double uv[3][2]);
#endif
