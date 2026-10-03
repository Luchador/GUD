#ifndef GUD_SKYBODYTRIANGLE_H
#define GUD_SKYBODYTRIANGLE_H

#include "skybodymath.h"

typedef struct SkyBodyTriangleVertex
{
    float x, y; /* NDC, deliberately allowed outside the viewport. */
    float s, t, q; /* Projective texture coordinates: sample (s/q, t/q). */
} SkyBodyTriangleVertex;

/* Fit ONE triangle around the visible part of the original tangent plane.
 * Clipping is used to find its bounds and texture transform, never to emit a
 * triangle fan. Its bottom edge follows the world horizon, and the RDP
 * scissor handles the viewport. Transparent, clamped image borders hide the
 * extra area. The original angular diameter and perspective mapping survive.
 * bounds is {left,bottom,right,top} in NDC. */
static int skyBodyCover(const SkyBodyVertex *polygon, int count,
        SkyBodyTriangleVertex output[3], float bounds[4])
{
    float x[SKY_BODY_MAX_VERTICES], y[SKY_BODY_MAX_VERTICES];
    float q[3], s[3], t[3], h[3];
    float area = 0.0f, magnitude = 0.0f, nx, ny, length;
    float umin = 0.0f, umax = 0.0f, vmin = 0.0f, vmax = 0.0f;
    float u[3], v[3], maximum = 0.0f;
    int a = 0, b = 0, c = 0, i, j, k;

    if (count < 3 || count > SKY_BODY_MAX_VERTICES) { return 0; }
    for (i = 0; i < count; i++)
    {
        x[i] = polygon[i].x / polygon[i].w;
        y[i] = polygon[i].y / polygon[i].w;
        if (!i) { bounds[0] = bounds[2] = x[i]; bounds[1] = bounds[3] = y[i]; }
        if (x[i] < bounds[0]) { bounds[0] = x[i]; }
        if (x[i] > bounds[2]) { bounds[2] = x[i]; }
        if (y[i] < bounds[1]) { bounds[1] = y[i]; }
        if (y[i] > bounds[3]) { bounds[3] = y[i]; }
    }
    /* Clip intersections can be collinear. Use the widest basis available. */
    for (i = 0; i < count - 2; i++) for (j = i + 1; j < count - 1; j++)
        for (k = j + 1; k < count; k++)
        {
            float d = (x[j]-x[i])*(y[k]-y[i]) - (x[k]-x[i])*(y[j]-y[i]);
            float absolute = d < 0.0f ? -d : d;
            if (absolute > magnitude) { magnitude = absolute; area = d; a = i; b = j; c = k; }
        }
    if (magnitude == 0.0f) { return 0; }
    /* Contain round-off at homogeneous clip intersections before the
     * unsigned RDP scissor fields are packed. */
    if (bounds[0] < -1.0f) { bounds[0] = -1.0f; }
    if (bounds[1] < -1.0f) { bounds[1] = -1.0f; }
    if (bounds[2] > 1.0f) { bounds[2] = 1.0f; }
    if (bounds[3] > 1.0f) { bounds[3] = 1.0f; }
    for (i = 0; i < 3; i++)
    {
        const SkyBodyVertex *p = &polygon[i == 0 ? a : i == 1 ? b : c];
        q[i] = 1.0f / p->w;
        s[i] = p->s * q[i]; t[i] = p->t * q[i]; h[i] = p->height * q[i];
    }
    /* Height/w is affine in screen space. Its gradient points into the sky.
     * Align the base to that line so no triangle fragment crosses the horizon. */
    nx = ((h[1]-h[0])*(y[c]-y[a]) - (h[2]-h[0])*(y[b]-y[a])) / area;
    ny = ((x[b]-x[a])*(h[2]-h[0]) - (x[c]-x[a])*(h[1]-h[0])) / area;
    length = sqrtf(nx*nx + ny*ny);
    if (length > 0.000001f) { nx /= length; ny /= length; }
    else { nx = 0.0f; ny = 1.0f; }
    for (i = 0; i < count; i++)
    {
        float along = x[i]*ny - y[i]*nx, above = x[i]*nx + y[i]*ny;
        if (!i) { umin = umax = along; vmin = vmax = above; }
        if (along < umin) { umin = along; }
        if (along > umax) { umax = along; }
        if (above < vmin) { vmin = above; }
        if (above > vmax) { vmax = above; }
    }
    if (umax <= umin || vmax <= vmin) { return 0; }
    u[0] = (umin+umax)*0.5f; v[0] = 2.0f*vmax-vmin;
    u[1] = umin-(umax-umin)*0.5f; v[1] = vmin;
    u[2] = umax+(umax-umin)*0.5f; v[2] = vmin;
    for (i = 0; i < 3; i++)
    {
        float dx, dy, wb, wc, wa, absolute;
        SkyBodyTriangleVertex *p = &output[i];
        p->x = u[i]*ny + v[i]*nx; p->y = -u[i]*nx + v[i]*ny;
        dx = p->x-x[a]; dy = p->y-y[a];
        wb = (dx*(y[c]-y[a]) - (x[c]-x[a])*dy) / area;
        wc = ((x[b]-x[a])*dy - dx*(y[b]-y[a])) / area;
        wa = 1.0f-wb-wc;
        p->s = wa*s[0] + wb*s[1] + wc*s[2];
        p->t = wa*t[0] + wb*t[1] + wc*t[2];
        p->q = wa*q[0] + wb*q[1] + wc*q[2];
        absolute = p->q < 0.0f ? -p->q : p->q;
        if (absolute > maximum) { maximum = absolute; }
    }
    if (maximum == 0.0f || !skyBodyFinite(maximum)) { return 0; }
    for (i = 0; i < 3; i++)
    {
        output[i].s /= maximum; output[i].t /= maximum; output[i].q /= maximum;
        if (!skyBodyFinite(output[i].s) || !skyBodyFinite(output[i].t)) { return 0; }
    }
    return 3;
}

#endif
