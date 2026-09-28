#include <math.h>
#include <float.h>
#include <string.h>
#include "studiomath.h"

void StudioMatrixBuild(const StudioTransform *t, StudioMatrix *matrix)
{
    Rotation rotation; RotationEuler(&rotation,t->rotation); memset(matrix,0,sizeof(*matrix));
    for (int row=0;row<3;row++) for (int col=0;col<3;col++)
    {
        matrix->m[col*4+row]=rotation.m[row][col]*t->scale[col];
        matrix->normal[col*3+row]=rotation.m[row][col]/t->scale[col];
    }
    for (int k=0;k<3;k++) { matrix->m[12+k]=t->position[k]; }
    matrix->m[15]=1;
}

void StudioPoint(const StudioMatrix *m,const double in[3],double out[3])
{
    double result[3];
    for (int k=0;k<3;k++) { result[k]=m->m[k]*in[0]+m->m[4+k]*in[1]+m->m[8+k]*in[2]+m->m[12+k]; }
    memcpy(out,result,sizeof(result));
}

BOOL StudioBounds(const StudioScene *scene, int selected, double lower[3], double upper[3])
{
    BOOL found=FALSE;
    if (!scene) { return FALSE; }
    for (DWORD i=0;i<scene->count;i++)
    {
        const StudioInstance *o=&scene->objects[i];
        if (!o->asset || (selected>=0 && (DWORD)selected!=i)) { continue; }
        StudioMatrix matrix; StudioMatrixBuild(&o->transform,&matrix);
        for (int corner=0;corner<8;corner++)
        {
            double p[3];
            for (int k=0;k<3;k++) { p[k]=(corner&(1<<k)) ? o->asset->upper[k] : o->asset->lower[k]; }
            StudioPoint(&matrix,p,p);
            for (int k=0;k<3;k++)
            { if (!found || p[k]<lower[k]) { lower[k]=p[k]; } if (!found || p[k]>upper[k]) { upper[k]=p[k]; } }
            found=TRUE;
        }
    }
    return found;
}

BOOL StudioViewBounds(const StudioScene *scene,int selected,double lower[3],double upper[3])
{
    if (!scene) { return FALSE; }
    BOOL found=selected>=-1 && StudioBounds(scene,selected,lower,upper);
    int slot=StudioSceneLightIndex(scene,selected);
    for (int i=0;i<STUDIO_LIGHT_COUNT;i++)
    {
        if (!scene->lights[i].enabled || (selected!=-1 && slot!=i)) { continue; }
        for (int k=0;k<3;k++)
        {
            double lo=scene->lights[i].position[k]-1,hi=scene->lights[i].position[k]+1;
            if (!found || lo<lower[k]) { lower[k]=lo; } if (!found || hi>upper[k]) { upper[k]=hi; }
        }
        found=TRUE;
    }
    return found;
}

static void StudioCameraBasis(const OrbitCamera *camera,double right[3],double up[3],double forward[3])
{
    double yaw=camera->yaw*3.14159265358979323846/180, pitch=camera->pitch*3.14159265358979323846/180;
    right[0]=cos(yaw); right[1]=0; right[2]=-sin(yaw);
    up[0]=sin(yaw)*sin(pitch); up[1]=cos(pitch); up[2]=cos(yaw)*sin(pitch);
    forward[0]=-sin(yaw)*cos(pitch); forward[1]=sin(pitch); forward[2]=-cos(yaw)*cos(pitch);
}

BOOL StudioRay(const OrbitCamera *camera, int width, int height, double x, double y, double origin[3], double direction[3])
{
    if (width<1 || height<1) { return FALSE; }
    double scale=tan(STUDIO_FOV*3.14159265358979323846/360);
    double dx=(2*x/width-1)*scale*width/height, dy=(1-2*y/height)*scale;
    double right[3],up[3],forward[3],length=0;
    StudioCameraBasis(camera,right,up,forward); OrbitCameraPosition(camera,origin);
    for (int k=0;k<3;k++) { direction[k]=forward[k]+right[k]*dx+up[k]*dy; length+=direction[k]*direction[k]; }
    length=sqrt(length); for (int k=0;k<3;k++) { direction[k]/=length; }
    return TRUE;
}

BOOL StudioProject(const OrbitCamera *camera,int width,int height,const double world[3],double screen[2])
{
    double right[3],up[3],forward[3],eye[3],x=0,y=0,z=0;
    if (width<1 || height<1) { return FALSE; }
    StudioCameraBasis(camera,right,up,forward); OrbitCameraPosition(camera,eye);
    for (int k=0;k<3;k++) { x+=(world[k]-eye[k])*right[k]; y+=(world[k]-eye[k])*up[k]; z+=(world[k]-eye[k])*forward[k]; }
    if (z<=1e-9) { return FALSE; }
    double scale=height/(2*z*tan(STUDIO_FOV*3.14159265358979323846/360));
    screen[0]=width*.5+x*scale; screen[1]=height*.5-y*scale; return TRUE;
}

BOOL StudioRayTriangle(const double origin[3],const double direction[3],const double v[3][3],double *distance)
{
    double a[3],b[3],s[3],h[3],q[3];
    for (int k=0;k<3;k++) { a[k]=v[1][k]-v[0][k]; b[k]=v[2][k]-v[0][k]; s[k]=origin[k]-v[0][k]; }
    for (int k=0;k<3;k++) { int j=(k+1)%3,l=(k+2)%3; h[k]=direction[j]*b[l]-direction[l]*b[j]; q[k]=s[j]*a[l]-s[l]*a[j]; }
    double determinant=0,u=0,w=0,t=0;
    for (int k=0;k<3;k++) { determinant+=a[k]*h[k]; u+=s[k]*h[k]; w+=direction[k]*q[k]; t+=b[k]*q[k]; }
    if (fabs(determinant)<1e-14) { return FALSE; }
    u/=determinant; w/=determinant; t/=determinant;
    if (u<0 || w<0 || u+w>1 || t<=0) { return FALSE; }
    *distance=t; return TRUE;
}

