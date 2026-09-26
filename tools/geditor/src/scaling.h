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
/* Group spacing follows group axes. Member factors are the lengths of its
 * pad axes after group scaling; orientation stays fixed because native pads
 * cannot encode shear. Axis-aligned members reproduce the group scale exactly. */
void ScalingGroupMember(const Scaling *group, const Rotation *axes, const double center[3],
    Scaling *local, double offset[3]);
#endif
