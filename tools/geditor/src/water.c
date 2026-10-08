#include <math.h>
#include "water.h"

WaterSample WaterSamplePlane(const RomWater *water, const double eye[3],
    const double direction[3], const float background[3], double seconds, int width, int height)
{
    WaterSample out = {0};
    if (!water || !water->enabled || width < 1 || height < 1 || !isfinite(seconds)
        || !isfinite(water->height)) { return out; }
    for (int i = 0; i < 3; i++)
    { if (!isfinite(eye[i]) || !isfinite(direction[i])) { return out; } }
    double elevation = eye[1] - water->height;
    if (elevation <= 0) { return out; }
    double horizontal = hypot(direction[0], direction[2]);
    /* The game limits horizontal reach to 300000 world units at the horizon.
     * Homogeneous UVs preserve perspective within each screen-mesh cell. */
    double q = fmax(1e-8, fmax(-direction[1], elevation * horizontal / 300000.0));
    double fade = horizontal > 0 ? -2.0 * direction[1] / horizontal : direction[1] < 0 ? 1 : 0;
    fade = fmax(0, fmin(1, fade));
    out.q = (float)q;
    /* Water uses position in s10.5 texel units (clouds use position * 0.1).
     * Wrap the camera offset only, keeping interpolation continuous. */
    out.s = (float)(fmod(eye[0] / (32.0 * width), 1.0) * q + elevation * direction[0] / (32.0 * width));
    out.t = (float)((fmod(eye[2] / (32.0 * height), 1.0)
        + fmod(seconds * 60.0, 4096.0) / (32.0 * height)) * q + elevation * direction[2] / (32.0 * height));
    for (int i = 0; i < 3; i++)
    {
        double sky = fmax(0, fmin(1, background[i]));
        out.color[i] = (float)(sky + water->color[i] / 255.0 * (1.0 - sky) * fade);
    }
    return out;
}

float WaterBlend(double seconds)
{
    if (!isfinite(seconds)) { return 0; }
    /* dyntexConfigureTwoLayerWater(FALSE): fixed tile offsets, with the
     * two samples crossfaded by the 60 Hz animation phase. */
    return (float)((sin(fmod(seconds * 60.0 * 0.04, 6.2831802)) * 127.0 + 128.0) / 255.0);
}