int StudioPick(const StudioScene *scene, const double origin[3], const double direction[3], int *material)
{
    double nearest=DBL_MAX; int result=-1; *material=-1;
    if (!scene) { return -1; }
    for (DWORD i=0;i<scene->count;i++)
    {
        const StudioInstance *o=&scene->objects[i]; if (!o->asset) { continue; }
        /* Inverse-transform the ray once per instance. Do not normalize: its
         * parameter must remain a world-space distance for nearest-hit ordering. */
        StudioMatrix matrix; double localorigin[3]={0},localdirection[3]={0}; StudioMatrixBuild(&o->transform,&matrix);
        for (int k=0;k<3;k++) for (int j=0;j<3;j++)
        { localorigin[k]+=(origin[j]-o->transform.position[j])*matrix.normal[k*3+j]; localdirection[k]+=direction[j]*matrix.normal[k*3+j]; }
        for (DWORD f=0;f<o->asset->mesh.count;f++)
        {
            const BgVertex *v=o->asset->mesh.vertices+f*3; double p[3][3],distance;
            for (int k=0;k<3;k++) { p[k][0]=v[k].x; p[k][1]=v[k].y; p[k][2]=v[k].z; }
            if (StudioRayTriangle(localorigin,localdirection,p,&distance) && distance<nearest)
            { nearest=distance; result=(int)i; *material=(int)o->asset->mesh.materials.faces[f].slot; }
        }
    }
    return result;
}

/* direction points from the shaded surface toward the light. Cone angles are
 * measured from the spotlight's outward axis, not across the full cone. */
double StudioLightSample(const StudioLight *light, BOOL spotlight, const double point[3], double direction[3])
{
    double distance=0;
    for (int k=0;k<3;k++) { direction[k]=light->position[k]-point[k]; distance+=direction[k]*direction[k]; }
    distance=sqrt(distance);
    if (!light->enabled || light->intensity<=0 || distance<=1e-12) { return 0; }
    for (int k=0;k<3;k++) { direction[k]/=distance; }
    if (!spotlight)
    {
        double falloff=fmax(0,1-distance/light->radius);
        return light->intensity*falloff*falloff;
    }
    double length=0,alignment=0;
    for (int k=0;k<3;k++) { length+=light->direction[k]*light->direction[k]; alignment-=light->direction[k]*direction[k]; }
    if (length<=1e-24) { return 0; }
    alignment/=sqrt(length);
    double inner=cos(light->inner*3.14159265358979323846/180),outer=cos(light->outer*3.14159265358979323846/180);
    if (alignment<outer) { return 0; }
    if (alignment>=inner || inner==outer) { return light->intensity; }
    double blend=(alignment-outer)/(inner-outer);
    return light->intensity*blend*blend*(3-2*blend);
}

static void StudioIlluminate(const StudioMaterial *m,const double normal[3],const double view[3],
    const double light[3],const float color[3],double diffusepower,double specularpower,
    double illumination[3],double highlights[3])
{
    double nl=0,rv=0;
    for (int k=0;k<3;k++) { nl+=normal[k]*light[k]; }
    if (nl<=0) { return; }
    for (int k=0;k<3;k++) { rv+=(2*normal[k]*nl-light[k])*view[k]; }
    double highlight=m->intensity*pow(fmax(0,fmin(1,rv)),m->shininess)*specularpower;
    for (int k=0;k<3;k++) { illumination[k]+=color[k]*nl*diffusepower; highlights[k]+=color[k]*highlight; }
}

void StudioShade(const StudioScene *scene,const StudioMaterial *m, const BgVertex *vertex, const StudioMatrix *matrix,
    const double eye[3], float diffuse[3], float specular[3])
{
    double normal[3]={0},view[3],length=0,viewlength=0;
    double p[3]={vertex->x,vertex->y,vertex->z};
    double illumination[3]={.2,.2,.2},highlights[3]={0}; BOOL authored=FALSE;
    StudioPoint(matrix,p,p);
    for (int k=0;k<3;k++)
    {
        for (int j=0;j<3;j++) { normal[k]+=matrix->normal[j*3+k]*vertex->environment.normal[j]; }
        length+=normal[k]*normal[k]; view[k]=eye[k]-p[k]; viewlength+=view[k]*view[k];
    }
    length=sqrt(length); viewlength=sqrt(viewlength);
    for (int k=0;k<3;k++) { normal[k]=length>1e-12 ? normal[k]/length : (k==1); view[k]=viewlength>1e-12 ? view[k]/viewlength : 0; }
    if (scene) for (int i=0;i<STUDIO_LIGHT_COUNT;i++)
    {
        const StudioLight *light=&scene->lights[i]; if (!light->enabled) { continue; } authored=TRUE;
        double direction[3],power=StudioLightSample(light,i==0,p,direction);
        if (power>0) { StudioIlluminate(m,normal,view,direction,light->color,power,power,illumination,highlights); }
    }
    if (!authored)
    {
        const double direction[3]={0.348742916,0.813733471,0.464990554}; const float white[3]={1,1,1};
        StudioIlluminate(m,normal,view,direction,white,.8,1,illumination,highlights);
    }
    double color[3]={vertex->r/255.0,vertex->g/255.0,vertex->b/255.0};
    for (int k=0;k<3;k++)
    {
        diffuse[k]=(float)fmin(1,m->base[k]*color[k]*illumination[k]);
        specular[k]=(float)fmin(1,m->specular[k]*highlights[k]);
    }
}
