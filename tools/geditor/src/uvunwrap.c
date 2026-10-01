/* Least Squares Conformal Maps (Levy et al., SIGGRAPH 2002).
 * Solve the area-weighted Cauchy-Riemann equations with two automatic pins
 * per chart. Matrix-free, Jacobi-preconditioned conjugate gradients keeps
 * memory linear in the selection size; no external solver is required.
 * https://libigl.github.io/tutorial/#least-squares-conformal-maps
 */
#include "uvprojection.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef struct UnwrapPoint { double p[3]; int source; } UnwrapPoint;
typedef struct UnwrapEdge { int a, b, corner; } UnwrapEdge;
typedef struct UnwrapTriangle { int nodes[3]; double real[3], imag[3], area; } UnwrapTriangle;
typedef struct UnwrapChart { int first, count; double width, height, x, y; } UnwrapChart;
typedef struct UnwrapBoundary { int a, b; double low[2], high[2]; } UnwrapBoundary;

static int UnwrapPointCompare(const void *aa, const void *bb)
{
    const UnwrapPoint *a = aa, *b = bb;
    for (int k = 0; k < 3; k++) if (a->p[k] != b->p[k]) { return a->p[k] < b->p[k] ? -1 : 1; }
    return (a->source > b->source) - (a->source < b->source);
}
static int UnwrapEdgeCompare(const void *aa, const void *bb)
{
    const UnwrapEdge *a = aa, *b = bb;
    if (a->a != b->a) { return a->a < b->a ? -1 : 1; }
    if (a->b != b->b) { return a->b < b->b ? -1 : 1; }
    return (a->corner > b->corner) - (a->corner < b->corner);
}
static int UnwrapRoot(int *parent, int v)
{
    while (v != parent[v]) { parent[v] = parent[parent[v]]; v = parent[v]; }
    return v;
}
static void UnwrapJoin(int *parent, int a, int b)
{
    a = UnwrapRoot(parent, a); b = UnwrapRoot(parent, b);
    if (a < b) { parent[b] = a; } else { parent[a] = b; }
}
static double UnwrapDistance(const double *a, const double *b)
{ return hypot(hypot(a[0]-b[0], a[1]-b[1]), a[2]-b[2]); }

static void UnwrapMultiply(const UnwrapTriangle *triangles, int count,
    const double *x, double *out, int n)
{
    memset(out, 0, (size_t)n * 2 * sizeof(*out));
    for (int f = 0; f < count; f++)
    {
        const UnwrapTriangle *t = &triangles[f];
        double real = 0, imag = 0;
        for (int c = 0; c < 3; c++)
        {
            int i = t->nodes[c] * 2;
            real += t->real[c]*x[i] - t->imag[c]*x[i+1];
            imag += t->imag[c]*x[i] + t->real[c]*x[i+1];
        }
        for (int c = 0; c < 3; c++)
        {
            int i = t->nodes[c] * 2;
            out[i] += t->real[c]*real + t->imag[c]*imag;
            out[i+1] += -t->imag[c]*real + t->real[c]*imag;
        }
    }
}

