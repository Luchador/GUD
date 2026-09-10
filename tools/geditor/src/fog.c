#include <float.h>
#include <math.h>
#include <stdint.h>
#include <string.h>
#include "fog.h"

int FogConfigure(FogCurve *curve, double nearclip, double farclip,
                 double renderscale, int start, int end)
{
    int64_t range = (int64_t)end - start;
    int64_t multiplier, offset;
    double worldnear, worldfar, linearstart, linearend, faramount;
    FogCurve result;

    if (curve == NULL) { return 0; }
    memset(curve, 0, sizeof(*curve));
    if (!isfinite(nearclip) || !isfinite(farclip) || !isfinite(renderscale)
        || nearclip <= 0 || farclip <= nearclip || renderscale <= 0 || range <= 0)
    { return 0; }

    /* gSPFogPosition (include/PR/gbi.h) sends signed 16-bit factors.
       Keep its integer division, including for FogEnd > 1000 (Bunker 2). */
    multiplier = 128000 / range;
    offset = ((int64_t)500 - start) * 256 / range;
    if (multiplier <= 0 || multiplier > INT16_MAX
        || offset < INT16_MIN || offset > INT16_MAX) { return 0; }

    /* guPerspectiveF maps forward depth d to (f+n)/(f-n)-2fn/((f-n)d).
       envLoadCurrentEnvironment divides clip distances by renderScale. */
    worldnear = nearclip / renderscale;
    worldfar = farclip / renderscale;
    result.constant = (multiplier * (farclip + nearclip) / (farclip - nearclip) + offset) / 255.0;
    result.reciprocal = 2.0 * multiplier * (farclip / (farclip - nearclip)) * worldnear / 255.0;
    if (!isfinite(result.constant) || !isfinite(result.reciprocal)
        || result.constant <= 0 || result.reciprocal <= 0
        || !isfinite(worldfar) || worldfar > FLT_MAX) { return 0; }

    /* Legacy drivers without fog coordinates still show distance fog.
       Match the zero-fog distance and the amount at the game's far plane;
       do not assume FogEnd == 1000 or force full fog at that plane. */
    linearstart = result.reciprocal / result.constant;
    faramount = result.constant - result.reciprocal / worldfar;
    if (faramount <= 0) { return 0; }
    linearend = linearstart + (worldfar - linearstart) / faramount;
    if (!isfinite(linearstart) || !isfinite(linearend)
        || linearstart < 0 || linearend > FLT_MAX || linearend <= linearstart)
    { return 0; }
    result.linearstart = (float)linearstart;
    result.linearend = (float)linearend;
    if (result.linearend <= result.linearstart) { return 0; }
    *curve = result;
    return 1;
}

float FogAmount(const FogCurve *curve, double depth)
{
    double amount;
    /* Behind/at the camera: avoid division by zero before GL clips triangles. */
    if (!(depth > 0)) { return 0.0f; }
    amount = curve->constant - curve->reciprocal / depth;
    return amount <= 0 ? 0.0f : amount >= 1 ? 1.0f : (float)amount;
}
