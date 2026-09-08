#ifndef GEDITOR_ROTATION_H
#define GEDITOR_ROTATION_H
/* Right-handed world rotations, column vectors; Euler order Rz * Ry * Rx. */
typedef struct Rotation
{
    double m[3][3];
} Rotation;
void RotationEuler(Rotation *out, const double degrees[3]);
void RotationDegrees(const Rotation *rotation, double degrees[3]);
void RotationAxis(Rotation *out, int axis, double degrees);
void RotationMultiply(Rotation *out, const Rotation *a, const Rotation *b);
void RotationDifference(Rotation *out, const Rotation *target, const Rotation *old);
void RotationVector(const Rotation *r, const double in[3], double out[3]);
void RotationPoint(const Rotation *r, const double pivot[3], const double in[3], double out[3]);
int RotationBasis(Rotation *out, const double up[3], const double look[3]);
int RotationValid(const Rotation *r);
#endif
