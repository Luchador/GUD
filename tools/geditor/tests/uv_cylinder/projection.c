#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "uvprojection.h"
#define PI 3.14159265358979323846

static void Cylinder(int sides, double height, int orientation, int caps, int duplicated, int rounded)
{
    UVProjectionVertex vertices[256] = {0}, original[256];
    UVProjectionFace faces[128];
    double uv[128][3][2], second[128][3][2];
    int nv=2*sides+2, nf=0;
    for (int i=0; i<nv; i++)
    {
        double angle=2*PI*(i%sides)/sides+0.137;
        double x=i>=2*sides ? 0 : 100*cos(angle);
        double y=(i>=2*sides ? i-2*sides : i/sides)*height;
        double z=i>=2*sides ? 0 : 100*sin(angle);
        double p[3]={x,y,z};
        if (orientation==1) { p[0]=y; p[1]=z; p[2]=x; }
        if (orientation==2)
        {
            double a=0.631,b=0.473, xx=cos(a)*x-sin(a)*y, yy=sin(a)*x+cos(a)*y;
            p[0]=xx; p[1]=cos(b)*yy-sin(b)*z; p[2]=sin(b)*yy+cos(b)*z;
        }
        for (int k=0; k<3; k++) { vertices[i].position[k]=rounded ? round(p[k])+17000 : p[k]+17000; }
    }
    for (int i=0; i<sides; i++)
    {
        int j=(i+1)%sides;
        faces[nf++]=(UVProjectionFace){{i,i+sides,j}};
        faces[nf++]=(UVProjectionFace){{j,i+sides,j+sides}};
    }
    if (caps) for (int i=0; i<sides; i++)
    {
        int j=(i+1)%sides;
        faces[nf++]=(UVProjectionFace){{i,j,2*sides}};
        faces[nf++]=(UVProjectionFace){{i+sides,2*sides+1,j+sides}};
    }
    else { nv-=2; }
    if (duplicated)
    {
        /* Native cylinder vertices are often already split by material, UV or color. */
        for (int f=0; f<nf; f+=3) for (int c=0; c<3; c++)
        { vertices[nv]=vertices[faces[f].vertices[c]]; faces[f].vertices[c]=nv++; }
    }
    memcpy(original,vertices,sizeof(vertices));
    const char *why="";
    if (!UVProjectionCylinder(vertices,nv,faces,nf,NULL,0,uv,&why))
    { fprintf(stderr,"%d sides, height %g, rotation %d, caps %d: %s\n",sides,height,orientation,caps,why); assert(0); }
    assert(!memcmp(vertices,original,sizeof(vertices)));
    double perimeter=2*sides*100*sin(PI/sides);
    double minimum=1e9,maximum=-1e9;
    for (int f=0; f<nf; f++)
    {
        double lo=1e9,hi=-1e9,vlo=1e9,vhi=-1e9;
        for (int c=0; c<3; c++)
        {
            double u=uv[f][c][0],v=uv[f][c][1];
            if (!(isfinite(u) && isfinite(v) && u>=-0.02 && u<=perimeter+5 && v>=-0.02 && v<=height+3))
            { fprintf(stderr,"Out of bounds %d sides height %g rotation %d caps %d rounded %d face %d: (%g,%g)\n",sides,height,orientation,caps,rounded,f,u,v); assert(0); }
            lo=fmin(lo,u); hi=fmax(hi,u); vlo=fmin(vlo,v); vhi=fmax(vhi,v);
        }
        if (f<2*sides)
        {
            if (fabs(hi-lo-perimeter/sides)>(rounded ? 3 : 1e-7) || fabs(vhi-vlo-height)>(rounded ? 3 : 1e-7))
            { fprintf(stderr,"Bad side %d/%d, height %g rotation %d caps %d rounded %d: du=%g dv=%g\n",f,sides,height,orientation,caps,rounded,hi-lo,vhi-vlo); assert(0); }
            minimum=fmin(minimum,lo); maximum=fmax(maximum,hi);
        }
    }
    assert(fabs(maximum-minimum-perimeter)<(rounded ? 5 : 1e-7));
    /* Deterministic and independent of face ordering/winding. */
    for (int f=0; f<nf; f++) { int v=faces[f].vertices[0]; faces[f].vertices[0]=faces[f].vertices[2]; faces[f].vertices[2]=v; }
    assert(UVProjectionCylinder(vertices,nv,faces,nf,NULL,0,second,&why));
    for (int f=0; f<2*sides; f++) for (int c=0; c<3; c++) for (int k=0; k<2; k++)
    { assert(fabs(uv[f][c][k]-second[f][2-c][k])<1e-8); }
}
int main(void)
{
    int count=0;
    for (int sides=6; sides<=16; sides++) for (int h=0; h<3; h++)
    for (int orientation=0; orientation<3; orientation++) for (int caps=0; caps<1; caps++)
    for (int duplicated=0; duplicated<2; duplicated++) for (int rounded=0; rounded<2; rounded++)
    { Cylinder(sides,(double[]){20,141.421356,600}[h],orientation,caps,duplicated,rounded); count++; }
    UVProjectionVertex v[6]={0}; UVProjectionFace f[6]={0}; double uv[6][3][2]; const char *why="";
    assert(!UVProjectionCylinder(v,6,f,6,NULL,0,uv,&why) && why[0]);
    v[1].position[1]=NAN; assert(!UVProjectionCylinder(v,6,f,6,NULL,0,uv,&why));
    v[1].position[1]=1; f[0].vertices[0]=6; assert(!UVProjectionCylinder(v,6,f,6,NULL,0,uv,&why));
    printf("PASS: %d cylinder cases (6-16 sides; short/tall; rotated; uncapped; split native vertices; rounded positions), seam spans and invalid geometry.\n",count);
}
