#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "uvprojection.h"
#define N 8
static UVProjectionVertex vertices[100];
static UVProjectionFace faces[100];
static unsigned char seams[100];
static double uv[100][3][2];
static const char *why;

static void Fixture(int axis)
{
    const double angles[N]={0,0.3,1.2,2,3,4,5,5.7};
    memset(vertices,0,sizeof(vertices)); memset(seams,0,sizeof(seams));
    for (int ring=0;ring<3;ring++) for (int i=0;i<N;i++)
    {
        double p[3]={160*cos(angles[i]),ring*250,90*sin(angles[i])};
        if (axis==1) { double t=p[0]; p[0]=p[1]; p[1]=t; }
        if (axis==3) { double t=p[2]; p[2]=p[1]; p[1]=t; }
        memcpy(vertices[ring*N+i].position,p,sizeof(p));
    }
    for (int band=0;band<2;band++) for (int i=0;i<N;i++)
    {
        int a=band*N+i,b=band*N+(i+1)%N,f=band*N*2+i*2;
        faces[f]=(UVProjectionFace){{a,a+N,b}};
        faces[f+1]=(UVProjectionFace){{b,a+N,b+N}};
    }
}
static double Side(int i)
{
    const double *a=vertices[i].position,*b=vertices[(i+1)%N].position;
    return sqrt(pow(a[0]-b[0],2)+pow(a[1]-b[1],2)+pow(a[2]-b[2],2));
}
static int Marked(int a,int b)
{ return (a==3 && b==N+3) || (b==3 && a==N+3) || (a==N+3 && b==2*N+3) || (b==N+3 && a==2*N+3); }
static void Check(void)
{
    double perimeter=0; for (int i=0;i<N;i++) { perimeter+=Side(i); }
    for (int f=0;f<N*4;f++)
    {
        double lo=1e10,hi=-1e10;
        for (int c=0;c<3;c++)
        {
            lo=fmin(lo,uv[f][c][0]); hi=fmax(hi,uv[f][c][0]);
            assert(fabs(uv[f][c][1]-(faces[f].vertices[c]/N)*250)<1e-7);
        }
        assert(fabs(hi-lo-Side((f/2)%N))<1e-7);
        for (int g=f+1;g<N*4;g++) for (int c=0;c<3;c++) for (int d=0;d<3;d++)
        {
            int a=faces[f].vertices[c],b=faces[f].vertices[(c+1)%3];
            int x=faces[g].vertices[d],y=faces[g].vertices[(d+1)%3];
            if (!((a==x && b==y)||(a==y && b==x))) { continue; }
            int gc=a==x?d:(d+1)%3;
            double du=fabs(uv[f][c][0]-uv[g][gc][0]);
            assert(fabs(du-(Marked(a,b)?perimeter:0))<1e-7);
        }
    }
}
int main(void)
{
    for (int axis=1;axis<=3;axis++)
    {
        Fixture(axis); seams[6]=1; seams[2*N+6]=1;
        assert(UVProjectionCylinder(vertices,3*N,faces,4*N,seams,axis,uv,&why)); Check();
        assert(UVProjectionCylinder(vertices,3*N,faces,4*N,seams,0,uv,&why)); Check();
        /* Incomplete, multiple, branching and rim cuts must not be ignored. */
        seams[2*N+6]=0; assert(!UVProjectionCylinder(vertices,3*N,faces,4*N,seams,axis,uv,&why));
        seams[2*N+6]=1; seams[10]=1; seams[2*N+10]=1;
        assert(!UVProjectionCylinder(vertices,3*N,faces,4*N,seams,axis,uv,&why));
        seams[10]=seams[2*N+10]=0; seams[6]|=2;
        assert(!UVProjectionCylinder(vertices,3*N,faces,4*N,seams,axis,uv,&why));
        memset(seams,0,sizeof(seams)); seams[0]=4;
        assert(!UVProjectionCylinder(vertices,3*N,faces,4*N,seams,axis,uv,&why));
        assert(UVProjectionCylinder(vertices,3*N,faces,4*N,NULL,axis,uv,&why));
    }
    /* A diagonal two-edge seam is legal and must remain the actual cut. */
    Fixture(2); seams[6]=2; seams[2*N+6]=1;
    assert(UVProjectionCylinder(vertices,3*N,faces,4*N,seams,2,uv,&why));
    Fixture(2); faces[4*N]=faces[0];
    assert(!UVProjectionCylinder(vertices,3*N,faces,4*N+1,NULL,2,uv,&why));
    /* Add a bottom cap: only one open rim remains. */
    Fixture(2); vertices[3*N]=(UVProjectionVertex){{0,0,0},{0,0}};
    for (int i=0;i<N;i++) { faces[4*N+i]=(UVProjectionFace){{i,(i+1)%N,3*N}}; }
    assert(!UVProjectionCylinder(vertices,3*N+1,faces,5*N,NULL,2,uv,&why));
    puts("PASS: explicit seams on all axes, irregular elliptical perimeter distances, multi-ring continuity, diagonal cuts, and rejection of incomplete/branched/multiple/rim seams, caps and non-manifold geometry.");
}