static int UnwrapSolve(const UnwrapTriangle *triangles, int count, int n,
    int pin0, int pin1, double *x, double *work)
{
    double *diag = work, *r = diag + n*2, *p = r + n*2, *ap = p + n*2;
    double initial = 0, rho = 0;
    memset(work, 0, (size_t)n * 8 * sizeof(*work));
    memset(x, 0, (size_t)n * 2 * sizeof(*x));
    x[pin1*2] = 1;
    for (int f = 0; f < count; f++) for (int c = 0; c < 3; c++)
    {
        const UnwrapTriangle *t = &triangles[f];
        double d = t->real[c]*t->real[c] + t->imag[c]*t->imag[c];
        diag[t->nodes[c]*2] += d; diag[t->nodes[c]*2+1] += d;
    }
    UnwrapMultiply(triangles, count, x, ap, n);
    for (int i = 0; i < n*2; i++)
    {
        if (i/2 == pin0 || i/2 == pin1) { diag[i] = 0; continue; }
        if (!isfinite(diag[i]) || diag[i] <= 0) { return 0; }
        diag[i] = 1/diag[i]; r[i] = -ap[i]; p[i] = r[i]*diag[i];
        rho += r[i]*p[i];
    }
    initial = rho;
    if (!isfinite(initial) || initial <= 0) { return 0; }
    int limit = n*4 + 64;
    if (limit > 10000) { limit = 10000; }
    /* Bound synchronous work for very large or ill-conditioned selections. */
    if (limit > 20000000/count) { limit = 20000000/count; }
    for (int step = 0; step < limit; step++)
    {
        double denominator = 0;
        UnwrapMultiply(triangles, count, p, ap, n);
        for (int i = 0; i < n*2; i++) { denominator += p[i]*ap[i]; }
        if (!isfinite(denominator) || denominator <= 0) { return 0; }
        double alpha = rho/denominator, next = 0;
        for (int i = 0; i < n*2; i++) if (diag[i])
        { x[i] += alpha*p[i]; r[i] -= alpha*ap[i]; next += r[i]*r[i]*diag[i]; }
        if (!isfinite(next)) { return 0; }
        /* Confirm convergence against the actual residual; don't trust an
         * accumulated residual after a long solve on thin triangles. */
        if (next <= initial*1e-20 || (step+1)%128 == 0)
        {
            UnwrapMultiply(triangles, count, x, ap, n); next = 0;
            for (int i = 0; i < n*2; i++) if (diag[i])
            { r[i] = -ap[i]; next += r[i]*r[i]*diag[i]; }
            if (next <= initial*1e-20) { return 1; }
            for (int i = 0; i < n*2; i++) { p[i] = r[i]*diag[i]; }
        }
        else
        {
            double beta = next/rho;
            for (int i = 0; i < n*2; i++) { p[i] = r[i]*diag[i] + beta*p[i]; }
        }
        rho = next;
    }
    return 0;
}

static double UnwrapSide(const double *a, const double *b, const double *p)
{ return (b[0]-a[0])*(p[1]-a[1]) - (b[1]-a[1])*(p[0]-a[0]); }
static int UnwrapBoundaryCompare(const void *aa, const void *bb)
{
    const UnwrapBoundary *a = aa, *b = bb;
    return (a->low[0] > b->low[0]) - (a->low[0] < b->low[0]);
}
/* Positive triangles plus a simple boundary give a non-overlapping disk.
 * LSCM alone does not guarantee this for every surface. */
static int UnwrapCheckBoundary(UnwrapBoundary *edges, int count, const double *uv)
{
    unsigned int checks = 0;
    for (int i = 0; i < count; i++) for (int k = 0; k < 2; k++)
    {
        edges[i].low[k] = fmin(uv[edges[i].a*2+k], uv[edges[i].b*2+k]);
        edges[i].high[k] = fmax(uv[edges[i].a*2+k], uv[edges[i].b*2+k]);
    }
    qsort(edges, count, sizeof(*edges), UnwrapBoundaryCompare);
    for (int i = 0; i < count; i++) for (int j = i+1; j < count; j++)
    {
        const UnwrapBoundary *a = &edges[i], *b = &edges[j];
        if (b->low[0] > a->high[0]) { break; }
        if (++checks > 20000000u) { return 0; }
        if (b->low[1] > a->high[1] || a->low[1] > b->high[1]
            || a->a == b->a || a->a == b->b || a->b == b->a || a->b == b->b) { continue; }
        double x = UnwrapSide(uv+a->a*2, uv+a->b*2, uv+b->a*2);
        double y = UnwrapSide(uv+a->a*2, uv+a->b*2, uv+b->b*2);
        double z = UnwrapSide(uv+b->a*2, uv+b->b*2, uv+a->a*2);
        double w = UnwrapSide(uv+b->a*2, uv+b->b*2, uv+a->b*2);
        if (((x <= 0 && y >= 0) || (y <= 0 && x >= 0))
            && ((z <= 0 && w >= 0) || (w <= 0 && z >= 0))) { return 0; }
    }
    return 1;
}

