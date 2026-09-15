#include <math.h>
#include "clouds.h"

CloudSample CloudsSample(const RomClouds *clouds, const double eye[3],
    const double direction[3], double seconds, int width, int height)
{
    CloudSample out = {0};
    double horizontal, distance, reach, x, z, fade;
    if (!clouds || !clouds->enabled || width < 1 || height < 1
        || !isfinite(seconds) || !isfinite(clouds->height)) { return out; }
    for (unsigned int i = 0; i < 3; i++)
    { if (!isfinite(eye[i]) || !isfinite(direction[i])) { return out; } }
    /* A small positive elevation also gives below-horizon vertices bounded
     * UVs. Their zero opacity blends smoothly into the solid sky colour. */
    horizontal = hypot(direction[0], direction[2]);
    distance = fmax(100.0, clouds->height - eye[1]) / fmax(0.001, direction[1]);
    reach = horizontal * distance;
    if (reach > 300000.0) { distance *= 300000.0 / reach; }
    x = eye[0] + distance * direction[0];
    z = eye[2] + distance * direction[2];
    fade = horizontal > 0 ? 2.0 * direction[1] / horizontal : direction[1] > 0 ? 1 : 0;
    out.opacity = (float)fmax(0, fmin(1, fade));
    /* skySetCloudVertex uses world position * 0.1 in s10.5 texel units.
     * Reduce only the camera offset modulo a tile; wrapping each vertex's
     * final UV would introduce interpolation seams across the screen mesh. */
    out.s = (float)(fmod(eye[0] / (320.0 * width), 1.0) + (x - eye[0]) / (320.0 * width));
    out.t = (float)(fmod(eye[2] / (320.0 * height), 1.0) + (z - eye[2]) / (320.0 * height)
        + fmod(seconds * 60.0 / (32.0 * height), 1.0));
    return out;
}

void CloudsTint(TexPixel *pixels, DWORD count, const RomClouds *clouds,
    const float background[3])
{
    DWORD i;
    for (i = 0; i < count; i++)
    {
        unsigned char *rgb = &pixels[i].r;
        for (unsigned int c = 0; c < 3; c++)
        {
            double sky = fmax(0, fmin(1, background[c]));
            double value = sky * 255.0 + clouds->color[c] * (1.0 - sky) * rgb[c] / 255.0;
            rgb[c] = (unsigned char)fmax(0, fmin(255, value + 0.5));
        }
        pixels[i].a = 255;
    }
}
