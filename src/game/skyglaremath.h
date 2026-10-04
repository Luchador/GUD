#ifndef GUD_SKYGLAREMATH_H
#define GUD_SKYGLAREMATH_H

#include <math.h>
#ifdef TARGET_N64
#include "math_atan2f.h"
#endif

/* Camera-space ray, normalized, with forward along -Z. Reject the cone
 * before the angle calculation and before doing any background collision. */
static float skyGlareStrength(const float ray[3])
{
    float angle, strength;
    if (ray[2] >= -0.9396926208f) { return 0.0f; } /* cos(20 degrees) */
    angle = atan2f(sqrtf(ray[0]*ray[0] + ray[1]*ray[1]), -ray[2]);
    strength = 0.2f * (1.0f - angle * 2.8647889757f);
    return strength > 0.0f ? strength : 0.0f;
}

/* 90% of the previous error remains after one 60 Hz game tick: about
 * 95% of a transition completes in half a second, independent of FPS.
 * Integer exponentiation avoids adding powf/expf to the N64 runtime. */
static float skyGlareSmooth(float current, float target, int ticks)
{
    float remaining = 1.0f, decay = 0.9f;
    while (ticks > 0)
    {
        if (ticks & 1) { remaining *= decay; }
        decay *= decay;
        ticks >>= 1;
    }
    return target + (current - target) * remaining;
}

/* Segment versus room bounds, all in BG coordinates. Keep fractional
 * bounds intact (door shadows can extend a room by fractional amounts). */
static int skyGlareIntersectsRoom(const float origin[3], const float delta[3],
        const float minimum[3], const float maximum[3])
{
    float enter = 0.0f, leave = 1.0f;
    int axis;
    for (axis = 0; axis < 3; axis++)
    {
        float near, far, swap;
        if (delta[axis] == 0.0f)
        {
            if (origin[axis] < minimum[axis] || origin[axis] > maximum[axis]) { return 0; }
            continue;
        }
        near = (minimum[axis] - origin[axis]) / delta[axis];
        far = (maximum[axis] - origin[axis]) / delta[axis];
        if (near > far) { swap = near; near = far; far = swap; }
        if (near > enter) { enter = near; }
        if (far < leave) { leave = far; }
        if (enter > leave) { return 0; }
    }
    return 1;
}

#endif
