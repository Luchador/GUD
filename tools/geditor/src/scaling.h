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
/* Group spacing follows group axes; each member retains orthogonal local
 * axes because native placement pads cannot encode shear. */
void ScalingGroupMember(const Scaling *group, const Rotation *axes, const double center[3],
    Scaling *local, double offset[3]);
#endif
