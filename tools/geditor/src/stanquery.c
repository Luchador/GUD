/* Placement subset of game/stan.c. Keep tile identity and link traversal:
   a global downward ray can pick the wrong floor in overlapping rooms. */
#include <math.h>
#include <stdlib.h>

#include "stanload.h"

static BOOL StanHasTile(const StanFile *stan, DWORD index)
{
    return stan != NULL && stan->tiles != NULL && index < stan->tilecount;
}

/* The overlay stores world floats, while the runtime tests edges against
   integer file coordinates after scaling the query. Restore those integers
   here: doing edge tests on pre-scaled floats changes exact seam decisions. */
static void StanQueryTile(const StanFile *stan, DWORD index, StanTile *tile)
{
    int point;

    *tile = stan->tiles[index];
    for (point = 0; point < tile->pointcount; point++)
    {
        tile->points[point].x = roundf(tile->points[point].x * stan->levelscale);
        tile->points[point].y = roundf(tile->points[point].y * stan->levelscale);
        tile->points[point].z = roundf(tile->points[point].z * stan->levelscale);
    }
}

static DWORD StanLinkedTile(const StanFile *stan, unsigned short link)
{
    DWORD offset, low = 0, high = stan->tilecount;

    if ((link >> 4) == 0) { return STAN_TILE_NONE; }
    /* The game's link base is 0x80 bytes before the first tile. */
    offset = stan->tiles[0].sourceoffset + ((DWORD)link << 3) - 0x80u;
    while (low < high)
    {
        DWORD mid = low + (high - low) / 2;

        if (stan->tiles[mid].sourceoffset < offset) { low = mid + 1; }
        else { high = mid; }
    }
    return low < stan->tilecount && stan->tiles[low].sourceoffset == offset
        ? low : STAN_TILE_NONE;
}

static float StanEdgeSide(const StanPoint *a, const StanPoint *b,
                          float x, float z)
{
    return (b->z - a->z) * (x - a->x) - (b->x - a->x) * (z - a->z);
}

static BOOL StanInsideTriple(const StanTile *tile, float x, float z)
{
    int edge;

    for (edge = 0; edge < 3; edge++)
    {
        if (StanEdgeSide(&tile->points[tile->extreme[edge]],
                         &tile->points[tile->extreme[(edge + 1) % 3]], x, z) < 0.0f)
        {
            return FALSE;
        }
    }
    return TRUE;
}

BOOL StanGetTileHeight(const StanFile *stan, DWORD index,
                       float x, float z, float *height)
{
    StanTile querytile;
    const StanTile *tile = &querytile;
    const StanPoint *a, *b, *c;
    float abx, aby, abz, acx, acy, acz, nx, ny, nz;

    if (!StanHasTile(stan, index) || height == NULL) { return FALSE; }
    StanQueryTile(stan, index, &querytile);
    x *= stan->levelscale;
    z *= stan->levelscale;
    a = &tile->points[tile->extreme[0]];
    b = &tile->points[tile->extreme[1]];
    c = &tile->points[tile->extreme[2]];
    abx = b->x - a->x; aby = b->y - a->y; abz = b->z - a->z;
    acx = c->x - a->x; acy = c->y - a->y; acz = c->z - a->z;
    nx = aby * acz - abz * acy;
    ny = abz * acx - abx * acz;
    nz = abx * acy - aby * acx;
    *height = ny == 0.0f ? a->y : a->y - ((x - a->x) * nx + (z - a->z) * nz) / ny;
    *height *= 1.0f / stan->levelscale;
    return isfinite(*height);
}

/* getRotationalDirectionBetween also orders collinear endpoints. These
   cases matter when a pad lies exactly on a linked edge or tile corner. */
static int StanDirection(float ax, float az, float bx, float bz)
{
    if (ax * bz > az * bx) { return 1; }
    if (ax * bz < az * bx) { return -1; }
    if (ax * bx < 0.0f || az * bz < 0.0f) { return -1; }
    if (ax * ax + az * az < bx * bx + bz * bz) { return 1; }
    return 0;
}