int UVProjectionUnwrap(const UVProjectionVertex *vertices, int vertexcount,
    const UVProjectionFace *faces, int facecount, const unsigned char *seams,
    double (*uv)[3][2], const char **reason)
{
    UnwrapPoint *points = NULL;
    UnwrapEdge *edges = NULL;
    UnwrapTriangle *triangles = NULL;
    UnwrapChart *charts = NULL;
    UnwrapBoundary *boundary = NULL;
    int *map = NULL, *parent = NULL, *other = NULL, *nodes = NULL;
    int *order = NULL, *flip = NULL, *degree = NULL, *nodepoint = NULL;
    double *solution = NULL, *work = NULL, (*mapped)[3][2] = NULL;
    double low[3] = {0}, high[3] = {0}, extent = 0, boundsarea = 0, widest = 0;
    int pointcount = 0, chartcount = 0, ordered = 0, ok = 0;
    *reason = "Select faces to unwrap.";
    if (!vertices || !faces || !uv || vertexcount <= 0 || facecount <= 0) { return 0; }
    if (vertexcount > 1000000 || facecount > 200000)
    { *reason = "Select fewer faces to unwrap at once."; return 0; }
#define ALLOC(name, count) do { name = calloc((size_t)(count), sizeof(*name)); \
    if (!name) { *reason = "Out of memory unwrapping UVs."; goto done; } } while (0)
    ALLOC(points, vertexcount); ALLOC(map, vertexcount); ALLOC(edges, facecount*3);
    ALLOC(parent, facecount*3); ALLOC(other, facecount*3); ALLOC(nodes, facecount*3);
    ALLOC(order, facecount); ALLOC(flip, facecount); ALLOC(charts, facecount);
    ALLOC(triangles, facecount); ALLOC(degree, facecount*3); ALLOC(nodepoint, facecount*3);
    ALLOC(solution, facecount*6); ALLOC(work, facecount*24);
    ALLOC(boundary, facecount*3); ALLOC(mapped, facecount);
