#ifndef GEDITOR_SCALING_H
#define GEDITOR_SCALING_H
#include "rotation.h"
/* Factors multiply distances from pivot along the orthonormal axes.
   Geometry uses world axes; pads and props use pad axes (no shear). */
typedef struct Scaling
{
    Rotation axes;
    double pivot[3];
    double factor[3];
} Scaling;
int ScalingValid(const Scaling *scale);
void ScalingPoint(const Scaling *scale, const double in[3], double out[3]);
#endif
