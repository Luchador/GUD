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
