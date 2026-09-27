#include <math.h>
#include <float.h>
#include "studiomath.h"

BOOL StudioBounds(const StudioScene *scene, int selected, double lower[3], double upper[3])
{
    BOOL found=FALSE;
    if (!scene) { return FALSE; }
    for (DWORD i=0;i<scene->count;i++)
    {
        const StudioInstance *o=&scene->objects[i];
        if (!o->asset || (selected>=0 && (DWORD)selected!=i)) { continue; }
        for (int k=0;k<3;k++)
        {
            double a=o->asset->lower[k]+o->position[k], b=o->asset->upper[k]+o->position[k];
            if (!found || a<lower[k]) { lower[k]=a; } if (!found || b>upper[k]) { upper[k]=b; }
        }
        found=TRUE;
    }
    return found;
}

BOOL StudioRay(const OrbitCamera *camera, int width, int height, double x, double y, double origin[3], double direction[3])
{
    if (width<1 || height<1) { return FALSE; }
    double yaw=camera->yaw*3.14159265358979323846/180, pitch=camera->pitch*3.14159265358979323846/180;
    double scale=tan(STUDIO_FOV*3.14159265358979323846/360);
    double dx=(2*x/width-1)*scale*width/height, dy=(1-2*y/height)*scale;
    double right[3]={cos(yaw),0,-sin(yaw)}, up[3]={sin(yaw)*sin(pitch),cos(pitch),cos(yaw)*sin(pitch)};
    double forward[3]={-sin(yaw)*cos(pitch),sin(pitch),-cos(yaw)*cos(pitch)}, length=0;
    OrbitCameraPosition(camera,origin);
    for (int k=0;k<3;k++) { direction[k]=forward[k]+right[k]*dx+up[k]*dy; length+=direction[k]*direction[k]; }
    length=sqrt(length); for (int k=0;k<3;k++) { direction[k]/=length; }
    return TRUE;
}

int StudioPick(const StudioScene *scene, const double origin[3], const double direction[3], int *material)
{
    double nearest=DBL_MAX; int result=-1; *material=-1;
    if (!scene) { return -1; }
    for (DWORD i=0;i<scene->count;i++)
    {
        const StudioInstance *o=&scene->objects[i]; if (!o->asset) { continue; }
        for (DWORD f=0;f<o->asset->mesh.count;f++)
        {
            const BgVertex *v=o->asset->mesh.vertices+f*3;
            double a[3]={v[1].x-v[0].x,v[1].y-v[0].y,v[1].z-v[0].z};
            double b[3]={v[2].x-v[0].x,v[2].y-v[0].y,v[2].z-v[0].z};
            double h[3]={direction[1]*b[2]-direction[2]*b[1],direction[2]*b[0]-direction[0]*b[2],direction[0]*b[1]-direction[1]*b[0]};
            double determinant=a[0]*h[0]+a[1]*h[1]+a[2]*h[2];
            if (fabs(determinant)<1e-12) { continue; }
            double s[3]={origin[0]-o->position[0]-v[0].x,origin[1]-o->position[1]-v[0].y,origin[2]-o->position[2]-v[0].z};
            double u=(s[0]*h[0]+s[1]*h[1]+s[2]*h[2])/determinant;
            if (u<0 || u>1) { continue; }
            double q[3]={s[1]*a[2]-s[2]*a[1],s[2]*a[0]-s[0]*a[2],s[0]*a[1]-s[1]*a[0]};
            double t=(b[0]*q[0]+b[1]*q[1]+b[2]*q[2])/determinant;
            double w=(direction[0]*q[0]+direction[1]*q[1]+direction[2]*q[2])/determinant;
            if (w<0 || u+w>1 || t<=0 || t>=nearest) { continue; }
            nearest=t; result=(int)i; *material=(int)o->asset->mesh.materials.faces[f].slot;
        }
    }
    return result;
}

void StudioShade(const StudioMaterial *m, const BgVertex *vertex, const double position[3],
    const double eye[3], float diffuse[3], float specular[3])
{
    const double light[3]={0.348742916,0.813733471,0.464990554}; /* unit directional preview light */
    double normal[3], view[3], length=0, viewlength=0, nl=0, rv=0;
    double color[3]={vertex->r/255.0,vertex->g/255.0,vertex->b/255.0};
    double p[3]={vertex->x+position[0],vertex->y+position[1],vertex->z+position[2]};
    for (int k=0;k<3;k++)
    { normal[k]=vertex->environment.normal[k]; length+=normal[k]*normal[k]; view[k]=eye[k]-p[k]; viewlength+=view[k]*view[k]; }
    length=sqrt(length); viewlength=sqrt(viewlength);
    for (int k=0;k<3;k++) { normal[k]=length>1e-12 ? normal[k]/length : (k==1); view[k]=viewlength>1e-12 ? view[k]/viewlength : 0; nl+=normal[k]*light[k]; }
    if (nl>0) for (int k=0;k<3;k++) { rv+=(2*normal[k]*nl-light[k])*view[k]; }
    double highlight=nl>0 ? m->intensity*pow(fmax(0,fmin(1,rv)),m->shininess) : 0;
    for (int k=0;k<3;k++) { diffuse[k]=(float)(m->base[k]*color[k]*(0.2+0.8*fmax(0,nl))); specular[k]=(float)(m->specular[k]*highlight); }
}
