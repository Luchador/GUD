#ifndef GEDITOR_UVPROJECTION_H
#define GEDITOR_UVPROJECTION_H

typedef enum UVProjection {
    UV_PROJECTION_X,
    UV_PROJECTION_Y,
    UV_PROJECTION_Z,
    UV_PROJECTION_BEST_FIT,
    UV_PROJECTION_COUNT
} UVProjection;

typedef struct UVProjectionVertex {
    double position[3];
    double uv[2];
} UVProjectionVertex;

typedef struct UVProjectionFace {
    int vertices[3];
} UVProjectionFace;

/* Projects all vertices using one plane and uniform scale, centered in 0-1.
   Best Fit averages unit face normals; zero-area faces do not contribute.
   Returns zero with a reason when a meaningful plane cannot be formed.
   Only the scratch UV outputs are modified; positions remain untouched. */
int UVProjectionMap(UVProjectionVertex *vertices, int vertexcount,
                    const UVProjectionFace *faces, int facecount,
                    UVProjection projection, const char **reason);

/* Fit one cylinder, including rotated or short/wide cylinders. U wraps once
 * around the sides; V spans the height. Caps use planar UVs in the same tile.
 * Per-corner output allows separate coordinates on either side of the seam.
 * The caller owns facecount triples; no source positions/UVs are changed. */
int UVProjectionCylinder(const UVProjectionVertex *vertices, int vertexcount,
    const UVProjectionFace *faces, int facecount, double (*uv)[3][2], const char **reason);

#endif
