#include <ultra64.h>
#include "math_sincos.h"

#define SINF_HALF_PI             1.570796251296997f
#define SINF_INV_PI              0.31830987334251f
#define SINF_PI_HIGH             3.1415927410125732f
#define SINF_PI_LOW              0.000000031786509424592f
#define SINF_COEFFICIENT_3        0.0000026057805371238f
#define SINF_COEFFICIENT_2       -0.0001980960223591f
#define SINF_COEFFICIENT_1        0.0083330664783716f
#define SINF_COEFFICIENT_0       -0.16666659712791f

#define SINF_POLYNOMIAL_MIN_KEY  0x0e6
#define SINF_RANGE_REDUCE_KEY    0x0ff
#define SINF_MAX_KEY             0x136

f32 cosf(f32 radians)
{
    return sinf(radians + SINF_HALF_PI);
}

f32 sinf(f32 radians)
{
    u32 angleBits = *(u32 *)&radians;
    s32 magnitudeKey = (angleBits >> 22) & 0x1ff;
    f32 angleSquared;
    f32 polynomial;

    if (magnitudeKey < SINF_RANGE_REDUCE_KEY)
    {
        if (magnitudeKey < SINF_POLYNOMIAL_MIN_KEY)
        {
            return radians;
        }

        angleSquared = radians * radians;

        polynomial = SINF_COEFFICIENT_3;
        polynomial = (polynomial * angleSquared) + SINF_COEFFICIENT_2;
        polynomial = (polynomial * angleSquared) + SINF_COEFFICIENT_1;
        polynomial = (polynomial * angleSquared) + SINF_COEFFICIENT_0;

        return ((polynomial * angleSquared) * radians) + radians;
    }

    if (magnitudeKey < SINF_MAX_KEY)
    {
        f32 piMultipleFloat = radians * SINF_INV_PI;
        s32 piMultiple = (s32)(piMultipleFloat > 0.0f ? piMultipleFloat + 0.5f : piMultipleFloat - 0.5f);
        f32 roundedRadians = (f32)piMultiple;

        radians -= SINF_PI_HIGH * roundedRadians;
        radians -= SINF_PI_LOW * roundedRadians;

        angleSquared = radians * radians;

        polynomial = SINF_COEFFICIENT_3;
        polynomial = (polynomial * angleSquared) + SINF_COEFFICIENT_2;
        polynomial = (polynomial * angleSquared) + SINF_COEFFICIENT_1;
        polynomial = (polynomial * angleSquared) + SINF_COEFFICIENT_0;
        polynomial = ((polynomial * angleSquared) * radians) + radians;

        if (piMultiple & 1)
        {
            polynomial = -polynomial;
        }

        return polynomial;
    }

    return 0.0f;
}
