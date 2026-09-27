#ifndef GEDITOR_STUDIOGIZMO_H
#define GEDITOR_STUDIOGIZMO_H
#include "studiomath.h"

enum { STUDIO_TRANSLATE, STUDIO_ROTATE, STUDIO_SCALE };
typedef struct StudioGizmo { BgVertex *vertices[3]; DWORD count[3]; } StudioGizmo;
typedef struct StudioGizmoFrame { double origin[3],length; Rotation axes; int mode; } StudioGizmoFrame;
typedef struct StudioDrag {
    StudioTransform before;
    OrbitCamera camera;
    int mode,axis,width,height;
    double origin[3],direction[3],plane[3],length,parameter,lastangle,angle;
    double mouse[2],tangent[2];
    BOOL fallback;
} StudioDrag;

BOOL StudioGizmoLoad(StudioGizmo *gizmo,HINSTANCE instance);
void StudioGizmoFree(StudioGizmo *gizmo);
BOOL StudioGizmoPlace(const StudioTransform *transform,const OrbitCamera *camera,int width,int height,int mode,StudioGizmoFrame *frame);
void StudioGizmoDraw(const StudioGizmo *gizmo,const StudioGizmoFrame *frame,int highlight);
int StudioGizmoPick(const StudioGizmo *gizmo,const StudioGizmoFrame *frame,const double origin[3],const double direction[3],double hit[3]);
BOOL StudioDragBegin(StudioDrag *drag,const StudioTransform *transform,const OrbitCamera *camera,int width,int height,
    double x,double y,const StudioGizmoFrame *frame,int axis,const double hit[3]);
BOOL StudioDragUpdate(StudioDrag *drag,double x,double y,BOOL snap,StudioTransform *transform);
#endif
