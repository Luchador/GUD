#ifndef GUD_SKYGRADIENTMATH_H
#define GUD_SKYGRADIENTMATH_H

#include <math.h>

/* Seven vertices per row lets two rows fit the original RSP's 16 slots. */
#define SKY_GRADIENT_COLUMNS 6
#define SKY_GRADIENT_ROWS 8

static float skyGradientEndSine(float degrees)
{
    if (!(degrees >= 1.0f && degrees <= 90.0f)) { return 1.0f; }
    return sinf(degrees * 0.017453292519943295f);
}

/* A direction, not a position: camera translation cannot move the gradient.
 * Interpolate in sin(elevation), avoiding per-vertex inverse trig. Smoothstep
 * keeps the horizon and the end angle free of a sharp change in slope. */
static float skyGradientAmount(const float direction[3], float endSine)
{
    float length, t;
    if (!(direction[1] > 0.0f)) { return 0.0f; }
    length = sqrtf(direction[0]*direction[0] + direction[1]*direction[1]
        + direction[2]*direction[2]);
    if (!(length > 0.0f && endSine > 0.0f)) { return 0.0f; }
    t = direction[1] / (length * endSine);
    if (t >= 1.0f) { return 1.0f; }
    return t*t*(3.0f - 2.0f*t);
}

#endif
