#include "levelexport.h"
#include "gltf.h"
#include <math.h>
#include <stdlib.h>

static BOOL LevelExportMeters(BgVertex *vertices, DWORD triangles, const char **why)
{
    for (DWORD i = 0; i < triangles * 3; i++)
    {
        BgVertex *v = vertices + i;
        if (!isfinite(v->x) || !isfinite(v->y) || !isfinite(v->z)
            || !isfinite(v->s) || !isfinite(v->t))
        { *why = "The level contains a non-finite position or UV coordinate."; return FALSE; }
        v->x *= .01f; v->y *= .01f; v->z *= .01f;
    }
    return TRUE;
}

BOOL LevelExportBackground(const char *path, const char *projectdir,
    const BgDocument *document, const char **why)
{
    BgDocumentRenderMesh mesh = {0};
    GltfGeometryPart *parts = NULL;
    BOOL ok = FALSE;
    *why = "There is no background geometry to export.";
    if (!document || !document->rooms || !document->facecount) { return FALSE; }
    if (!BgDocumentBuildRenderMesh(document, &mesh, why)) { return FALSE; }
    parts = malloc((size_t)mesh.facecount * sizeof(*parts));
    if (!parts) { *why = "Out of memory preparing the background export."; goto done; }
    for (DWORD i = 0; i < mesh.facecount; i++)
    {
        parts[i].room = mesh.facerefs[i].room;
        parts[i].layer = mesh.facerefs[i].layer;
        /* BG stores per-face culling in its triangle tag; model exports use
         * render flags. Resolve it exactly as the background viewport does. */
        mesh.renderflags[i] &= ~BG_RENDER_CULL_MASK;
        mesh.renderflags[i] |= BG_RENDER_CULL_EXPLICIT
            | (BG_TRI_CULLS_BACK(mesh.tags[i]) ? BG_RENDER_CULL_BACK : 0);
    }
    if (LevelExportMeters(mesh.vertices, mesh.facecount, why))
        ok = GltfWriteGlb(path, projectdir, mesh.vertices, mesh.tags, mesh.renderflags,
            parts, mesh.facecount, FALSE, why);
done:
    free(parts); BgDocumentRenderMeshFree(&mesh);
    return ok;
}

BOOL LevelExportStans(const char *path, const StanFile *stan, const char **why)
{
    DWORD triangles = 0, at = 0;
    BgVertex *vertices = NULL;
    BgRenderFlags *flags = NULL;
    GltfGeometryPart *parts = NULL;
    BOOL ok = FALSE;
    *why = "There are no stan tiles to export.";
    if (!stan || !stan->tiles || !stan->tilecount) { return FALSE; }
    for (DWORD i = 0; i < stan->tilecount; i++)
    {
        unsigned int n = stan->tiles[i].pointcount;
        if (n < 3 || n > STAN_TILE_MAX_POINTS)
        { *why = "A stan tile has an invalid point count."; return FALSE; }
        if (triangles > 1000000u - (n - 2))
        { *why = "The stan geometry is too large to export."; return FALSE; }
        triangles += n - 2;
    }
    vertices = calloc((size_t)triangles * 3, sizeof(*vertices));
    flags = calloc(triangles, sizeof(*flags)); /* Opaque, double-sided. */
    parts = calloc(triangles, sizeof(*parts));
    if (!vertices || !flags || !parts)
    { *why = "Out of memory preparing the stan export."; goto done; }
    for (DWORD i = 0; i < stan->tilecount; i++)
    {
        const StanTile *tile = stan->tiles + i;
        for (unsigned int p = 1; p + 1 < tile->pointcount; p++, at++)
        {
            const unsigned int corners[3] = {0, p, p + 1};
            parts[at].room = tile->room;
            for (unsigned int c = 0; c < 3; c++)
            {
                const StanPoint *point = tile->points + corners[c];
                BgVertex *v = vertices + at * 3 + c;
                v->x = point->x; v->y = point->y; v->z = point->z;
                v->r = tile->red; v->g = tile->green; v->b = tile->blue; v->a = 255;
            }
        }
    }
    if (LevelExportMeters(vertices, triangles, why))
        ok = GltfWriteGlb(path, NULL, vertices, NULL, flags, parts, triangles, TRUE, why);
done:
    free(vertices); free(flags); free(parts);
    return ok;
}
