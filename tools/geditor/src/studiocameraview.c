#include "studiocameraview.h"
#include <GL/gl.h>
#include <float.h>
#include <math.h>
#include <stdlib.h>
#include "resource.h"

BOOL StudioCameraModelLoad(StudioCameraModel *model,HINSTANCE instance)
{
    HRSRC resource=FindResource(instance,MAKEINTRESOURCE(IDR_MARKER_INTRO_CAMERA),RT_RCDATA);
    HGLOBAL loaded=resource ? LoadResource(instance,resource) : NULL; const char *why="";
    if (!loaded) { return FALSE; }
    model->vertices=GltfLoadGlbLitMesh(LockResource(loaded),SizeofResource(instance,resource),NULL,&model->count,&why);
    if (!model->vertices) { return FALSE; }
    /* The existing marker's lens points along +X. Render cameras look along -Z.
     * Keep its authored origin at the camera position and fit a unit sphere. */
    double radius=0;
    for (DWORD i=0;i<model->count*3;i++)
    {
        const BgVertex *v=&model->vertices[i]; radius=fmax(radius,sqrt(v->x*v->x+v->y*v->y+v->z*v->z));
    }
    if (!(radius>0)) { StudioCameraModelFree(model); return FALSE; }
    for (DWORD i=0;i<model->count*3;i++)
    {
        BgVertex *v=&model->vertices[i]; float x=v->x,nx=v->environment.normal[0];
        v->x=(float)(v->z*.8/radius); v->y=(float)(v->y*.8/radius); v->z=(float)(-x*.8/radius);
        v->environment.normal[0]=v->environment.normal[2]; v->environment.normal[2]=-nx;
    }
    return TRUE;
}

void StudioCameraModelFree(StudioCameraModel *model)
{ free(model->vertices); model->vertices=NULL; model->count=0; }

void StudioCameraModelDraw(const StudioCameraModel *model,const StudioCamera *camera,BOOL selected)
{
    StudioMatrix matrix; StudioMatrixBuild(&camera->transform,&matrix);
    glPushAttrib(GL_ENABLE_BIT|GL_CURRENT_BIT|GL_LINE_BIT|GL_POLYGON_BIT|GL_DEPTH_BUFFER_BIT);
    glDisable(GL_TEXTURE_2D); glDisable(GL_LIGHTING); glDisable(GL_BLEND); glDisable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST); glDepthMask(GL_TRUE); glDepthFunc(GL_LEQUAL);
    glPushMatrix(); glMultMatrixd(matrix.m);
    glEnable(GL_POLYGON_OFFSET_FILL); glPolygonOffset(1,1);
    glBegin(GL_TRIANGLES);
    for (DWORD i=0;i<model->count*3;i++)
    {
        const BgVertex *v=&model->vertices[i]; double normal[3]={0};
        for (int k=0;k<3;k++) for (int j=0;j<3;j++) { normal[k]+=matrix.normal[j*3+k]*v->environment.normal[j]; }
        float light=(float)(.35+.65*fmax(0,-.35*normal[0]+.8*normal[1]+.45*normal[2]));
        glColor3f(v->r*light/255,v->g*light/255,v->b*light/255); glVertex3f(v->x,v->y,v->z);
    }
    glEnd(); glDisable(GL_POLYGON_OFFSET_FILL);
    if (selected)
    {
        glColor3ub(242,194,70); glLineWidth(1); glPolygonMode(GL_FRONT_AND_BACK,GL_LINE); glBegin(GL_TRIANGLES);
        for (DWORD i=0;i<model->count*3;i++)
        { const BgVertex *v=&model->vertices[i]; glVertex3f(v->x,v->y,v->z); }
        glEnd();
    }
    glPopMatrix(); glPopAttrib();
}

BOOL StudioCameraModelPick(const StudioCameraModel *model,const StudioCamera *camera,
    const double origin[3],const double direction[3],double *distance)
{
    StudioMatrix matrix; StudioMatrixBuild(&camera->transform,&matrix); BOOL found=FALSE; *distance=DBL_MAX;
    for (DWORD face=0;face<model->count;face++)
    {
        double triangle[3][3],hit;
        for (int k=0;k<3;k++)
        {
            const BgVertex *v=&model->vertices[face*3+k]; double p[3]={v->x,v->y,v->z};
            StudioPoint(&matrix,p,triangle[k]);
        }
        if (StudioRayTriangle(origin,direction,triangle,&hit) && hit<*distance) { *distance=hit; found=TRUE; }
    }
    return found;
}
