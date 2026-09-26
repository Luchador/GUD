#include <math.h>
#include "scaling.h"

int ScalingValid(const Scaling *scale)
{
    int axis;
    if (!scale || !RotationValid(&scale->axes))
    {
        return 0;
    }
    for (axis = 0; axis < 3; axis++)
    {
        if (!isfinite(scale->pivot[axis]) || !isfinite(scale->factor[axis]) ||
            scale->factor[axis] <= 0 || scale->factor[axis] > 1000000)
        {
            return 0;
        }
    }
    return 1;
}

void ScalingGroupMember(const Scaling *group, const Rotation *axes, const double center[3],
    Scaling *local, double offset[3])
{
    ScalingPoint(group, center, offset);
    *local = *group;
    local->axes = *axes;
    for (int i = 0; i < 3; i++)
    {
        double length2 = 0;
        /* Measure each pad axis after applying the group scale. Copying XYZ
         * factors directly confuses world axes with pad axes (doors permute
         * them). Keep the pad orthogonal: native placements cannot shear. */
        for (int a = 0; a < 3; a++)
        {
            double component = 0;
            for (int b = 0; b < 3; b++) { component += group->axes.m[b][a] * axes->m[b][i]; }
            component *= group->factor[a];
            length2 += component * component;
        }
        local->factor[i] = sqrt(length2);
        local->pivot[i] = center[i];
        offset[i] -= center[i];
    }
}

void ScalingPoint(const Scaling *scale, const double in[3], double out[3])
{
    double relative[3], local[3] = {0}, result[3];
    int axis, component;
    for (axis = 0; axis < 3; axis++)
    {
        relative[axis] = in[axis] - scale->pivot[axis];
    }
    for (axis = 0; axis < 3; axis++)
    {
        for (component = 0; component < 3; component++)
        {
            local[axis] += scale->axes.m[component][axis] * relative[component];
        }
        local[axis] *= scale->factor[axis];
    }
    RotationVector(&scale->axes, local, result);
    for (axis = 0; axis < 3; axis++)
    {
        out[axis] = scale->pivot[axis] + result[axis];
    }
}
