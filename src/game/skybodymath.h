#ifndef GUD_SKYBODYMATH_H
#define GUD_SKYBODYMATH_H

#include <math.h>

#define SKY_BODY_SUN_IMAGE 0x0aa4u
#define SKY_BODY_MOON_IMAGE 0x0aa5u
#define SKY_BODY_MAX_VERTICES 12

/* A tangent-plane disc at infinity. Clip before perspective division so
 * turning past it, looking straight up and crossing a viewport edge are safe.
 * Shared by the engine and GEditor; no camera position or world scale enters
 * this calculation. Texture coordinates follow image rows, top to bottom. */
typedef struct SkyBodyVertex
{
    float x, y, w, s, t, height;
} SkyBodyVertex;

static int skyBodyFinite(float value)
{
    union { float f; unsigned int bits; } number;
    number.f = value;
    return (number.bits & 0x7f800000u) != 0x7f800000u;
}

static float skyBodyClipDistance(const SkyBodyVertex *v, int plane)
{
    switch (plane)
    {
    case 0: return v->height; /* Do not draw over the water/ground hemisphere. */
    case 1: return v->w - 0.00001f;
    case 2: return v->w + v->x;
    case 3: return v->w - v->x;
    case 4: return v->w + v->y;
    default: return v->w - v->y;
    }
}

static int skyBodyBuild(const float direction[3], float degrees,
        const float worldToClip[4][4], float horizonNdc,
        SkyBodyVertex output[SKY_BODY_MAX_VERTICES])
{
    SkyBodyVertex buffers[2][SKY_BODY_MAX_VERTICES];
    float d[3], right[3], up[3], ray[3];
    float largest = 0.0f, length, radius, angle;
    int i, j, plane, count = 4, source = 0;

    if (!(degrees > 0.0f && degrees <= 90.0f)
            || !skyBodyFinite(horizonNdc)) { return 0; }
    for (i = 0; i < 3; i++)
    {
        if (!skyBodyFinite(direction[i])) { return 0; }
        if (fabsf(direction[i]) > largest) { largest = fabsf(direction[i]); }
    }
    if (largest == 0.0f) { return 0; }
    for (i = 0; i < 3; i++) { d[i] = direction[i] / largest; }
    length = sqrtf(d[0]*d[0] + d[1]*d[1] + d[2]*d[2]);
    for (i = 0; i < 3; i++) { d[i] /= length; }
    length = sqrtf(d[0]*d[0] + d[2]*d[2]);
    right[0] = length > 0.00001f ? -d[2] / length : 1.0f;
    right[1] = 0.0f;
    right[2] = length > 0.00001f ? d[0] / length : 0.0f;
    up[0] = -right[2]*d[1];
    up[1] = right[2]*d[0] - right[0]*d[2];
    up[2] = right[0]*d[1];
    angle = degrees * 0.008726646259971648f;
    radius = sinf(angle) / cosf(angle);
    for (i = 0; i < 4; i++)
    {
        SkyBodyVertex *v = &buffers[0][i];
        v->s = i == 1 || i == 2 ? 1.0f : 0.0f;
        v->t = i >= 2 ? 1.0f : 0.0f;
        for (j = 0; j < 3; j++)
        { ray[j] = d[j] + radius * ((2*v->s-1)*right[j] + (1-2*v->t)*up[j]); }
        v->height = ray[1];
        v->x = ray[0]*worldToClip[0][0] + ray[1]*worldToClip[1][0] + ray[2]*worldToClip[2][0];
        v->y = ray[0]*worldToClip[0][1] + ray[1]*worldToClip[1][1] + ray[2]*worldToClip[2][1];
        v->w = ray[0]*worldToClip[0][3] + ray[1]*worldToClip[1][3] + ray[2]*worldToClip[2][3];
        v->y += horizonNdc * v->w;
        if (!skyBodyFinite(v->x) || !skyBodyFinite(v->y) || !skyBodyFinite(v->w)) { return 0; }
    }
    for (plane = 0; plane < 6 && count >= 3; plane++)
    {
        int next = 0;
        SkyBodyVertex *in = buffers[source], *out = buffers[1-source];
        for (i = 0; i < count; i++)
        {
            SkyBodyVertex *a = &in[i], *b = &in[(i+1)%count];
            float da = skyBodyClipDistance(a, plane), db = skyBodyClipDistance(b, plane);
            if (da >= 0.0f)
            {
                if (next == SKY_BODY_MAX_VERTICES) { return 0; }
                out[next++] = *a;
            }
            if ((da < 0.0f && db > 0.0f) || (da > 0.0f && db < 0.0f))
            {
                float fraction = da / (da-db);
                SkyBodyVertex *v;
                if (next == SKY_BODY_MAX_VERTICES) { return 0; }
                v = &out[next++];
                v->x = a->x + (b->x-a->x)*fraction;
                v->y = a->y + (b->y-a->y)*fraction;
                v->w = a->w + (b->w-a->w)*fraction;
                v->s = a->s + (b->s-a->s)*fraction;
                v->t = a->t + (b->t-a->t)*fraction;
                v->height = a->height + (b->height-a->height)*fraction;
            }
        }
        count = next; source = 1-source;
    }
    if (count < 3) { return 0; }
    for (i = 0; i < count; i++) { output[i] = buffers[source][i]; }
    return count;
}
#endif