static BOOL StanSegmentsCross(float sx, float sz, float ex, float ez,
                              const StanPoint *a, const StanPoint *b,
                              BOOL touching)
{
    float dx = ex - sx, dz = ez - sz;
    float ax = b->x - a->x, az = b->z - a->z;
    int side1 = StanDirection(dx, dz, a->x - sx, a->z - sz)
              * StanDirection(dx, dz, b->x - sx, b->z - sz);
    int side2 = StanDirection(ax, az, sx - a->x, sz - a->z)
              * StanDirection(ax, az, ex - a->x, ez - a->z);

    return side1 < touching && side2 < touching;
}

BOOL StanWalkTiles(const StanFile *stan, DWORD *index,
                   float sx, float sz, float ex, float ez)
{
    DWORD current, previous, beforeprevious, next = STAN_TILE_NONE;
    int iteration = 0;
    float dx, negdz, minx, maxx, minz, maxz;

    if (index == NULL || !StanHasTile(stan, *index)) { return FALSE; }
    sx *= stan->levelscale; sz *= stan->levelscale;
    ex *= stan->levelscale; ez *= stan->levelscale;
    dx = ex - sx; negdz = -(ez - sz);
    minx = fminf(sx, ex); maxx = fmaxf(sx, ex);
    minz = fminf(sz, ez); maxz = fmaxf(sz, ez);
    current = previous = beforeprevious = *index;
    for (;;)
    {
        StanTile querytile;
        const StanTile *tile = &querytile;
        int edge, crossings = 0;

        StanQueryTile(stan, current, &querytile);
        for (edge = 0; edge < tile->pointcount; edge++)
        {
            const StanPoint *a = &tile->points[edge];
            const StanPoint *b = &tile->points[(edge + 1) % tile->pointcount];
            BOOL linked = (a->link >> 4) != 0;
            DWORD destination;

            if (negdz * (b->x - a->x) + dx * (b->z - a->z) > 0.0f
                || (a->x < minx && b->x < minx) || (a->x > maxx && b->x > maxx)
                || (a->z < minz && b->z < minz) || (a->z > maxz && b->z > maxz)
                || !StanSegmentsCross(sx, sz, ex, ez, a, b, linked))
            {
                continue;
            }
            destination = StanLinkedTile(stan, a->link);
            crossings++;
            if (destination != previous && destination != beforeprevious)
            {
                next = destination;
            }
        }
        beforeprevious = previous;
        previous = current;
        if (current == next) { crossings = 0; }
        if (crossings == 0) { return TRUE; }
        if (iteration++ > 500 || next == STAN_TILE_NONE) { return FALSE; }
        current = next;
        *index = next;
    }
}

/* Radius-zero specialization of stanTestCircleCollisionNoCallbacks, used
   by stanFindNearestWalkablePosition. Only crossed edges can block it. */
static BOOL StanSampleIsWalkable(const StanFile *stan, DWORD index,
                                 float x, float z)
{
    DWORD visited[41];
    int count = 1, cursor = 0;

    visited[0] = index;
    x *= stan->levelscale;
    z *= stan->levelscale;
    while (cursor < count)
    {
        StanTile querytile;
        const StanTile *tile = &querytile;
        int edge;

        StanQueryTile(stan, visited[cursor++], &querytile);
        for (edge = 0; edge < tile->pointcount; edge++)
        {
            const StanPoint *a = &tile->points[edge];
            const StanPoint *b = &tile->points[(edge + 1) % tile->pointcount];
            float dx = b->x - a->x, dz = b->z - a->z;
            float projection = (x - a->x) * dx + (z - a->z) * dz;
            DWORD linked;
            int i;

            if (StanEdgeSide(a, b, x, z) >= 0.0f
                || projection <= 0.0f || projection >= dx * dx + dz * dz)
            {
                continue;
            }
            linked = StanLinkedTile(stan, a->link);
            if (linked == STAN_TILE_NONE) { return FALSE; }
            for (i = 0; i < count && visited[i] != linked; i++) { }
            if (i == count)
            {
                if (count >= 41) { return FALSE; }
                visited[count++] = linked;
            }
        }
        if (count >= 41) { return FALSE; }
    }
    return TRUE;
}

