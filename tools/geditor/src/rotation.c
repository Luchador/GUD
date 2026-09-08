#include <math.h>
#include "rotation.h"
#define ROTATION_RAD (3.14159265358979323846 / 180.0)
void RotationMultiply(Rotation *out, const Rotation *a, const Rotation *b)
{
    Rotation result = {{{0}}};
    int i, j, k;
    for (i = 0; i < 3; i++)
    {
        for (j = 0; j < 3; j++)
        {
            for (k = 0; k < 3; k++)
            {
                result.m[i][j] += a->m[i][k] * b->m[k][j];
            }
        }
    }
    *out = result;
}
void RotationDifference(Rotation *out, const Rotation *target, const Rotation *old)
{
    Rotation transpose;
    int i, j;
    for (i = 0; i < 3; i++)
    {
        for (j = 0; j < 3; j++)
        {
            transpose.m[i][j] = old->m[j][i];
        }
    }
    RotationMultiply(out, target, &transpose);
}
void RotationAxis(Rotation *out, int axis, double degrees)
{
    int i, a = (axis + 1) % 3, b = (axis + 2) % 3;
    double angle = remainder(degrees, 360.0) * ROTATION_RAD;
    *out = (Rotation){{{0}}};
    for (i = 0; i < 3; i++)
    {
        out->m[i][i] = 1;
    }
    out->m[a][a] = out->m[b][b] = cos(angle);
    out->m[a][b] = -sin(angle);
    out->m[b][a] = sin(angle);
}
void RotationEuler(Rotation *out, const double degrees[3])
{
    Rotation x, y, z;
    RotationAxis(&x, 0, degrees[0]);
    RotationAxis(&y, 1, degrees[1]);
    RotationAxis(&z, 2, degrees[2]);
    RotationMultiply(out, &y, &x);
    RotationMultiply(out, &z, out);
}
void RotationDegrees(const Rotation *r, double degrees[3])
{
    double c = hypot(r->m[0][0], r->m[1][0]);
    degrees[1] = atan2(-r->m[2][0], c) / ROTATION_RAD;
    degrees[0] =
        (c > 1e-8 ? atan2(r->m[2][1], r->m[2][2]) : atan2(-r->m[1][2], r->m[1][1])) / ROTATION_RAD;
    degrees[2] = (c > 1e-8 ? atan2(r->m[1][0], r->m[0][0]) : 0) / ROTATION_RAD;
}
void RotationVector(const Rotation *r, const double in[3], double out[3])
{
    double result[3] = {0};
    int i, j;
    for (i = 0; i < 3; i++)
    {
        for (j = 0; j < 3; j++)
        {
            result[i] += r->m[i][j] * in[j];
        }
    }
    for (i = 0; i < 3; i++)
    {
        out[i] = result[i];
    }
}
void RotationPoint(const Rotation *r, const double pivot[3], const double in[3], double out[3])
{
    double v[3];
    int i;
    for (i = 0; i < 3; i++)
    {
        v[i] = in[i] - pivot[i];
    }
    RotationVector(r, v, v);
    for (i = 0; i < 3; i++)
    {
        out[i] = pivot[i] + v[i];
    }
}
static int RotationNormalize(double v[3])
{
    double length = sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    int i;
    if (!isfinite(length) || length < 1e-10)
    {
        return 0;
    }
    for (i = 0; i < 3; i++)
    {
        v[i] /= length;
    }
    return 1;
}
int RotationBasis(Rotation *out, const double up[3], const double look[3])
{
    double z[3] = {look[0], look[1], look[2]}, x[3], y[3];
    int i;
    if (!RotationNormalize(z))
    {
        return 0;
    }
    for (i = 0; i < 3; i++)
    {
        x[i] = up[(i + 1) % 3] * z[(i + 2) % 3] - up[(i + 2) % 3] * z[(i + 1) % 3];
    }
    if (!RotationNormalize(x))
    {
        return 0;
    }
    for (i = 0; i < 3; i++)
    {
        y[i] = z[(i + 1) % 3] * x[(i + 2) % 3] - z[(i + 2) % 3] * x[(i + 1) % 3];
    }
    for (i = 0; i < 3; i++)
    {
        out->m[i][0] = x[i];
        out->m[i][1] = y[i];
        out->m[i][2] = z[i];
    }
    return 1;
}
int RotationValid(const Rotation *r)
{
    int i, j;
    double determinant;
    if (!r)
    {
        return 0;
    }
    for (i = 0; i < 3; i++)
    {
        for (j = 0; j < 3; j++)
        {
            double dot = 0;
            int k;
            for (k = 0; k < 3; k++)
            {
                dot += r->m[k][i] * r->m[k][j];
            }
            if (!isfinite(dot) || fabs(dot - (i == j)) > 1e-5)
            {
                return 0;
            }
        }
    }
    determinant = r->m[0][0] * (r->m[1][1] * r->m[2][2] - r->m[1][2] * r->m[2][1]) -
                  r->m[0][1] * (r->m[1][0] * r->m[2][2] - r->m[1][2] * r->m[2][0]) +
                  r->m[0][2] * (r->m[1][0] * r->m[2][1] - r->m[1][1] * r->m[2][0]);
    return fabs(determinant - 1) < 1e-5;
}
