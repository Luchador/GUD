#include <windows.h>
#include <GL/gl.h>
#include <math.h>
#include <float.h>
#include <stdlib.h>
#include <string.h>
#include "studiogizmo.h"
#include "resource.h"

void StudioGizmoFree(StudioGizmo *gizmo)
{
    for (int mode=0;mode<3;mode++) { free(gizmo->vertices[mode]); }
    memset(gizmo,0,sizeof(*gizmo));
}

BOOL StudioGizmoLoad(StudioGizmo *gizmo,HINSTANCE instance)
{
    const int resources[3]={IDR_GIZMO_ARROW,IDR_GIZMO_CYLINDER,IDR_GIZMO_SCALE};
    for (int mode=0;mode<3;mode++)
    {
        HRSRC resource=FindResource(instance,MAKEINTRESOURCE(resources[mode]),RT_RCDATA);
        HGLOBAL loaded=resource ? LoadResource(instance,resource) : NULL; const char *why=""; double extent=0;
        if (!loaded) { goto fail; }
        gizmo->vertices[mode]=GltfLoadGlbMesh(LockResource(loaded),SizeofResource(instance,resource),&gizmo->count[mode],&why);
        if (!gizmo->vertices[mode]) { goto fail; }
        for (DWORD i=0;i<gizmo->count[mode]*3;i++)
        {
            const BgVertex *v=&gizmo->vertices[mode][i];
            extent=fmax(extent,mode==STUDIO_ROTATE ? hypot(v->y,v->z) : v->x);
        }
        if (!(extent>0)) { goto fail; }
        for (DWORD i=0;i<gizmo->count[mode]*3;i++)
        { BgVertex *v=&gizmo->vertices[mode][i]; v->x/=extent; v->y/=extent; v->z/=extent; }
    }
    return TRUE;
fail:
    StudioGizmoFree(gizmo); return FALSE;
}

static DWORD StudioGizmoCount(const StudioGizmo *gizmo,const StudioGizmoFrame *frame,int axis)
{ return axis==3 ? 12 : gizmo->count[frame->mode]; }

static void StudioGizmoVertex(const StudioGizmo *gizmo,const StudioGizmoFrame *frame,int axis,DWORD index,double out[3])
{
    double p[3];
    if (axis==3)
    {
        static const unsigned char corners[36]={0,2,3,0,3,1,4,5,7,4,7,6,0,1,5,0,5,4,2,6,7,2,7,3,0,4,6,0,6,2,1,3,7,1,7,5};
        for (int k=0;k<3;k++) { p[k]=(corners[index]&(1<<k)) ? .09 : -.09; }
    }
    else
    {
        const BgVertex *v=&gizmo->vertices[frame->mode][index]; p[0]=v->x; p[1]=v->y; p[2]=v->z;
        if (axis==1) { p[0]=-v->y; p[1]=v->x; }
        if (axis==2) { p[0]=-v->z; p[2]=v->x; }
    }
    RotationVector(&frame->axes,p,p);
    for (int k=0;k<3;k++) { out[k]=frame->origin[k]+p[k]*frame->length; }
}

void StudioGizmoDraw(const StudioGizmo *gizmo,const StudioGizmoFrame *frame,int highlight)
{
    glPushAttrib(GL_CURRENT_BIT|GL_ENABLE_BIT|GL_DEPTH_BUFFER_BIT|GL_POLYGON_BIT);
    glDisable(GL_TEXTURE_2D); glDisable(GL_LIGHTING); glDisable(GL_CULL_FACE); glDisable(GL_BLEND);
    glDepthMask(GL_TRUE); glClear(GL_DEPTH_BUFFER_BIT); glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LESS);
    glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
    for (int axis=0;axis<(frame->mode==STUDIO_SCALE ? 4 : 3);axis++)
    {
        if (axis==highlight) { glColor3ub(255,205,0); }
        else if (axis==3) { glColor3ub(255,255,255); }
        else if (!axis) { glColor3ub(240,40,40); }
        else if (axis==1) { glColor3ub(40,220,60); }
        else { glColor3ub(40,100,255); }
        glBegin(GL_TRIANGLES);
        for (DWORD vertex=0;vertex<StudioGizmoCount(gizmo,frame,axis)*3;vertex++)
        { double p[3]; StudioGizmoVertex(gizmo,frame,axis,vertex,p); glVertex3dv(p); }
        glEnd();
    }
    glPopAttrib();
}

int StudioGizmoPick(const StudioGizmo *gizmo,const StudioGizmoFrame *frame,const double origin[3],const double direction[3],double hit[3])
{
    double nearest=DBL_MAX; int result=-1;
    for (int axis=0;axis<(frame->mode==STUDIO_SCALE ? 4 : 3);axis++)
        for (DWORD triangle=0;triangle<StudioGizmoCount(gizmo,frame,axis);triangle++)
        {
            double v[3][3],distance;
            for (int corner=0;corner<3;corner++) { StudioGizmoVertex(gizmo,frame,axis,triangle*3+corner,v[corner]); }
            if (StudioRayTriangle(origin,direction,v,&distance) && distance<nearest) { nearest=distance; result=axis; }
        }
    if (result>=0) for (int k=0;k<3;k++) { hit[k]=origin[k]+direction[k]*nearest; }
    return result;
}