#undef ALLOC
    /* Ignore unused input nodes. Join exact geometric endpoints in scratch
     * space so native UV/color splits don't become unintended unwrap seams. */
    for (int i = 0; i < vertexcount; i++) { map[i] = -1; }
    for (int f = 0; f < facecount; f++) for (int c = 0; c < 3; c++)
    {
        int v = faces[f].vertices[c];
        if (v < 0 || v >= vertexcount) { *reason = "An unwrap face references a missing vertex."; goto done; }
        if (map[v] >= 0) { continue; }
        for (int k = 0; k < 3; k++)
        {
            double p = vertices[v].position[k];
            if (!isfinite(p)) { *reason = "The selected faces contain invalid coordinates."; goto done; }
            points[pointcount].p[k] = p;
            if (!pointcount || p < low[k]) { low[k] = p; }
            if (!pointcount || p > high[k]) { high[k] = p; }
        }
        points[pointcount].source = v; map[v] = pointcount++;
    }
    for (int k = 0; k < 3; k++) { extent = fmax(extent, high[k]-low[k]); }
    if (!isfinite(extent) || extent <= 0) { *reason = "The selection has no usable surface area."; goto done; }
    qsort(points, pointcount, sizeof(*points), UnwrapPointCompare);
    int unique = 0;
    for (int i = 0; i < pointcount; i++)
    {
        UnwrapPoint p = points[i];
        if (!unique || p.p[0] != points[unique-1].p[0]
            || p.p[1] != points[unique-1].p[1] || p.p[2] != points[unique-1].p[2])
        { points[unique++] = p; }
        map[p.source] = unique-1;
    }
    for (int i = 0; i < unique; i++) for (int k = 0; k < 3; k++)
    { points[i].p[k] = (points[i].p[k]-low[k])/extent; }
    for (int f = 0; f < facecount; f++)
    {
        flip[f] = -1;
        for (int c = 0; c < 3; c++)
        {
            int i = f*3+c, a = map[faces[f].vertices[c]], b = map[faces[f].vertices[(c+1)%3]];
            if (a == b) { *reason = "Remove collapsed triangles before unwrapping."; goto done; }
            edges[i] = (UnwrapEdge){a < b ? a : b, a < b ? b : a, i};
            parent[i] = i; other[i] = nodes[i] = -1;
        }
    }
    qsort(edges, facecount*3, sizeof(*edges), UnwrapEdgeCompare);
    for (int i = 0; i < facecount*3;)
    {
        int end = i+1;
        while (end < facecount*3 && edges[end].a == edges[i].a && edges[end].b == edges[i].b) { end++; }
        if (end-i > 2) { *reason = "More than two selected faces share an edge. Select one surface to unwrap."; goto done; }
        if (end-i == 2)
        {
            int a = edges[i].corner, b = edges[i+1].corner;
            if (!seams || !((seams[a/3] & (1 << (a%3))) || (seams[b/3] & (1 << (b%3)))))
            {
                int an = a/3*3 + (a%3+1)%3, bn = b/3*3 + (b%3+1)%3;
                int same = map[faces[a/3].vertices[a%3]] == map[faces[b/3].vertices[b%3]];
                other[a] = b; other[b] = a;
                UnwrapJoin(parent, a, same ? b : bn); UnwrapJoin(parent, an, same ? bn : b);
            }
        }
        i = end;
    }
    /* Orient equations consistently without changing authored face winding. */
    for (int seed = 0; seed < facecount; seed++) if (flip[seed] < 0)
    {
        UnwrapChart *chart = &charts[chartcount++];
        chart->first = ordered; order[ordered++] = seed; flip[seed] = 0;
        for (int at = chart->first; at < ordered; at++)
        {
            int f = order[at];
            for (int c = 0; c < 3; c++) if (other[f*3+c] >= 0)
            {
                int o = other[f*3+c], g = o/3;
                int direction = flip[f] ^ (map[faces[f].vertices[c]] == map[faces[g].vertices[o%3]]);
                if (flip[g] < 0) { flip[g] = direction; order[ordered++] = g; }
                else if (flip[g] != direction)
                { *reason = "The selection cannot be oriented as one surface. Mark a seam to open it."; goto done; }
            }
        }
        chart->count = ordered-chart->first;
    }
    for (int ch = 0; ch < chartcount; ch++)
    {
        UnwrapChart *chart = &charts[ch];
        int n = 0, nb = 0, pins[2] = {-1, -1};
        double area = 0, uvarea = 0, minimum[2] = {0}, maximum[2] = {0};
        for (int at = 0; at < chart->count; at++)
        {
            int f = order[chart->first+at];
            UnwrapTriangle *t = &triangles[at];
            for (int c = 0; c < 3; c++)
            {
                int root = UnwrapRoot(parent, f*3+c);
                if (nodes[root] < 0)
                { nodes[root] = n; nodepoint[n] = map[faces[f].vertices[c]]; degree[n++] = 0; }
                t->nodes[c] = nodes[root];
            }
            const double *a = points[nodepoint[t->nodes[0]]].p;
            const double *b = points[nodepoint[t->nodes[1]]].p;
            const double *c = points[nodepoint[t->nodes[2]]].p;
            double ab[3], ac[3], cross[3], length = UnwrapDistance(a, b), x = 0;
            for (int k = 0; k < 3; k++) { ab[k] = b[k]-a[k]; ac[k] = c[k]-a[k]; x += ab[k]*ac[k]; }
            cross[0] = ab[1]*ac[2]-ab[2]*ac[1]; cross[1] = ab[2]*ac[0]-ab[0]*ac[2]; cross[2] = ab[0]*ac[1]-ab[1]*ac[0];
            double twicearea = hypot(hypot(cross[0], cross[1]), cross[2]);
            if (!isfinite(twicearea) || length <= 0 || twicearea <= length*UnwrapDistance(a,c)*1e-12)
            { *reason = "Remove zero-area or nearly collapsed triangles before unwrapping."; goto done; }
            x /= length;
            double y = twicearea/length * (flip[f] ? -1 : 1), weight = 1/sqrt(twicearea);
            t->real[0] = (x-length)*weight; t->imag[0] = y*weight;
            t->real[1] = -x*weight; t->imag[1] = -y*weight;
            t->real[2] = length*weight; t->imag[2] = 0;
            area += (t->area = twicearea*0.5);
            for (int k = 0; k < 3; k++) if (other[f*3+k] < 0)
            {
                int a = t->nodes[k], b = t->nodes[(k+1)%3];
                boundary[nb++] = (UnwrapBoundary){.a=a, .b=b}; degree[a]++; degree[b]++;
            }
        }
        /* Require disk charts, including seam cuts. Never invent hidden cuts
         * in closed surfaces, handles or uncapped tubes. */
        if (n - (chart->count*3+nb)/2 + chart->count != 1 || nb < 3)
        { *reason = "Mark seams to open each closed surface or loop into a single patch, then unwrap again."; goto done; }
        for (int i = 0; i < n; i++)
        {
            if (degree[i] && degree[i] != 2)
            { *reason = "The selected surface has a branched boundary. Separate it or mark seams."; goto done; }
            if (degree[i] && (pins[0] < 0 || nodepoint[i] < nodepoint[pins[0]])) { pins[0] = i; }
        }
        if (pins[0] < 0) { *reason = "Mark a seam to open this surface before unwrapping."; goto done; }
        /* Two far-apart boundary points remove translation, rotation and
         * scale ambiguity. Overall texel scale is restored by surface area. */
        for (int pass = 0; pass < 2; pass++)
        {
            int start = pins[0], best = start; double distance = -1;
            for (int i = 0; i < n; i++) if (degree[i])
            {
                double d = UnwrapDistance(points[nodepoint[start]].p, points[nodepoint[i]].p);
                if (d > distance) { distance = d; best = i; }
            }
            if (!pass) { pins[0] = best; } else { pins[1] = best; }
        }
        if (pins[0] == pins[1] || !UnwrapSolve(triangles, chart->count, n, pins[0], pins[1], solution, work))
        { *reason = "The unwrap did not converge. Add seams or unwrap a smaller selection."; goto done; }
        for (int at = 0; at < chart->count; at++)
        {
            const UnwrapTriangle *t = &triangles[at]; int f = order[chart->first+at];
            double a = UnwrapSide(solution+t->nodes[0]*2, solution+t->nodes[1]*2, solution+t->nodes[2]*2);
            if (flip[f]) { a = -a; }
            if (!isfinite(a) || a <= 1e-14)
            { *reason = "The unwrap would fold or collapse faces. Add seams and try again."; goto done; }
            uvarea += a*0.5;
        }
        if (!UnwrapCheckBoundary(boundary, nb, solution))
        { *reason = "The unwrap boundary overlaps or is too complex. Add seams or select a smaller patch."; goto done; }
        double scale = sqrt(area/uvarea);
        for (int i = 0; i < n; i++) for (int k = 0; k < 2; k++)
        {
            double v = solution[i*2+k]*scale;
            if (!isfinite(v)) { *reason = "The unwrap exceeds the coordinate range."; goto done; }
            solution[i*2+k] = v;
            if (!i || v < minimum[k]) { minimum[k] = v; }
            if (!i || v > maximum[k]) { maximum[k] = v; }
        }
        chart->width = maximum[0]-minimum[0]; chart->height = maximum[1]-minimum[1];
        widest = fmax(widest, chart->width); boundsarea += chart->width*chart->height;
        for (int at = 0; at < chart->count; at++) for (int c = 0; c < 3; c++) for (int k = 0; k < 2; k++)
        { mapped[order[chart->first+at]][c][k] = solution[triangles[at].nodes[c]*2+k]-minimum[k]; }
    }
    /* Simple deterministic shelf packing, with one common density. */
    double gap = sqrt(boundsarea)*0.02, target = fmax(widest, sqrt(boundsarea)), x = 0, y = 0, rowheight = 0;
    for (int ch = 0; ch < chartcount; ch++)
    {
        UnwrapChart *chart = &charts[ch];
        if (x > 0 && x+chart->width > target) { x = 0; y += rowheight+gap; rowheight = 0; }
        chart->x = x; chart->y = y; x += chart->width+gap; rowheight = fmax(rowheight, chart->height);
        for (int at = 0; at < chart->count; at++) for (int c = 0; c < 3; c++) for (int k = 0; k < 2; k++)
        {
            double *v = &mapped[order[chart->first+at]][c][k];
            *v = (*v + (k ? chart->y : chart->x))*extent;
            if (!isfinite(*v)) { *reason = "The unwrap exceeds the coordinate range."; goto done; }
        }
    }
    memcpy(uv, mapped, (size_t)facecount*sizeof(*uv)); *reason = ""; ok = 1;
done:
    free(points); free(map); free(edges); free(parent); free(other); free(nodes);
    free(order); free(flip); free(charts); free(triangles); free(degree); free(nodepoint);
    free(solution); free(work); free(boundary); free(mapped);
    return ok;
}
