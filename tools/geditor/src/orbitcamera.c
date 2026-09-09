#include <math.h>
#include "orbitcamera.h"

#define ORBIT_RADIANS (3.14159265358979323846 / 180.0)

void OrbitCameraFrame(OrbitCamera *camera, const double min[3], const double max[3],
                       double aspect, double fovy)
{
    double squared = 0;
    double halfangle = fovy * 0.5 * ORBIT_RADIANS;
    int axis;
    for (axis = 0; axis < 3; axis++)
    {
        double extent = (max[axis] - min[axis]) * 0.5;
        camera->center[axis] = min[axis] + extent;
        squared += extent * extent;
    }
    camera->radius = fmax(sqrt(squared), 0.001);
    if (aspect > 0 && aspect < 1) { halfangle = atan(tan(halfangle) * aspect); }
    /* Fit a bounding sphere, so subsequent orbiting cannot cut off corners. */
    camera->distance = camera->radius / sin(halfangle) * 1.1;
    camera->yaw = 30;
    camera->pitch = -15;
}

void OrbitCameraRotate(OrbitCamera *camera, double dx, double dy)
{
    camera->yaw = fmod(camera->yaw - dx * 0.3, 360.0);
    camera->pitch = fmax(-89.0, fmin(89.0, camera->pitch - dy * 0.3));
}

void OrbitCameraDolly(OrbitCamera *camera, double steps)
{
    camera->distance = fmax(camera->radius * 0.02,
        fmin(camera->radius * 100.0, camera->distance * pow(0.85, steps)));
}

void OrbitCameraPosition(const OrbitCamera *camera, double position[3])
{
    double yaw = camera->yaw * ORBIT_RADIANS;
    double pitch = camera->pitch * ORBIT_RADIANS;
    position[0] = camera->center[0] + sin(yaw) * cos(pitch) * camera->distance;
    position[1] = camera->center[1] - sin(pitch) * camera->distance;
    position[2] = camera->center[2] + cos(yaw) * cos(pitch) * camera->distance;
}

void OrbitCameraClip(const OrbitCamera *camera, double *nearout, double *farout)
{
    *nearout = fmax(camera->radius * 0.001, camera->distance - camera->radius * 1.5);
    *farout = camera->distance + camera->radius * 2.0;
}
