/* Edits preserve stan record sizes, tile IDs, room/special flags and links.
 * Raw bytes are updated with their decoded view so save and ROM export use
 * exactly the same document as the viewport and placement queries. */
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "stanload.h"

static DWORD StanPointRoot(DWORD *map, DWORD point)
{
    while (map[point] != point)
    {
        map[point] = map[map[point]];
        point = map[point];
    }
    return point;
}

DWORD *StanBuildPointMap(const StanFile *stan, const char **reasonout)
{
    DWORD *map, tile, i;
    *reasonout = "";
    if (stan == NULL || stan->tiles == NULL || stan->tilecount == 0)
    {
        *reasonout = "there are no stan points to edit.";
        return NULL;
    }
    map = malloc((size_t)stan->tilecount * STAN_TILE_MAX_POINTS * sizeof(*map));
    if (map == NULL)
    {
        *reasonout = "out of memory resolving shared stan points.";
        return NULL;
    }
    for (i = 0; i < stan->tilecount * STAN_TILE_MAX_POINTS; i++) { map[i] = i; }
    for (tile = 0; tile < stan->tilecount; tile++)
    {
        const StanTile *a = &stan->tiles[tile];
        unsigned int edge;
        for (edge = 0; edge < a->pointcount; edge++)
        {
            DWORD linked = StanLinkedTile(stan, a->points[edge].link);
            unsigned int end, other;
            if (linked == STAN_TILE_NONE) { continue; }
            for (end = 0; end < 2; end++)
            {
                unsigned int point = (edge + end) % a->pointcount;
                const StanPoint *v = &a->points[point];
                for (other = 0; other < stan->tiles[linked].pointcount; other++)
                {
                    const StanPoint *w = &stan->tiles[linked].points[other];
                    if (v->x == w->x && v->y == w->y && v->z == w->z)
                    {
                        DWORD first = StanPointRoot(map, tile * STAN_TILE_MAX_POINTS + point);
                        DWORD second = StanPointRoot(map, linked * STAN_TILE_MAX_POINTS + other);
                        /* The smallest record identity is stable across rebuilds. */
                        if (first < second) { map[second] = first; }
                        else { map[first] = second; }
                    }
                }
            }
        }
    }
    for (i = 0; i < stan->tilecount * STAN_TILE_MAX_POINTS; i++) { map[i] = StanPointRoot(map, i); }
    return map;
}

static short StanEditRead16(const unsigned char *p)
{
    return (short)(((unsigned int)p[0] << 8) | p[1]);
}

static void StanEditWrite16(unsigned char *p, unsigned short value)
{
    p[0] = (unsigned char)(value >> 8);
    p[1] = (unsigned char)value;
}

/* Runtime height queries and interior samples use this representative
 * triangle. Choose the widest XZ triple after deformation, retaining perimeter
 * order. Vertical tiles use the widest 3D triple when all XZ areas are zero. */
static void StanUpdateRepresentativeTriangle(StanFile *stan, DWORD index)
{
    StanTile *tile = &stan->tiles[index];
    double bestxz = -1, bestarea = -1;
    unsigned int i, j, k;
    double position[STAN_TILE_MAX_POINTS][3];
    for (i = 0; i < tile->pointcount; i++)
    {
        const unsigned char *raw = stan->data + tile->sourceoffset + 8 + i * 8;
        for (j = 0; j < 3; j++) { position[i][j] = StanEditRead16(raw + j * 2); }
    }
    for (i = 0; i + 2 < tile->pointcount; i++)
    for (j = i + 1; j + 1 < tile->pointcount; j++)
    for (k = j + 1; k < tile->pointcount; k++)
    {
        const double *a = position[i], *b = position[j], *c = position[k];
        double abx = b[0]-a[0], aby = b[1]-a[1], abz = b[2]-a[2];
        double acx = c[0]-a[0], acy = c[1]-a[1], acz = c[2]-a[2];
        double nx = aby*acz-abz*acy, ny = abz*acx-abx*acz, nz = abx*acy-aby*acx;
        double xz = fabs(ny), area = nx*nx+ny*ny+nz*nz;
        if (xz > bestxz || (xz == bestxz && area > bestarea))
        {
            bestxz = xz; bestarea = area;
            tile->extreme[0] = (unsigned char)i;
            tile->extreme[1] = (unsigned char)j;
            tile->extreme[2] = (unsigned char)k;
        }
    }
    StanEditWrite16(stan->data + tile->sourceoffset + 6,
        (unsigned short)((tile->pointcount << 12) | (tile->extreme[0] << 8)
                       | (tile->extreme[1] << 4) | tile->extreme[2]));
}

