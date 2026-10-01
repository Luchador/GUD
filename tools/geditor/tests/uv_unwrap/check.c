#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "uvprojection.h"

static int allocations = -1;
static void *TestCalloc(size_t n, size_t size)
{ if (!allocations) { return NULL; } if (allocations > 0) { allocations--; } return calloc(n, size); }
#define calloc TestCalloc
#include "uvunwrap.c"
#undef calloc

static double Area3(const UVProjectionVertex *v, const UVProjectionFace *f)
{
    double a[3], b[3], c[3];
    for (int k=0;k<3;k++) { a[k]=v[f->vertices[1]].position[k]-v[f->vertices[0]].position[k]; b[k]=v[f->vertices[2]].position[k]-v[f->vertices[0]].position[k]; }
    c[0]=a[1]*b[2]-a[2]*b[1]; c[1]=a[2]*b[0]-a[0]*b[2]; c[2]=a[0]*b[1]-a[1]*b[0];
    return hypot(hypot(c[0],c[1]),c[2])/2;
}
static void Check(const UVProjectionVertex *v,int n,const UVProjectionFace *f,int count,
    const unsigned char *seams,double (*uv)[3][2],int isometric)
{
    const char *why="";
    if (!UVProjectionUnwrap(v,n,f,count,seams,uv,&why)) { fprintf(stderr,"unwrap failed: %s\n",why); abort(); }
    double worldarea=0, mappedarea=0;
    for (int i=0;i<count;i++)
    {
        double area=fabs(UnwrapSide(uv[i][0],uv[i][1],uv[i][2]))/2;
        assert(area>0 && isfinite(area)); mappedarea+=area; worldarea+=Area3(v,&f[i]);
        if (isometric) for (int c=0;c<3;c++)
        {
            double world=UnwrapDistance(v[f[i].vertices[c]].position,v[f[i].vertices[(c+1)%3]].position);
            double mapped=hypot(uv[i][c][0]-uv[i][(c+1)%3][0],uv[i][c][1]-uv[i][(c+1)%3][1]);
            assert(fabs(mapped/world-1)<1e-6);
        }
    }
    assert(fabs(mappedarea/worldarea-1)<1e-8);
}
static void Shared(const UVProjectionFace *faces,int count,double (*uv)[3][2])
{
    for (int f=0;f<count;f++) for (int g=0;g<count;g++) for (int c=0;c<3;c++) for (int k=0;k<3;k++)
    { if (faces[f].vertices[c]==faces[g].vertices[k]) { assert(!memcmp(uv[f][c],uv[g][k],sizeof(uv[f][c]))); } }
}
static void Grid(void)
{
    enum {N=21, V=N*N, F=(N-1)*(N-1)*2};
    UVProjectionVertex v[V+1]={0},before[V+1]; UVProjectionFace faces[F]; double uv[F][3][2];
    for (int y=0;y<N;y++) for (int x=0;x<N;x++)
    { v[y*N+x].position[0]=1000000+x*20; v[y*N+x].position[1]=-1000000+y*30; v[y*N+x].position[2]=50; }
    v[V].position[0]=NAN; /* Unused input is ignored. */
    int f=0;
    for (int y=0;y<N-1;y++) for (int x=0;x<N-1;x++)
    { int a=y*N+x; faces[f++]=(UVProjectionFace){{a,a+1,a+N+1}}; faces[f++]=(UVProjectionFace){{a,a+N+1,a+N}}; }
    memcpy(before,v,sizeof(v)); Check(v,V+1,faces,F,NULL,uv,1); assert(!memcmp(before,v,sizeof(v)));
    for (int i=0;i<F;i+=3) { int a=faces[i].vertices[0]; faces[i].vertices[0]=faces[i].vertices[1]; faces[i].vertices[1]=a; }
    Check(v,V+1,faces,F,NULL,uv,1); Shared(faces,F,uv);
    puts("PASS: 800-face planar grid is isometric, shared UVs exact, inconsistent authored winding supported and inputs untouched.");
}
static void Arch(void)
{
    enum {N=17, V=N*4, F=(N-1)*6};
    UVProjectionVertex v[V]={0},split[F*3]; UVProjectionFace faces[F],splitfaces[F];
    double uv[F][3][2],again[F][3][2]; const double profile[4][2]={{0,0},{12,16},{28,16},{40,0}};
    int f=0;
    for (int i=0;i<N;i++) for (int p=0;p<4;p++)
    {
        double angle=i*3.14159265358979323846/(N-1), radius=200+profile[p][0];
        v[i*4+p].position[0]=cos(angle)*radius; v[i*4+p].position[1]=sin(angle)*radius; v[i*4+p].position[2]=profile[p][1];
    }
    for (int i=0;i<N-1;i++) for (int p=0;p<3;p++)
    { int a=i*4+p; faces[f++]=(UVProjectionFace){{a,a+4,a+5}}; faces[f++]=(UVProjectionFace){{a,a+5,a+1}}; }
    Check(v,V,faces,F,NULL,uv,0); Shared(faces,F,uv);
    for (int i=0;i<F;i++) for (int c=0;c<3;c++)
    { split[i*3+c]=v[faces[i].vertices[c]]; split[i*3+c].uv[0]=i; splitfaces[i].vertices[c]=i*3+c; }
    Check(split,F*3,splitfaces,F,NULL,again,0); assert(!memcmp(uv,again,sizeof(uv)));
    /* Stretch the arch into a developable bent strip: three flat panels
       joined at right angles should unfold without changing any edge length. */
    for (int i=0;i<N;i++) for (int p=0;p<4;p++)
    { v[i*4+p].position[0]=(p==0||p==3)?0:40; v[i*4+p].position[1]=p<2?0:40; v[i*4+p].position[2]=i*10; }
    Check(v,V,faces,F,NULL,uv,1); Shared(faces,F,uv);
    const char *why=""; int success=0;
    for (int budget=0;budget<30 && !success;budget++)
    {
        memset(again,0x55,sizeof(again)); double before[F][3][2]; memcpy(before,again,sizeof(before));
        allocations=budget; success=UVProjectionUnwrap(v,V,faces,F,NULL,again,&why); allocations=-1;
        if (!success) { assert(why[0] && !memcmp(before,again,sizeof(before))); }
    }
    assert(success);
    puts("PASS: 96-face three-sided arch, folded strip, native attribute splits, shared edges and atomic allocation failures.");
}
static void Cuts(void)
{
    UVProjectionVertex v[8]={0}; UVProjectionFace faces[8]; unsigned char seams[8]={0}; double uv[8][3][2];
    const double corners[4][2]={{0,0},{40,0},{40,40},{0,40}};
    for (int i=0;i<8;i++) { v[i].position[0]=corners[i%4][0]; v[i].position[1]=corners[i%4][1]; v[i].position[2]=i<4?0:100; }
    for (int i=0;i<4;i++)
    { int j=(i+1)%4; faces[i*2]=(UVProjectionFace){{i,j,j+4}}; faces[i*2+1]=(UVProjectionFace){{i,j+4,i+4}}; }
    const char *why="";
    memset(uv,0x55,sizeof(uv)); double before[8][3][2]; memcpy(before,uv,sizeof(uv));
    assert(!UVProjectionUnwrap(v,8,faces,8,NULL,uv,&why) && why[0] && !memcmp(before,uv,sizeof(uv)));
    seams[1]=4; /* Only one side needs to carry the seam marker. */
    Check(v,8,faces,8,seams,uv,1);
    assert(hypot(uv[1][0][0]-uv[6][1][0],uv[1][0][1]-uv[6][1][1])>100);
    /* Closed tetrahedron: require seams; all marked faces become islands. */
    const UVProjectionFace tetra[4]={{{0,1,2}},{{0,3,1}},{{1,3,2}},{{2,3,0}}};
    v[0]=(UVProjectionVertex){{0,0,0},{0}}; v[1]=(UVProjectionVertex){{20,0,0},{0}};
    v[2]=(UVProjectionVertex){{0,20,0},{0}}; v[3]=(UVProjectionVertex){{0,0,20},{0}};
    assert(!UVProjectionUnwrap(v,4,tetra,4,NULL,uv,&why) && why[0]);
    memset(seams,7,sizeof(seams)); Check(v,4,tetra,4,seams,uv,1);
    for (int a=0;a<4;a++) for (int b=a+1;b<4;b++)
    {
        int separate=0;
        for (int k=0;k<2;k++)
        { double al=uv[a][0][k],ah=al,bl=uv[b][0][k],bh=bl;
          for (int c=1;c<3;c++) { al=fmin(al,uv[a][c][k]); ah=fmax(ah,uv[a][c][k]); bl=fmin(bl,uv[b][c][k]); bh=fmax(bh,uv[b][c][k]); }
          separate |= ah<bl || bh<al; }
        assert(separate);
    }
    UVProjectionFace invalid[3]={{{0,1,2}},{{0,1,3}},{{0,1,2}}};
    assert(!UVProjectionUnwrap(v,4,invalid,3,NULL,uv,&why));
    invalid[0].vertices[2]=4; assert(!UVProjectionUnwrap(v,4,invalid,1,NULL,uv,&why));
    invalid[0].vertices[2]=1; assert(!UVProjectionUnwrap(v,4,invalid,1,NULL,uv,&why));
    v[0].position[0]=NAN; assert(!UVProjectionUnwrap(v,4,tetra,1,NULL,uv,&why));
    puts("PASS: marked tube seam, closed-surface guidance, non-overlapping islands, degenerate/non-manifold/invalid input rejection.");
}
int main(void) { setbuf(stdout,NULL); Grid(); Arch(); Cuts(); return 0; }
