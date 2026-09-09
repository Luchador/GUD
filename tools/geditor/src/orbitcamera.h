#ifndef GEDITOR_ORBITCAMERA_H
#define GEDITOR_ORBITCAMERA_H

typedef struct OrbitCamera {
    double center[3];
    double radius, distance;
    double yaw, pitch;
} OrbitCamera;

void OrbitCameraFrame(OrbitCamera *camera, const double min[3], const double max[3],
                       double aspect, double fovy);
void OrbitCameraRotate(OrbitCamera *camera, double dx, double dy);
void OrbitCameraDolly(OrbitCamera *camera, double steps);
void OrbitCameraPosition(const OrbitCamera *camera, double position[3]);
void OrbitCameraClip(const OrbitCamera *camera, double *nearout, double *farout);

#endif
