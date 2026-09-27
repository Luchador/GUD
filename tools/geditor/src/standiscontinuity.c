/* Diagnostic geometry only: never edits native links or tile coordinates. */
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include "stanload.h"

typedef struct StanBoundary {
    int direction[3], a[3], b[3], axis, low, high;
    int64_t moment[3];
    DWORD tile, neighbor;
    BOOL forward;
} StanBoundary;

static int StanBoundaryGcd(int a, int b)
{
    a = abs(a); b = abs(b);
    while (b) { int remainder = a % b; a = b; b = remainder; }
    return a;
}

static BOOL StanBoundaryPoint(const StanFile *stan, const StanPoint *point, int out[3])
{
    const float values[3] = {point->x, point->y, point->z};
    for (int axis = 0; axis < 3; axis++)
    {
        double value = round((double)values[axis] * stan->levelscale);
        if (!isfinite(value) || value < -32768 || value > 32767) { return FALSE; }
        out[axis] = (int)value;
    }
    return TRUE;
}

static BOOL StanBoundaryMake(const StanFile *stan, DWORD tile, unsigned int point, StanBoundary *edge)
{
    const StanTile *t = &stan->tiles[tile];
    int divisor;
    if (!StanBoundaryPoint(stan, &t->points[point], edge->a)
        || !StanBoundaryPoint(stan, &t->points[(point + 1) % t->pointcount], edge->b)) { return FALSE; }
    for (int axis = 0; axis < 3; axis++) { edge->direction[axis] = edge->b[axis] - edge->a[axis]; }
    divisor = StanBoundaryGcd(StanBoundaryGcd(edge->direction[0], edge->direction[1]), edge->direction[2]);
    if (!divisor) { return FALSE; }
    for (edge->axis = 0; !edge->direction[edge->axis]; edge->axis++) { }
    edge->forward = edge->direction[edge->axis] > 0;
    if (!edge->forward) { divisor = -divisor; }
    for (int axis = 0; axis < 3; axis++) { edge->direction[axis] /= divisor; }
    /* Primitive direction plus p cross direction uniquely identifies a 3D
     * integer line. Use 64 bits for the full signed-short coordinate range. */
    for (int axis = 0; axis < 3; axis++)
    {
        int j = (axis + 1) % 3, k = (axis + 2) % 3;
        edge->moment[axis] = (int64_t)edge->a[j] * edge->direction[k]
            - (int64_t)edge->a[k] * edge->direction[j];
    }
    edge->low = edge->forward ? edge->a[edge->axis] : edge->b[edge->axis];
    edge->high = edge->forward ? edge->b[edge->axis] : edge->a[edge->axis];
    edge->tile = tile;
    edge->neighbor = StanLinkedTile(stan, t->points[point].link);
    return TRUE;
}

static int StanBoundaryLineCompare(const StanBoundary *a, const StanBoundary *b)
{
    for (int axis = 0; axis < 3; axis++)
        if (a->direction[axis] != b->direction[axis]) { return a->direction[axis] < b->direction[axis] ? -1 : 1; }
    for (int axis = 0; axis < 3; axis++)
        if (a->moment[axis] != b->moment[axis]) { return a->moment[axis] < b->moment[axis] ? -1 : 1; }
    return 0;
}

static int StanBoundaryCompare(const void *left, const void *right)
{
    const StanBoundary *a = left, *b = right;
    int order = StanBoundaryLineCompare(a, b);
    if (order) { return order; }
    return (a->low > b->low) - (a->low < b->low);
}

static StanPoint StanBoundaryEnd(const StanFile *stan, const StanBoundary *edge, int value)
{
    double fraction = (double)(value - edge->a[edge->axis]) / (edge->b[edge->axis] - edge->a[edge->axis]);
    StanPoint point = {0};
    point.x = (float)((edge->a[0] + fraction * (edge->b[0] - edge->a[0])) / stan->levelscale);
    point.y = (float)((edge->a[1] + fraction * (edge->b[1] - edge->a[1])) / stan->levelscale);
    point.z = (float)((edge->a[2] + fraction * (edge->b[2] - edge->a[2])) / stan->levelscale);
    return point;
}