BOOL StanTranslatePoints(StanFile *stan, const StanPointRef *points, DWORD count,
                          const double offset[3], DWORD *movedout,
                          const char **reasonout)
{
    DWORD *map = NULL, i, tile;
    unsigned char *selected = NULL;
    double delta[3];
    int axis;
    *movedout = 0;
    *reasonout = "";
    if (stan == NULL || stan->data == NULL || points == NULL || count == 0
        || !(stan->levelscale > 0) || !isfinite(stan->levelscale))
    {
        *reasonout = "there are no editable selected stan points.";
        return FALSE;
    }
    for (axis = 0; axis < 3; axis++)
    {
        delta[axis] = round(offset[axis] * stan->levelscale);
        if (!isfinite(delta[axis]) || fabs(delta[axis]) > 65535)
        {
            *reasonout = "the move exceeds the stan coordinate range.";
            return FALSE;
        }
    }
    if (delta[0] == 0 && delta[1] == 0 && delta[2] == 0) { return TRUE; }
    map = StanBuildPointMap(stan, reasonout);
    if (map == NULL) { return FALSE; }
    selected = calloc((size_t)stan->tilecount * STAN_TILE_MAX_POINTS, 1);
    if (selected == NULL)
    {
        *reasonout = "out of memory collecting stan points to move.";
        goto fail;
    }
    for (i = 0; i < count; i++)
    {
        if (points[i].tile >= stan->tilecount || points[i].point >= stan->tiles[points[i].tile].pointcount)
        {
            *reasonout = "a selected stan point no longer exists.";
            goto fail;
        }
        selected[map[points[i].tile * STAN_TILE_MAX_POINTS + points[i].point]] = 1;
    }
    /* Validate the complete linked selection before writing any bytes. */
    for (tile = 0; tile < stan->tilecount; tile++)
    for (i = 0; i < stan->tiles[tile].pointcount; i++)
    {
        const unsigned char *raw = stan->data + stan->tiles[tile].sourceoffset + 8 + i * 8;
        if (!selected[map[tile * STAN_TILE_MAX_POINTS + i]]) { continue; }
        for (axis = 0; axis < 3; axis++)
        {
            double value = StanEditRead16(raw + axis * 2) + delta[axis];
            if (value < SHRT_MIN || value > SHRT_MAX)
            {
                *reasonout = "the move would place a stan point outside its signed 16-bit coordinate range.";
                goto fail;
            }
        }
    }
    for (tile = 0; tile < stan->tilecount; tile++)
    {
        unsigned int movedpoints = 0;
        for (i = 0; i < stan->tiles[tile].pointcount; i++)
        {
            unsigned char *raw = stan->data + stan->tiles[tile].sourceoffset + 8 + i * 8;
            StanPoint *point = &stan->tiles[tile].points[i];
            float position[3];
            if (!selected[map[tile * STAN_TILE_MAX_POINTS + i]]) { continue; }
            for (axis = 0; axis < 3; axis++)
            {
                short value = (short)(StanEditRead16(raw + axis * 2) + delta[axis]);
                StanEditWrite16(raw + axis * 2, (unsigned short)value);
                position[axis] = value * (1.0f / stan->levelscale);
            }
            point->x = position[0]; point->y = position[1]; point->z = position[2];
            movedpoints++;
            (*movedout)++;
        }
        /* A uniform translation leaves the authored representative triangle
           valid. Only deformed tiles need their triple recalculated. */
        if (movedpoints > 0 && movedpoints < stan->tiles[tile].pointcount)
        {
            StanUpdateRepresentativeTriangle(stan, tile);
        }
    }
    stan->dirty = *movedout != 0 || stan->dirty;
    free(selected); free(map);
    return TRUE;
fail:
    free(selected); free(map);
    return FALSE;
}

BOOL StanPaintTile(StanFile *stan, DWORD index, const unsigned char rgba[4],
                   BOOL *changedout, const char **reasonout)
{
    unsigned char red, green, blue;
    unsigned char *raw;
    StanTile *tile;
    *changedout = FALSE;
    *reasonout = "";
    if (stan == NULL || stan->data == NULL || index >= stan->tilecount)
    {
        *reasonout = "the selected stan tile no longer exists.";
        return FALSE;
    }
    tile = &stan->tiles[index];
    red = (unsigned char)((rgba[0] + 8) / 17);
    green = (unsigned char)((rgba[1] + 8) / 17);
    blue = (unsigned char)((rgba[2] + 8) / 17);
    if (tile->red == red * 17 && tile->green == green * 17 && tile->blue == blue * 17) { return TRUE; }
    raw = stan->data + tile->sourceoffset + 4;
    raw[0] = (unsigned char)((raw[0] & 0xf0) | red);
    raw[1] = (unsigned char)((green << 4) | blue);
    tile->red = red * 17; tile->green = green * 17; tile->blue = blue * 17;
    stan->dirty = TRUE;
    *changedout = TRUE;
    return TRUE;
}

