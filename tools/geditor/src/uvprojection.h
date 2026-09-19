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

/* Unwrap one uncapped side wall. Geometric adjacency joins native UV/color
 * duplicates; seams has one three-bit edge mask per face (or NULL for auto).
 * Axis: 0=Auto (open rims), 1=X, 2=Y, 3=Z. Output distances are in
 * world units, ready for uniform texel density. No source data is modified. */
int UVProjectionCylinder(const UVProjectionVertex *vertices, int vertexcount,
    const UVProjectionFace *faces, int facecount, const unsigned char *seams,
    int axis, double (*uv)[3][2], const char **reason);

#endif