/* Pick a non-collinear point in each incident tile, so line rasterization
 * uses that tile's plane and clears its own opaque fill at every camera angle. */
static StanPoint StanBoundaryInterior(const StanFile *stan, const StanBoundary *edge)
{
    const StanTile *tile = &stan->tiles[edge->tile];
    StanPoint result = StanBoundaryEnd(stan, edge, edge->low);
    double best = 0;
    for (unsigned int p = 0; p < tile->pointcount; p++)
    {
        int point[3]; double delta[3], area = 0;
        if (!StanBoundaryPoint(stan, &tile->points[p], point)) { continue; }
        for (int a = 0; a < 3; a++) { delta[a] = point[a] - edge->a[a]; }
        for (int a = 0; a < 3; a++)
        {
            int j = (a + 1) % 3, k = (a + 2) % 3;
            double cross = delta[j] * edge->direction[k] - delta[k] * edge->direction[j];
            area += cross * cross;
        }
        if (area > best)
        {
            best = area;
            result = (StanPoint){point[0] / stan->levelscale, point[1] / stan->levelscale, point[2] / stan->levelscale, 0};
        }
    }
    return result;
}

BOOL StanBuildDiscontinuities(const StanFile *stan, StanDiscontinuity **out, DWORD *countout)
{
    StanBoundary *edges = NULL;
    StanDiscontinuity *gaps = NULL;
    size_t capacity = 0, edgecount = 0, count = 0, gapcapacity = 0;
    *out = NULL; *countout = 0;
    if (!stan || !stan->tiles || !stan->tilecount) { return TRUE; }
    if (!(stan->levelscale > 0) || !isfinite(stan->levelscale)) { return FALSE; }
    for (DWORD t = 0; t < stan->tilecount; t++)
    {
        if (stan->tiles[t].pointcount < 3 || stan->tiles[t].pointcount > STAN_TILE_MAX_POINTS) { return FALSE; }
        capacity += stan->tiles[t].pointcount;
    }
    if (capacity > SIZE_MAX / sizeof(*edges)) { return FALSE; }
    edges = malloc(capacity * sizeof(*edges));
    if (!edges) { return FALSE; }
    for (DWORD t = 0; t < stan->tilecount; t++)
        for (unsigned int p = 0; p < stan->tiles[t].pointcount; p++)
            if (StanBoundaryMake(stan, t, p, &edges[edgecount])) { edgecount++; }
    qsort(edges, edgecount, sizeof(*edges), StanBoundaryCompare);
    /* Compare only overlapping intervals on the same line, not every pair
     * of level edges. A subdivided neighbor may cover just part of an edge. */
    for (size_t first = 0; first < edgecount; )
    {
        size_t end = first + 1;
        while (end < edgecount && !StanBoundaryLineCompare(&edges[first], &edges[end])) { end++; }
        for (size_t i = first; i < end; i++)
        {
            const StanBoundary *a = &edges[i];
            for (size_t j = i + 1; j < end && edges[j].low < a->high; j++)
            {
                const StanBoundary *b = &edges[j];
                if (a->tile == b->tile
                    || (a->neighbor == b->tile && b->neighbor == a->tile)) { continue; }
                if (count == gapcapacity)
                {
                    size_t next = gapcapacity ? gapcapacity * 2 : 64;
                    if (next < gapcapacity || next > SIZE_MAX / sizeof(*gaps) || next > UINT32_MAX) { goto fail; }
                    StanDiscontinuity *grown = realloc(gaps, next * sizeof(*gaps));
                    if (!grown) { goto fail; }
                    gaps = grown; gapcapacity = next;
                }
                StanDiscontinuity *gap = &gaps[count++];
                gap->tiles[0] = a->tile; gap->tiles[1] = b->tile;
                gap->ends[0] = StanBoundaryEnd(stan, a, b->low);
                gap->ends[1] = StanBoundaryEnd(stan, a, a->high < b->high ? a->high : b->high);
                gap->interior[0] = StanBoundaryInterior(stan, a);
                gap->interior[1] = StanBoundaryInterior(stan, b);
            }
        }
        first = end;
    }
    free(edges); *out = gaps; *countout = (DWORD)count; return TRUE;
fail:
    free(edges); free(gaps); return FALSE;
}