BOOL StanRotatePoints(StanFile *stan, const StanPointRef *points, DWORD count,
                      const Rotation *rotation, const double pivot[3], DWORD *movedout,
                      const char **reasonout)
{
    DWORD *map = NULL, i, tile;
    unsigned char *selected = NULL;
    int axis;
    *movedout = 0;
    *reasonout = "";
    if (stan == NULL || stan->data == NULL || points == NULL || count == 0 ||
        !(stan->levelscale > 0) || !isfinite(stan->levelscale))
    {
        *reasonout = "there are no editable selected stan points.";
        return FALSE;
    }
    if (!pivot || !RotationValid(rotation))
    {
        *reasonout = "Invalid stan rotation.";
        return FALSE;
    }
    map = StanBuildPointMap(stan, reasonout);
    if (map == NULL)
    {
        return FALSE;
    }
    selected = calloc((size_t)stan->tilecount * STAN_TILE_MAX_POINTS, 1);
    if (selected == NULL)
    {
        *reasonout = "out of memory collecting stan points to move.";
        goto fail;
    }
    for (i = 0; i < count; i++)
    {
        if (points[i].tile >= stan->tilecount ||
            points[i].point >= stan->tiles[points[i].tile].pointcount)
        {
            *reasonout = "a selected stan point no longer exists.";
            goto fail;
        }
        selected[map[points[i].tile * STAN_TILE_MAX_POINTS + points[i].point]] = 1;
    }
    /* Validate the complete linked selection before writing any bytes. */
    for (tile = 0; tile < stan->tilecount; tile++)
    {
        for (i = 0; i < stan->tiles[tile].pointcount; i++)
        {
            const unsigned char *raw = stan->data + stan->tiles[tile].sourceoffset + 8 + i * 8;
            if (!selected[map[tile * STAN_TILE_MAX_POINTS + i]])
            {
                continue;
            }
            double source[3], destination[3];
            for (axis = 0; axis < 3; axis++)
            {
                source[axis] = StanEditRead16(raw + axis * 2) / stan->levelscale;
            }
            RotationPoint(rotation, pivot, source, destination);
            for (axis = 0; axis < 3; axis++)
            {
                double value = round(destination[axis] * stan->levelscale);
                if (!isfinite(value) || value < SHRT_MIN || value > SHRT_MAX)
                {
                    *reasonout = "the move would place a stan point outside its signed 16-bit "
                                 "coordinate range.";
                    goto fail;
                }
            }
        }
    }
    for (tile = 0; tile < stan->tilecount; tile++)
    {
        unsigned int movedpoints = 0;
        for (i = 0; i < stan->tiles[tile].pointcount; i++)
        {
            unsigned char *raw = stan->data + stan->tiles[tile].sourceoffset + 8 + i * 8;
            StanPoint *point = &stan->tiles[tile].points[i];
            float position[3];
            double source[3], destination[3];
            if (!selected[map[tile * STAN_TILE_MAX_POINTS + i]])
            {
                continue;
            }
            for (axis = 0; axis < 3; axis++)
            {
                source[axis] = StanEditRead16(raw + axis * 2) / stan->levelscale;
            }
            RotationPoint(rotation, pivot, source, destination);
            for (axis = 0; axis < 3; axis++)
            {
                short value = (short)round(destination[axis] * stan->levelscale);
                StanEditWrite16(raw + axis * 2, (unsigned short)value);
                position[axis] = value * (1.0f / stan->levelscale);
            }
            point->x = position[0];
            point->y = position[1];
            point->z = position[2];
            movedpoints++;
            (*movedout)++;
        }
        /* Rotation changes which triple best represents the tile in XZ. */
        if (movedpoints > 0)
        {
            StanUpdateRepresentativeTriangle(stan, tile);
        }
    }
    stan->dirty = *movedout != 0 || stan->dirty;
    free(selected);
    free(map);
    return TRUE;
fail:
    free(selected);
    free(map);
    return FALSE;
}
