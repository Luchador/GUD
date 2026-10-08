#include "bgstan.h"

#include <math.h>
#include <stdlib.h>

static int CompareFaceIds(const void *left, const void *right)
{
    DWORD a = *(const DWORD *)left, b = *(const DWORD *)right;
    return a < b ? -1 : a > b;
}

BOOL BgCreateStanFromFaces(const BgDocument *bg, const BgFaceRef *faces, DWORD count,
    StanFile *stan, DWORD *out, const char **why)
{
    StanTriangle *triangles = NULL;
    DWORD *ids = NULL;
    BOOL ok = FALSE;
    if (!bg || !bg->rooms || !faces || !count || count > 0x7fffu || !out
        || !isfinite(bg->levelscale) || bg->levelscale <= 0)
    { *why = "Select background faces in a loaded level to create stan tiles."; return FALSE; }
    triangles = calloc(count, sizeof(*triangles));
    ids = malloc((size_t)count * sizeof(*ids));
    if (!triangles || !ids) { *why = "Out of memory creating stan tiles."; goto done; }
    for (DWORD i = 0; i < count; i++)
    {
        const BgDocumentFace *face = BgDocumentFindFace(bg, &faces[i], NULL);
        if (!face || !face->room || face->room > STAN_MAX_ROOM)
        { *why = "A selected background face is missing or has an invalid stan room."; goto done; }
        const BgDocumentRoom *room = &bg->rooms[face->room];
        StanTriangle *triangle = &triangles[i];
        unsigned int rgb[3] = {0};
        ids[i] = face->id;
        triangle->room = (unsigned char)face->room;
        for (int p = 0; p < 3; p++)
        {
            if (!room->vertices || face->vertexindices[p] >= room->vertexcount)
            { *why = "A selected background face contains an invalid vertex."; goto done; }
            const BgDocumentVertex *v = &room->vertices[face->vertexindices[p]];
            const double local[3] = {v->x, v->y, v->z};
            for (int k = 0; k < 3; k++)
            { triangle->points[p][k] = (local[k] + room->origin[k]) / bg->levelscale; }
            rgb[0] += v->r; rgb[1] += v->g; rgb[2] += v->b;
        }
        triangle->red = (unsigned char)((rgb[0] + 1) / 3);
        triangle->green = (unsigned char)((rgb[1] + 1) / 3);
        triangle->blue = (unsigned char)((rgb[2] + 1) / 3);
    }
    qsort(ids, count, sizeof(*ids), CompareFaceIds);
    for (DWORD i = 1; i < count; i++) if (ids[i] == ids[i-1])
    { *why = "The background face selection contains duplicates."; goto done; }
    ok = StanCreateTriangles(stan, triangles, count, out, why);
done:
    free(triangles); free(ids); return ok;
}