static DWORD StanTileIdFromName(const char *name)
{
    unsigned long number;
    char *end;
    unsigned int sub = 0;

    if (name == NULL || (name[0] != 'p' && name[0] != 'q')) { return STAN_TILE_NONE; }
    number = strtoul(name + 1, &end, 10);
    if (end == name + 1 || number > 32767 || *end < 'a' || *end > 'z')
    {
        return STAN_TILE_NONE;
    }
    if (end[1] != '\0')
    {
        if (end[1] < '0' || end[1] > '7' || end[2] != '\0') { return STAN_TILE_NONE; }
        sub = (unsigned int)(end[1] - '0');
    }
    return ((DWORD)(name[0] - 'p') << 23) | ((DWORD)number << 8)
         | ((DWORD)(*end - 'a') << 3) | sub;
}

DWORD StanResolvePadTile(const StanFile *stan, const char *name, const float pos[3])
{
    DWORD named = StanTileIdFromName(name), best = STAN_TILE_NONE, i;
    float bestdistance = 4294967296.0f; /* M_U32_MAX_VALUE_F in the game */
    float bestpos[3] = { 0.0f, 0.0f, 0.0f };
    float inverse;

    if (!StanHasTile(stan, 0) || pos == NULL) { return STAN_TILE_NONE; }
    inverse = 1.0f / stan->levelscale;
    for (i = 0; i < stan->tilecount; i++)
    {
        if (stan->tiles[i].id == named)
        {
            StanTile tile;

            StanQueryTile(stan, i, &tile);
            if (StanInsideTriple(&tile, pos[0] * stan->levelscale, pos[2] * stan->levelscale)) { return i; }
            break;
        }
    }

    /* Preserve sample order and strict distance comparisons. This is a
       3D nearest-sample search, not a below-first/above-second floor cast. */
    for (i = 0; i < stan->tilecount; i++)
    {
        StanTile querytile;
        const StanTile *tile = &querytile;
        int sample;

        StanQueryTile(stan, i, &querytile);
        const StanPoint *a = &tile->points[tile->extreme[0]];
        const StanPoint *b = &tile->points[tile->extreme[1]];
        const StanPoint *c = &tile->points[tile->extreme[2]];
        float midpoint[3] = { (a->x + b->x + c->x) / 3.0f * inverse,
                             (a->y + b->y + c->y) / 3.0f * inverse,
                             (a->z + b->z + c->z) / 3.0f * inverse };

        if ((tile->id & 0x800000u) || StanEdgeSide(a, b, c->x, c->z) == 0.0f)
        {
            continue;
        }
        for (sample = 0; sample < 4; sample++)
        {
            float candidate[3], distance, dx, dy, dz;

            if (sample == 3)
            {
                candidate[0] = midpoint[0]; candidate[1] = midpoint[1]; candidate[2] = midpoint[2];
            }
            else
            {
                const StanPoint *p = &tile->points[tile->extreme[sample]];

                candidate[0] = midpoint[0] * 0.1f + (p->x * inverse) * 0.9f;
                candidate[1] = midpoint[1] * 0.1f + (p->y * inverse) * 0.9f;
                candidate[2] = midpoint[2] * 0.1f + (p->z * inverse) * 0.9f;
            }
            dx = candidate[0] - pos[0]; dy = candidate[1] - pos[1]; dz = candidate[2] - pos[2];
            distance = dx * dx + dy * dy + dz * dz;
            if (distance < bestdistance && StanSampleIsWalkable(stan, i, candidate[0], candidate[2]))
            {
                best = i;
                bestdistance = distance;
                bestpos[0] = candidate[0]; bestpos[1] = candidate[1]; bestpos[2] = candidate[2];
            }
        }
    }
    if (best != STAN_TILE_NONE && StanWalkTiles(stan, &best, bestpos[0], bestpos[2], pos[0], pos[2]))
    {
        return best;
    }
    return STAN_TILE_NONE;
}
