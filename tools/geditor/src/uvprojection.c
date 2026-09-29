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

int UVProjectionBox(const UVProjectionVertex *vertices, int vertexcount,
    const UVProjectionFace *faces, int facecount, double unitspertexel,
    double (*uv)[3][2], const char **reason)
{
    double minimum[3] = {0}, maximum[3] = {0}, span[3], extent;
    *reason = "Select faces to box map.";
    if (!vertices || !faces || !uv || vertexcount <= 0 || facecount <= 0) { return 0; }
    if (!isfinite(unitspertexel) || unitspertexel < 0)
    { *reason = "Enter a positive texel size, or turn off Use texel size."; return 0; }
    for (int f = 0; f < facecount; f++) for (int c = 0; c < 3; c++)
    {
        int index = faces[f].vertices[c];
        if (index < 0 || index >= vertexcount)
        { *reason = "A projection face references a missing vertex."; return 0; }
        for (int a = 0; a < 3; a++)
        {
            double p = vertices[index].position[a];
            if (!isfinite(p)) { *reason = "The selected faces contain invalid coordinates."; return 0; }
            if ((!f && !c) || p < minimum[a]) { minimum[a] = p; }
            if ((!f && !c) || p > maximum[a]) { maximum[a] = p; }
        }
    }
    for (int a = 0; a < 3; a++) { span[a] = maximum[a] - minimum[a]; }
    extent = fmax(span[0], fmax(span[1], span[2]));
    if (!isfinite(extent)) { *reason = "The selected geometry is too large to project."; return 0; }
    for (int f = 0; f < facecount; f++)
    {
        const double *a = vertices[faces[f].vertices[0]].position;
        const double *b = vertices[faces[f].vertices[1]].position;
        const double *c = vertices[faces[f].vertices[2]].position;
        double ab[3], ac[3], normal[3];
        for (int k = 0; k < 3; k++) { ab[k] = b[k] - a[k]; ac[k] = c[k] - a[k]; }
        UVProjectionCross(ab, ac, normal);
        int plane = 0;
        for (int k = 0; k < 3; k++)
        {
            if (!isfinite(normal[k])) { *reason = "The selected geometry is too large to project."; return 0; }
            /* Largest normal component is the nearest axis; ties prefer X,
             * then Y, then Z, independently of selection/vertex ordering. */
            if (fabs(normal[k]) > fabs(normal[plane])) { plane = k; }
        }
        if (normal[plane] == 0)
        { *reason = "Box mapping requires every selected face to have a nonzero area."; return 0; }
        int axes[2] = {plane == 0 ? 2 : 0, plane == 1 ? 2 : 1};
        int signs[2] = {(plane == 0 ? -1 : 1) * (normal[plane] < 0 ? -1 : 1), plane == 1 ? -1 : 1};
        /* Positive sides use the planar tools' right-handed bases. Flip U
         * on negative sides so walls stay upright when viewed from outside. */
        for (int corner = 0; corner < 3; corner++) for (int k = 0; k < 2; k++)
        {
            int axis = axes[k];
            double p = vertices[faces[f].vertices[corner]].position[axis];
            double distance = signs[k] > 0 ? p - minimum[axis] : maximum[axis] - p;
            double mapped = unitspertexel > 0 ? distance / unitspertexel
                : distance / extent + (1 - span[axis] / extent) * 0.5;
            if (!isfinite(mapped)) { *reason = "The selected geometry is too large to project."; return 0; }
            uv[f][corner][k] = mapped;
        }
    }
    *reason = "";
    return 1;
}

int UVProjectionMap(UVProjectionVertex *vertices, int vertexcount,
                    const UVProjectionFace *faces, int facecount,
                    UVProjection projection, double unitspertexel, const char **reason)
{
    double u[3] = {0}, v[3] = {0}, normal[3] = {0};
    double minimum[2] = {0}, maximum[2] = {0}, span[2], extent;
    int i, axis;
    *reason = "Select background faces to project.";
    if (vertices == 0 || faces == 0 || vertexcount <= 0 || facecount <= 0
        || projection < 0 || projection >= UV_PROJECTION_COUNT) { return 0; }
    if (!isfinite(unitspertexel) || unitspertexel < 0)
    { *reason = "Enter a positive texel size, or turn off Use texel size."; return 0; }
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
            vertices[i].uv[axis] = unitspertexel > 0
                ? (vertices[i].uv[axis] - minimum[axis]) / unitspertexel
                : extent > 0
                ? (vertices[i].uv[axis] - minimum[axis]) / extent + (1 - span[axis] / extent) * 0.5
                : 0.5;
        }
    }
    *reason = "";
    return 1;
}
