#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include "bgdocument.h"
#include "stanload.h"
typedef unsigned int GLuint;
typedef int GLsizei;
typedef float GLfloat;
typedef unsigned char GLubyte;
#include "types.inc"

typedef struct ViewportState {
    float posx,posy,posz;
    Vertex *scene;
    SceneBatch *batches;
    int batchcount;
    BOOL showobjects;
    int hidden;
} ViewportState;

#include "rays.inc"
static BOOL ViewportTriangleHidden(const ViewportState *s,int triangle) { return triangle==s->hidden; }
static DWORD ViewportFindPickedStan(const ViewportState *s,const ViewportPickRay *ray,double *distance)
{ *distance=DBL_MAX;return STAN_TILE_NONE; }
static BOOL ViewportBatchIsPickable(const ViewportState *s,const SceneBatch *b) { return !b->object; }
/* Plain opaque geometry is enough to exercise visibility/occlusion here;
 * the renderer's independent material/texture sampling is not under test. */
static BOOL ViewportRayBatchTriangleDistance(const ViewportState *s,const SceneBatch *b,
    const ViewportPickRay *ray,int corner,double *distance)
{ return !ViewportTriangleHidden(s,corner/3) && ViewportRayTriangleDistance(ray,s->scene+corner,b->cullbackfaces,distance); }
#include "visible.inc"

int main(void)
{
    Vertex vertices[6]={
        {.x=507.062683f,.y=813.413025f,.z=-3084.63135f},
        {.x=-211.337f,.y=813.413025f,.z=-3084.63135f},
        {.x=-211.337f,.y=-770.297f,.z=-3084.63135f}};
    SceneBatch batch={.first=3,.count=3};
    ViewportState state={.scene=vertices,.batches=&batch,.hidden=-1};
    int c,i,rejected=0;
    /* Fractional Jungle-scale positions and changing viewpoints. Every ray
     * points exactly at a visible corner: the old code rejected 89 of 150. */
    for(c=0;c<3;c++) for(i=0;i<50;i++)
    {
        state.posx=123.456f+i*13.579f;state.posy=1400.271f-i*7.919f;state.posz=-2800.357f;
        if(!ViewportComponentVisible(&state,0,&vertices[c],TRUE)) { rejected++; }
    }
    printf("Exact-corner visibility failures: %d / 150\n",rejected);
    assert(rejected==0);
    state.posx=100;state.posy=0;state.posz=-3500;
    assert(!ViewportComponentVisible(&state,0,&vertices[0],TRUE)); /* Backface culling. */
    assert(ViewportComponentVisible(&state,0,&vertices[0],FALSE));
    state.hidden=0;assert(!ViewportComponentVisible(&state,0,&vertices[0],FALSE));state.hidden=-1;
    /* A blocker on the exact ray still occludes the target. */
    state.posx=vertices[0].x;state.posy=vertices[0].y;state.posz=-2800;
    vertices[3]=(Vertex){.x=state.posx-100,.y=state.posy-100,.z=-2900};
    vertices[4]=(Vertex){.x=state.posx+100,.y=state.posy-100,.z=-2900};
    vertices[5]=(Vertex){.x=state.posx,.y=state.posy+100,.z=-2900};
    state.batchcount=1;
    assert(!ViewportComponentVisible(&state,0,&vertices[0],TRUE));
    state.hidden=1;assert(ViewportComponentVisible(&state,0,&vertices[0],TRUE));
    /* Preserve strict face hit bounds and the near/far interval. */
    {
        ViewportPickRay ray={{vertices[0].x+1,vertices[0].y+1,-2800},{0,0,-1},0,DBL_MAX};double d;
        assert(!ViewportRayTriangleDistance(&ray,vertices,FALSE,&d));
        ray.origin[0]=vertices[1].x;ray.origin[1]=vertices[1].y;
        assert(ViewportRayTriangleDistance(&ray,vertices,FALSE,&d));
        ray.maxdistance=200;assert(!ViewportRayTriangleDistance(&ray,vertices,FALSE,&d));
        ray.maxdistance=DBL_MAX;ray.mindistance=400;assert(!ViewportRayTriangleDistance(&ray,vertices,FALSE,&d));
    }
    puts("PASS: fractional-coordinate vertex visibility, backfaces, hidden geometry, occlusion and ray bounds.");
    return 0;
}
