#include <math.h>
#include "occlusionmath.h"

/* Fail open on non-finite, huge or degenerate authored data. Comparisons also
 * reject NaN without depending on a C99 isfinite in the native toolchain. */
static int occlusionFinite(float x)
{
    return x >= -100000000.0f && x <= 100000000.0f;
}

static float occlusionDistance(const float plane[4], const float p[3])
{
    return plane[0]*p[0] + plane[1]*p[1] + plane[2]*p[2] + plane[3];
}

static int occlusionPlane(float plane[4], const float a[3], const float b[3], const float c[3])
{
    float u[3], v[3], length;
    int i;
    for (i = 0; i < 3; i++) { u[i] = b[i]-a[i]; v[i] = c[i]-a[i]; }
    for (i = 0; i < 3; i++) { plane[i] = u[(i+1)%3]*v[(i+2)%3] - u[(i+2)%3]*v[(i+1)%3]; }
    length = sqrtf(plane[0]*plane[0] + plane[1]*plane[1] + plane[2]*plane[2]);
    if (!(length > 0.0001f) || !occlusionFinite(length)) { return 0; }
    for (i = 0; i < 3; i++) { plane[i] /= length; }
    plane[3] = -(plane[0]*a[0] + plane[1]*a[1] + plane[2]*a[2]);
    return occlusionFinite(plane[3]);
}

int occlusionBoxInit(OcclusionBox *box, const float pos[3], const float up[3],
    const float look[3], const float bounds[6])
{
    float side[3], length, volume;
    int axis, corner;
    box->count = 0;
    for (axis = 0; axis < 3; axis++) {
        if (!occlusionFinite(pos[axis]) || !occlusionFinite(up[axis]) || !occlusionFinite(look[axis])
            || !occlusionFinite(bounds[axis*2]) || !occlusionFinite(bounds[axis*2+1])
            || !(bounds[axis*2+1]-bounds[axis*2] > 0.01f)) { return 0; }
    }
    for (axis = 0; axis < 3; axis++) {
        side[axis] = up[(axis+1)%3]*look[(axis+2)%3] - up[(axis+2)%3]*look[(axis+1)%3];
    }
    length = side[0]*side[0] + side[1]*side[1] + side[2]*side[2];
    volume = length*(bounds[1]-bounds[0])*(bounds[3]-bounds[2])*(bounds[5]-bounds[4]);
    if (!(length > 0.0001f) || !occlusionFinite(length) || !(volume > 0.0001f)) { return 0; }
    /* Same basis as SetupPadGetBoxCorners, including non-unit axes. */
    for (corner = 0; corner < 8; corner++) for (axis = 0; axis < 3; axis++) {
        float value = pos[axis] + side[axis]*bounds[(corner&1) != 0]
            + up[axis]*bounds[2+((corner&2) != 0)] + look[axis]*bounds[4+((corner&4) != 0)];
        if (!occlusionFinite(value)) { return 0; }
        box->corners[corner][axis] = value;
    }
    return 1;
}

void occlusionBoxView(OcclusionBox *box, const float eye[3], const float nearplane[4])
{
    static const unsigned char faces[6][4] = {
        {0,2,6,4}, {1,5,7,3}, {0,4,5,1},
        {2,3,7,6}, {0,1,3,2}, {4,6,7,5}
    };
    float center[3], facecenter[3], plane[4], distance;
    int face, i, edge, valid;
    box->count = 0;
    for (i = 0; i < 3; i++) {
        if (!occlusionFinite(eye[i])) { return; }
        center[i] = (box->corners[0][i]+box->corners[7][i])*0.5f;
    }
    for (face = 0; face < 6 && box->count < 3; face++) {
        float (*shadow)[4] = box->shadows[box->count];
        const unsigned char *v = faces[face];
        /* A clipped-away surface must not hide visible props. Use only faces
         * wholly beyond the near plane; straddling faces simply fail open. */
        if (nearplane) {
            for (i = 0; i < 4; i++) {
                if (!(occlusionDistance(nearplane, box->corners[v[i]]) > 1.0f)) { break; }
            }
            if (i != 4) { continue; }
        }
        if (!occlusionPlane(plane, box->corners[v[0]], box->corners[v[1]], box->corners[v[2]])) { continue; }
        /* Positive distance points inside the box and beyond this face. */
        if (occlusionDistance(plane, center) < 0) { for (i = 0; i < 4; i++) { plane[i] = -plane[i]; } }
        distance = occlusionDistance(plane, eye);
        if (!(distance < -1.0f)) { continue; }
        for (i = 0; i < 4; i++) { shadow[0][i] = plane[i]; }
        for (i = 0; i < 3; i++) { facecenter[i] = (box->corners[v[0]][i]+box->corners[v[2]][i])*0.5f; }
        valid = 1;
        for (edge = 0; edge < 4; edge++) {
            float *side = shadow[edge+1];
            if (!occlusionPlane(side, eye, box->corners[v[edge]], box->corners[v[(edge+1)%4]])) { valid = 0; break; }
            if (occlusionDistance(side, facecenter) < 0) { for (i = 0; i < 4; i++) { side[i] = -side[i]; } }
        }
        if (valid) { box->count++; }
    }
}

int occlusionBoxContainsSphere(const OcclusionBox *box, const float pos[3], float radius)
{
    int shadow, plane;
    if (!(radius > 0) || !occlusionFinite(radius)) { return 0; }
    for (plane = 0; plane < 3; plane++) { if (!occlusionFinite(pos[plane])) { return 0; } }
    /* One world unit of safety keeps contact/tangent cases visible. */
    radius += 1.0f;
    for (shadow = 0; shadow < box->count; shadow++) {
        for (plane = 0; plane < 5; plane++) {
            if (!(occlusionDistance(box->shadows[shadow][plane], pos) > radius)) { break; }
        }
        if (plane == 5) { return 1; }
    }
    return 0;
}
