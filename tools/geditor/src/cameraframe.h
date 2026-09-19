#ifndef GEDITOR_CAMERAFRAME_H
#define GEDITOR_CAMERAFRAME_H

/* World positions are centimetres; angles are degrees; time is seconds. */
typedef struct CameraFrame {
    double start[3], end[3];
    double duration, farclip;
} CameraFrame;

/* Preserve the view direction and fit every bounds corner, with 10% margin. */
int CameraFrameBegin(CameraFrame *frame, const double min[3], const double max[3],
                     const double eye[3], double yaw, double pitch,
                     double aspect, double fovy, double nearclip);
/* Returns nonzero at completion, writing the exact destination. */
int CameraFrameSample(const CameraFrame *frame, double elapsed, double eye[3]);

#endif
