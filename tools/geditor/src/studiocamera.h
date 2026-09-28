#ifndef GEDITOR_STUDIOCAMERA_H
#define GEDITOR_STUDIOCAMERA_H
#include "studiomath.h"
#define STUDIO_PREVIEW_SIZE 256
/* Client-pixel rectangle, excluding the one-pixel border. No DPI scaling. */
typedef struct StudioPreviewRect { int left,top,right,bottom; } StudioPreviewRect;
BOOL StudioCameraPreviewRect(int width,int height,StudioPreviewRect *rect);
/* Fit the output aspect inside a preview rectangle, centered with bars. */
void StudioCameraImageRect(const StudioRenderSettings *settings,StudioPreviewRect *rect);
void StudioCameraView(const StudioCamera *camera,double matrix[16],double view[3]);
void StudioCameraClip(const StudioScene *scene,double *nearz,double *farz);
#endif
