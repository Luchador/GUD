#include "uvprojection.h"
#include <math.h>

static void UVProjectionCross(const double a[3], const double b[3], double out[3])
{
    out[0] = a[1] * b[2] - a[2] * b[1];
    out[1] = a[2] * b[0] - a[0] * b[2];
    out[2] = a[0] * b[1] - a[1] * b[0];
}

static double UVProjectionLength(const double vector[3])
{
    return hypot(hypot(vector[0], vector[1]), vector[2]);
}

int UVProjectionMap(UVProjectionVertex *vertices, int vertexcount,
                    const UVProjectionFace *faces, int facecount,
                    UVProjection projection, const char **reason)
{
    double u[3] = {0}, v[3] = {0}, normal[3] = {0};
    double minimum[2] = {0}, maximum[2] = {0}, span[2], extent;
    int i, axis;
    *reason = "Select background faces to project.";
    if (vertices == 0 || faces == 0 || vertexcount <= 0 || facecount <= 0
        || projection < 0 || projection >= UV_PROJECTION_COUNT) { return 0; }
    for (i = 0; i < vertexcount; i++)
    {
        for (axis = 0; axis < 3; axis++)
        {
            if (!isfinite(vertices[i].position[axis]))
            { *reason = "The selected faces contain invalid coordinates."; return 0; }
        }
    }
    for (i = 0; i < facecount; i++)
    {
        for (axis = 0; axis < 3; axis++)
        {
            if (faces[i].vertices[axis] < 0 || faces[i].vertices[axis] >= vertexcount)
            { *reason = "A projection face references a missing vertex."; return 0; }
        }
    }

    /* Right-handed planes: U cross V points along the named positive axis.
       Walls keep world Y upright; a Y projection uses X and negative Z. */
    if (projection == UV_PROJECTION_X) { u[2] = -1; v[1] = 1; }
    else if (projection == UV_PROJECTION_Y) { u[0] = 1; v[2] = -1; }
    else if (projection == UV_PROJECTION_Z) { u[0] = 1; v[1] = 1; }
    else
    {
        int contributing = 0;
        double length, up[3] = {0, 1, 0};
        for (i = 0; i < facecount; i++)
        {
            const double *a = vertices[faces[i].vertices[0]].position;
            const double *b = vertices[faces[i].vertices[1]].position;
            const double *c = vertices[faces[i].vertices[2]].position;
            double ab[3], ac[3], face[3];
            for (axis = 0; axis < 3; axis++) { ab[axis] = b[axis] - a[axis]; ac[axis] = c[axis] - a[axis]; }
            UVProjectionCross(ab, ac, face);
            length = UVProjectionLength(face);
            if (!isfinite(length)) { *reason = "The selected geometry is too large to project."; return 0; }
            if (length == 0) { continue; }
            for (axis = 0; axis < 3; axis++) { normal[axis] += face[axis] / length; }
            contributing++;
        }
        if (contributing == 0)
        { *reason = "Best Fit needs at least one face with a nonzero area."; return 0; }
        length = UVProjectionLength(normal);
        if (length <= contributing * 1.0e-8)
        { *reason = "The selected face normals cancel out. Choose an axis projection instead."; return 0; }
        for (axis = 0; axis < 3; axis++) { normal[axis] /= length; }
        /* Avoid an unstable basis when the averaged normal is near world Y. */
        if (fabs(normal[1]) > 0.999) { up[1] = 0; up[2] = -1; }
        UVProjectionCross(up, normal, u);
        length = UVProjectionLength(u);
        for (axis = 0; axis < 3; axis++) { u[axis] /= length; }
        UVProjectionCross(normal, u, v);
    }

    for (i = 0; i < vertexcount; i++)
    {
        double offset[3];
        for (axis = 0; axis < 3; axis++)
        { offset[axis] = vertices[i].position[axis] - vertices[0].position[axis]; }
        vertices[i].uv[0] = offset[0] * u[0] + offset[1] * u[1] + offset[2] * u[2];
        vertices[i].uv[1] = offset[0] * v[0] + offset[1] * v[1] + offset[2] * v[2];
        for (axis = 0; axis < 2; axis++)
        {
            double coordinate = vertices[i].uv[axis];
            if (!isfinite(coordinate)) { *reason = "The selected geometry is too large to project."; return 0; }
            if (i == 0 || coordinate < minimum[axis]) { minimum[axis] = coordinate; }
            if (i == 0 || coordinate > maximum[axis]) { maximum[axis] = coordinate; }
        }
    }
    span[0] = maximum[0] - minimum[0]; span[1] = maximum[1] - minimum[1];
    extent = fmax(span[0], span[1]);
    if (!isfinite(extent)) { *reason = "The selected geometry is too large to project."; return 0; }
    for (i = 0; i < vertexcount; i++)
    {
        for (axis = 0; axis < 2; axis++)
        {
            vertices[i].uv[axis] = extent > 0
                ? (vertices[i].uv[axis] - minimum[axis]) / extent + (1 - span[axis] / extent) * 0.5
                : 0.5;
        }
    }
    *reason = "";
    return 1;
}
