#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "src/game/occlusionmath.h"

static const float pos[3] = {0}, up[3] = {0,1,0}, look[3] = {0,0,1};
static const float bounds[6] = {-10,10,-10,10,-10,10};
static unsigned randomstate = 1234567;
static float Random(float low, float high)
{ randomstate = randomstate*1664525u+1013904223u; return low+(high-low)*(randomstate>>8)/16777216.0f; }

/* Independent segment/slab oracle in the box's local coordinates. */
static int Blocked(const float eye[3], const float point[3])
{
    double first = 0, last = 1;
    for (int i = 0; i < 3; i++)
    {
        double d = point[i]-eye[i], a, b;
        if (fabs(d) < 1e-10) { if (eye[i] < -10 || eye[i] > 10) { return 0; } continue; }
        a = (-10-eye[i])/d; b = (10-eye[i])/d;
        if (a > b) { double t = a; a = b; b = t; }
        if (a > first) { first = a; }
        if (b < last) { last = b; }
        if (first > last) { return 0; }
    }
    return first < 1 && last > 0;
}

static void Rotate(const float in[3], float out[3])
{ out[0] = .6f*in[0]+.8f*in[2]; out[1] = in[1]; out[2] = -.8f*in[0]+.6f*in[2]; }

int main(void)
{
    OcclusionBox box;
    float eye[3] = {0,0,-50};
    assert(occlusionBoxInit(&box,pos,up,look,bounds));
    occlusionBoxView(&box,eye,NULL);
    assert(box.count == 1);
    assert(occlusionBoxContainsSphere(&box,(float[]){0,0,40},5));
    assert(!occlusionBoxContainsSphere(&box,(float[]){0,0,-30},5)); /* in front */
    assert(!occlusionBoxContainsSphere(&box,(float[]){0,0,-60},1)); /* behind camera */
    assert(!occlusionBoxContainsSphere(&box,(float[]){18,0,40},5)); /* partial silhouette */
    assert(!occlusionBoxContainsSphere(&box,(float[]){0,0,-4},5)); /* contact margin */
    assert(!occlusionBoxContainsSphere(&box,(float[]){0,0,40},80)); /* surrounds box */
    assert(!occlusionBoxContainsSphere(&box,(float[]){NAN,0,40},1));
    assert(!occlusionBoxContainsSphere(&box,(float[]){0,0,40},NAN));
    assert(!occlusionBoxContainsSphere(&box,(float[]){0,0,40},-1));
    occlusionBoxView(&box,(float[]){0,0,0},NULL); assert(box.count == 0);
    occlusionBoxView(&box,(float[]){0,0,-10.5f},NULL); assert(box.count == 0);
    occlusionBoxView(&box,(float[]){0,0,50},NULL);
    assert(occlusionBoxContainsSphere(&box,(float[]){0,0,-40},5));
    assert(!occlusionBoxContainsSphere(&box,(float[]){0,0,40},5)); /* new player's view */
    occlusionBoxView(&box,(float[]){0,0,-50},(float[]){0,0,1,45});
    assert(occlusionBoxContainsSphere(&box,(float[]){0,0,40},5));
    occlusionBoxView(&box,(float[]){0,0,-50},(float[]){0,0,1,5});
    assert(box.count == 0); /* face removed by near clipping */
    assert(!occlusionBoxInit(&box,pos,up,look,(float[]){0,0,-10,10,-10,10}));
    assert(!occlusionBoxInit(&box,pos,up,up,bounds));
    assert(!occlusionBoxInit(&box,(float[]){INFINITY,0,0},up,look,bounds));

    int rejected = 0;
    for (int rotated = 0; rotated < 2; rotated++)
    {
        float direction[3];
        if (rotated) { Rotate(look,direction); } else { for (int i=0;i<3;i++) { direction[i]=look[i]; } }
        assert(occlusionBoxInit(&box,pos,up,direction,bounds));
        for (int trial = 0; trial < 20000; trial++)
        {
            float center[3], worldeye[3], worldcenter[3], radius = Random(.1f,10);
            for (int i = 0; i < 3; i++) { eye[i] = Random(-70,70); center[i] = Random(-120,120); }
            if (rotated) { Rotate(eye,worldeye); Rotate(center,worldcenter); }
            else { for (int i=0;i<3;i++) { worldeye[i]=eye[i]; worldcenter[i]=center[i]; } }
            occlusionBoxView(&box,worldeye,NULL);
            if (!occlusionBoxContainsSphere(&box,worldcenter,radius)) { continue; }
            rejected++;
            assert(Blocked(eye,center));
            for (int sample = 0; sample < 128; sample++)
            {
                float y = Random(-1,1), angle = Random(0,6.2831853f), r = sqrtf(1-y*y);
                float point[3] = {center[0]+radius*r*cosf(angle),center[1]+radius*y,center[2]+radius*r*sinf(angle)};
                assert(Blocked(eye,point));
            }
        }
    }
    assert(rejected > 100);
    printf("PASS: front/contact/partial/inside/near/invalid/view-switch cases; %d hidden spheres checked against independent rays.\n",rejected);
    return 0;
}
