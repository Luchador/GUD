/* Transform interaction math, independent of Win32 message dispatch and OpenGL. */
#include <math.h>
#include <string.h>
#include "studiogizmo.h"
#define STUDIO_RAD (3.14159265358979323846/180.0)

BOOL StudioGizmoPlace(const StudioTransform *t,const OrbitCamera *camera,int width,int height,int mode,StudioGizmoFrame *frame)
{
    double eye[3],forward[3],depth=0;
    if (mode<STUDIO_TRANSLATE || mode>STUDIO_SCALE || !StudioRay(camera,width,height,width*.5,height*.5,eye,forward)) { return FALSE; }
    for (int k=0;k<3;k++) { depth+=(t->position[k]-eye[k])*forward[k]; frame->origin[k]=t->position[k]; }
    if (depth<=1e-8) { return FALSE; }
    frame->length=90*depth*2*tan(STUDIO_FOV*.5*STUDIO_RAD)/height;
    frame->mode=mode;
    if (mode==STUDIO_SCALE) { RotationEuler(&frame->axes,t->rotation); }
    else { frame->axes=(Rotation){{{1,0,0},{0,1,0},{0,0,1}}}; }
    return TRUE;
}

static BOOL StudioDragParameter(const StudioDrag *d,double x,double y,double *value)
{
    if (d->fallback) { *value=-y*d->length/90; return TRUE; }
    double eye[3],ray[3],numerator=0,denominator=0;
    if (!StudioRay(&d->camera,d->width,d->height,x,y,eye,ray)) { return FALSE; }
    for (int k=0;k<3;k++) { numerator+=(d->origin[k]-eye[k])*d->plane[k]; denominator+=ray[k]*d->plane[k]; }
    if (fabs(denominator)<1e-8 || numerator/denominator<=0) { return FALSE; }
    *value=0;
    for (int k=0;k<3;k++) { *value+=(eye[k]+ray[k]*numerator/denominator-d->origin[k])*d->direction[k]; }
    return TRUE;
}

static BOOL StudioDragAngle(const StudioDrag *d,double x,double y,double *angle)
{
    if (d->fallback)
    { *angle=((x-d->mouse[0])*d->tangent[0]+(y-d->mouse[1])*d->tangent[1])/(90*STUDIO_RAD); return TRUE; }
    double eye[3],ray[3],t; int a=(d->axis+1)%3,b=(d->axis+2)%3;
    if (!StudioRay(&d->camera,d->width,d->height,x,y,eye,ray) || fabs(ray[d->axis])<1e-8) { return FALSE; }
    t=(d->origin[d->axis]-eye[d->axis])/ray[d->axis];
    if (t<=0) { return FALSE; }
    *angle=atan2(eye[b]+ray[b]*t-d->origin[b],eye[a]+ray[a]*t-d->origin[a])/STUDIO_RAD;
    return TRUE;
}

BOOL StudioDragBegin(StudioDrag *d,const StudioTransform *t,const OrbitCamera *camera,int width,int height,
    double x,double y,const StudioGizmoFrame *frame,int axis,const double hit[3])
{
    if (axis<0 || axis>(frame->mode==STUDIO_SCALE ? 3 : 2) || !(frame->length>0)) { return FALSE; }
    memset(d,0,sizeof(*d)); d->before=*t; d->camera=*camera; d->width=width; d->height=height;
    d->mode=frame->mode; d->axis=axis; d->length=frame->length; d->mouse[0]=x; d->mouse[1]=y;
    memcpy(d->origin,frame->origin,sizeof(d->origin));
    if (axis==3) { return TRUE; }
    double eye[3],ray[3];
    if (!StudioRay(camera,width,height,x,y,eye,ray)) { return FALSE; }
    if (d->mode==STUDIO_ROTATE)
    {
        d->fallback=fabs(ray[axis])<.15;
        if (d->fallback)
        {
            int a=(axis+1)%3,b=(axis+2)%3;
            double tangent[3]={0},end[3],screen[2],screenend[2],length;
            tangent[a]=-(hit[b]-frame->origin[b]); tangent[b]=hit[a]-frame->origin[a];
            for (int k=0;k<3;k++) { end[k]=hit[k]+tangent[k]; }
            if (!StudioProject(camera,width,height,hit,screen) || !StudioProject(camera,width,height,end,screenend)) { return FALSE; }
            length=hypot(screenend[0]-screen[0],screenend[1]-screen[1]);
            if (length<1e-6) { d->tangent[0]=1; }
            else for (int k=0;k<2;k++) { d->tangent[k]=(screenend[k]-screen[k])/length; }
        }
        return StudioDragAngle(d,x,y,&d->lastangle);
    }
    double along=0,length=0;
    for (int k=0;k<3;k++) { d->direction[k]=frame->axes.m[k][axis]; along+=ray[k]*d->direction[k]; }
    for (int k=0;k<3;k++) { d->plane[k]=ray[k]-along*d->direction[k]; length+=d->plane[k]*d->plane[k]; }
    d->fallback=length<.01;
    return StudioDragParameter(d,x,y,&d->parameter);
}

BOOL StudioDragUpdate(StudioDrag *d,double x,double y,BOOL snap,StudioTransform *transform)
{
    StudioTransform next=d->before;
    if (d->mode==STUDIO_ROTATE)
    {
        double angle,delta; Rotation before,rotation,result;
        if (!StudioDragAngle(d,x,y,&angle)) { return FALSE; }
        delta=angle-d->lastangle; if (!d->fallback) { delta=remainder(delta,360); }
        d->angle+=delta; d->lastangle=angle;
        RotationEuler(&before,d->before.rotation); RotationAxis(&rotation,d->axis,snap ? RotationSnapDegrees(d->angle) : d->angle);
        RotationMultiply(&result,&rotation,&before); RotationDegrees(&result,next.rotation);
    }
    else if (d->mode==STUDIO_SCALE)
    {
        double factor,parameter,minimum=0,maximum=1e12;
        if (d->axis==3) { factor=exp(fmax(-40,fmin(40,((x-d->mouse[0])-(y-d->mouse[1]))/100))); }
        else
        {
            if (!StudioDragParameter(d,x,y,&parameter)) { return FALSE; }
            factor=1+(parameter-d->parameter)/d->length;
        }
        for (int k=0;k<3;k++) if (d->axis==3 || d->axis==k)
        { minimum=fmax(minimum,.0001/d->before.scale[k]); maximum=fmin(maximum,10000/d->before.scale[k]); }
        factor=fmax(minimum,fmin(maximum,factor));
        for (int k=0;k<3;k++) if (d->axis==3 || d->axis==k) { next.scale[k]=fmax(.0001,fmin(10000,d->before.scale[k]*factor)); }
    }
    else
    {
        double parameter;
        if (!StudioDragParameter(d,x,y,&parameter)) { return FALSE; }
        for (int k=0;k<3;k++) { next.position[k]+=(parameter-d->parameter)*d->direction[k]; }
    }
    if (!StudioTransformValid(&next)) { return FALSE; }
    *transform=next; return TRUE;
}
