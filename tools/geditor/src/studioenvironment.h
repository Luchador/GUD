#ifndef GEDITOR_STUDIOENVIRONMENT_H
#define GEDITOR_STUDIOENVIRONMENT_H
#include "texload.h"

/* Separable Gaussian convolution of a top-down 2:1, power-of-two panorama.
 * Percent 0 copies the source; 100 has sigma = 1/16 of the panorama width
 * (22.5 degrees). Longitude wraps; crossing a pole reflects latitude and
 * turns longitude by 180 degrees. Alpha is ignored for reflections.
 * Output is allocated for the caller. Source pixels are never modified.
 * Blurred previews are area-filtered to a suitable power-of-two resolution
 * (at most 1024 wide) to bound convolution work and the texture cache. */
BOOL StudioEnvironmentBlur(const TexPixel *source,int width,int height,int percent,
    TexPixel **output,int *outwidth,int *outheight);
#endif
