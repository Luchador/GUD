#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "uvprojection.h"
static double Area(double uv[3][2])
{ return (uv[1][0]-uv[0][0])*(uv[2][1]-uv[0][1])-(uv[1][1]-uv[0][1])*(uv[2][0]-uv[0][0]); }
int main(void)
{
    UVProjectionVertex vertices[9]={0}, before[9];
    const UVProjectionFace faces[12]={{{0,4,6}},{{0,6,2}},{{1,3,7}},{{1,7,5}},
        {{0,1,5}},{{0,5,4}},{{2,6,7}},{{2,7,3}},{{0,2,3}},{{0,3,1}},{{4,5,7}},{{4,7,6}}};
    const double spans[3]={128,64,32};
    double uv[12][3][2], fit[12][3][2], moved[12][3][2]; const char *why="";
    for (int i=0;i<8;i++) for (int a=0;a<3;a++) { vertices[i].position[a]=(i&(1<<a))?spans[a]:0; }
    vertices[8].position[0]=1e9; /* Unreferenced vertices must not change the fit. */
    memcpy(before,vertices,sizeof(vertices));
    assert(UVProjectionBox(vertices,9,faces,12,4,uv,&why));
    assert(UVProjectionBox(vertices,9,faces,12,0,fit,&why));
    assert(!memcmp(vertices,before,sizeof(vertices)));
    for (int f=0;f<12;f++)
    {
        assert(Area(uv[f])>0 && Area(fit[f])>0); /* All six sides face outward. */
        for (int c=0;c<3;c++)
        {
            int next=(c+1)%3; double length=0, texels=0, normalized=0;
            for (int a=0;a<3;a++)
            { double d=vertices[faces[f].vertices[c]].position[a]-vertices[faces[f].vertices[next]].position[a]; length+=d*d; }
            for (int k=0;k<2;k++)
            {
                double d=uv[f][c][k]-uv[f][next][k]; texels+=d*d;
                d=fit[f][c][k]-fit[f][next][k]; normalized+=d*d;
                assert(fit[f][c][k]>=0 && fit[f][c][k]<=1);
            }
            assert(fabs(texels*16-length)<1e-8 && fabs(normalized*128*128-length)<1e-8);
            if (f<4 || f>=8) /* Vertical walls retain world-up V. */
            { assert(uv[f][c][1]==vertices[faces[f].vertices[c]].position[1]/4); }
        }
        if (!(f&1)) for (int c=0;c<3;c++) for (int d=0;d<3;d++)
        { if (faces[f].vertices[c]==faces[f+1].vertices[d]) assert(!memcmp(uv[f][c],uv[f+1][d],sizeof(uv[f][c]))); }
    }
    assert(uv[0][0][0]!=uv[8][0][0]); /* Shared box corner needs a UV split. */
    for (int i=0;i<9;i++) for (int a=0;a<3;a++) { vertices[i].position[a]+=1000000*(a+1); }
    assert(UVProjectionBox(vertices,9,faces,12,4,moved,&why) && !memcmp(uv,moved,sizeof(uv)));

    /* Sloped triangles choose their own closest axis, with both windings. */
    const double bases[3][2][3]={{{0,0,-1},{0,1,0}},{{1,0,0},{0,0,-1}},{{1,0,0},{0,1,0}}};
    for (int axis=0;axis<3;axis++) for (int reverse=0;reverse<2;reverse++)
    {
        UVProjectionVertex v[3]={0}; UVProjectionFace face={{0,reverse?2:1,reverse?1:2}};
        double mapped[1][3][2];
        for (int a=0;a<3;a++) { v[1].position[a]=80*bases[axis][0][a]; v[2].position[a]=40*bases[axis][1][a]; }
        v[1].position[axis]=10; v[2].position[axis]=5;
        assert(UVProjectionBox(v,3,&face,1,4,mapped,&why) && Area(mapped[0])>0);
        for (int k=0;k<2;k++)
        {
            double lo=mapped[0][0][k],hi=lo;
            for (int c=1;c<3;c++) { lo=fmin(lo,mapped[0][c][k]); hi=fmax(hi,mapped[0][c][k]); }
            assert(hi-lo==(k?10:20));
        }
    }
    UVProjectionFace bad={{0,0,0}};
    assert(!UVProjectionBox(vertices,9,&bad,1,4,uv,&why) && why[0]);
    bad.vertices[1]=9; assert(!UVProjectionBox(vertices,9,&bad,1,4,uv,&why));
    assert(!UVProjectionBox(vertices,9,faces,12,-1,uv,&why));
    assert(!UVProjectionBox(vertices,9,faces,12,NAN,uv,&why));
    vertices[0].position[0]=INFINITY; assert(!UVProjectionBox(vertices,9,faces,12,4,uv,&why));
    puts("PASS: six box sides, sloped faces, winding, upright walls, uniform density/fit, aligned triangulation, shared-corner seams, large translations and invalid inputs.");
    return 0;
}
