#include "cameraframe.h"
#include <math.h>
#include <float.h>

int CameraFrameBegin(CameraFrame *frame, const double min[3], const double max[3],
                     const double eye[3], double yaw, double pitch,
                     double aspect, double fovy, double nearclip)
{
    const double radians = 3.14159265358979323846 / 180.0;
    double center[3], forward[3], right[3], up[3], tangent, distance, travel = 0;
    double depthmin = DBL_MAX, depthmax = -DBL_MAX, startdepth = 0;
    CameraFrame result = {0};
    int axis, corner;
    if (!frame || !min || !max || !eye || !isfinite(yaw) || !isfinite(pitch)
        || !isfinite(aspect) || aspect <= 0 || !isfinite(fovy) || fovy <= 0 || fovy >= 180
        || !isfinite(nearclip) || nearclip <= 0) { return 0; }
    for (axis = 0; axis < 3; axis++)
    {
        if (!isfinite(min[axis]) || !isfinite(max[axis]) || min[axis] > max[axis]
            || !isfinite(eye[axis])) { return 0; }
        center[axis] = min[axis]*0.5 + max[axis]*0.5;
        travel = hypot(travel, center[axis] - eye[axis]);
        result.start[axis] = eye[axis];
    }
    yaw *= radians; pitch *= radians;
    forward[0] = -sin(yaw)*cos(pitch); forward[1] = sin(pitch); forward[2] = -cos(yaw)*cos(pitch);
    right[0] = cos(yaw); right[1] = 0; right[2] = -sin(yaw);
    up[0] = sin(yaw)*sin(pitch); up[1] = cos(pitch); up[2] = cos(yaw)*sin(pitch);
    tangent = tan(fovy*radians*0.5);
    /* A point selection still needs comfortable working space (half a metre). */
    distance = fmax(50.0, nearclip*2.0);
    for (corner = 0; corner < 8; corner++)
    {
        double x = 0, y = 0, z = 0;
        for (axis = 0; axis < 3; axis++)
        {
            double delta = ((corner & (1 << axis)) ? max[axis] : min[axis]) - center[axis];
            x += delta*right[axis]; y += delta*up[axis]; z += delta*forward[axis];
        }
        distance = fmax(distance, fabs(x)*1.1/(tangent*aspect) - z);
        distance = fmax(distance, fabs(y)*1.1/tangent - z);
        depthmin = fmin(depthmin, z); depthmax = fmax(depthmax, z);
    }
    distance = fmax(distance, nearclip*2.0 - depthmin);
    for (axis = 0; axis < 3; axis++)
    {
        result.end[axis] = center[axis] - forward[axis]*distance;
        if (!isfinite(result.end[axis]) || fabs(result.end[axis]) > FLT_MAX) { return 0; }
        startdepth += (center[axis] - eye[axis])*forward[axis];
    }
    /* Cover the whole straight-line animation, even for a level-wide selection. */
    result.farclip = fmax(nearclip*4.0, (fmax(startdepth, distance) + depthmax)*1.1);
    result.duration = 0.5 + fmax(0.0, fmin(1.0, (travel - 1000.0)/9000.0));
    if (!isfinite(result.farclip)) { return 0; }
    *frame = result;
    return 1;
}

int CameraFrameSample(const CameraFrame *frame, double elapsed, double eye[3])
{
    double t = fmax(0.0, fmin(1.0, elapsed/frame->duration));
    double eased = t*t*(3.0 - 2.0*t);
    int axis;
    for (axis = 0; axis < 3; axis++)
    {
        eye[axis] = t >= 1.0 ? frame->end[axis]
            : frame->start[axis]*(1.0 - eased) + frame->end[axis]*eased;
    }
    return t >= 1.0;
}
