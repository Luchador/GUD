#ifndef GEDITOR_STUDIOCAMERAVIEW_H
#define GEDITOR_STUDIOCAMERAVIEW_H
#include "studiocamera.h"
typedef struct StudioCameraModel { BgVertex *vertices; DWORD count; } StudioCameraModel;
BOOL StudioCameraModelLoad(StudioCameraModel *model,HINSTANCE instance);
void StudioCameraModelFree(StudioCameraModel *model);
void StudioCameraModelDraw(const StudioCameraModel *model,const StudioCamera *camera,BOOL selected);
BOOL StudioCameraModelPick(const StudioCameraModel *model,const StudioCamera *camera,
    const double origin[3],const double direction[3],double *distance);
#endif
