#ifndef GUD_GLASSMATH_H
#define GUD_GLASSMATH_H

/* Shared by the runtime and editor. Distances are world centimetres; the
 * returned additive alpha is quantized exactly as the game's primitive alpha. */
static int glassOpacityAtDistance(float distance, float tintDistance,
    float opaqueDistance, float minimumOpacity)
{
    float opacity;
    if (!(minimumOpacity >= 0.0f)) minimumOpacity = 0.0f;
    if (minimumOpacity > 1.0f) minimumOpacity = 1.0f;
    if (distance >= opaqueDistance) return 255;
    if (distance <= tintDistance || opaqueDistance <= tintDistance)
        return (int)(minimumOpacity * 255.0f);
    opacity = ((distance - tintDistance) * (1.0f - minimumOpacity)
        / (opaqueDistance - tintDistance) + minimumOpacity) * 255.0f;
    return (int)opacity;
}
#endif
