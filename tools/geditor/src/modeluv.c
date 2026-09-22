#include "modeluv.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>

typedef struct ModelUVNode {
    DWORD offset, corner, triangle;
    int width, height;
    float uv[2], position[3];
} ModelUVNode;
static int NodeCompare(const void *a, const void *b)
{
    const ModelUVNode *x = a, *y = b;
    if (x->offset != y->offset) return x->offset < y->offset ? -1 : 1;
    if (x->width != y->width) return x->width < y->width ? -1 : 1;
    if (x->height != y->height) return x->height < y->height ? -1 : 1;
    for (int k = 0; k < 2; k++) if (x->uv[k] != y->uv[k]) return x->uv[k] < y->uv[k] ? -1 : 1;
    for (int k = 0; k < 3; k++) if (x->position[k] != y->position[k]) return x->position[k] < y->position[k] ? -1 : 1;
    return 0;
}
static int NodeOrder(const void *a, const void *b)
{
    int order = NodeCompare(a,b);
    DWORD x = ((const ModelUVNode *)a)->corner, y = ((const ModelUVNode *)b)->corner;
    return order ? order : x < y ? -1 : x > y;
}

BOOL ModelUVBuild(const ModelSource *source, const BgFaceRef *faces, DWORD count,
    const char *project, UVCanvasTriangle **out, const char **why)
{
    UVCanvasTriangle *triangles = NULL;
    ModelUVNode *nodes = NULL;
    BOOL ok = FALSE;
    *out = NULL; *why = "The selected model faces are no longer available.";
    if (!count) { *why = ""; return TRUE; }
    if (!source || !faces || !source->vertexoffsets || !source->vertices
        || count > source->count || count > INT_MAX / 3
        || source->materials.facecount != source->count) return FALSE;
    triangles = calloc(count, sizeof(*triangles));
    nodes = calloc((size_t)count * 3, sizeof(*nodes));
    if (!triangles || !nodes) { *why = "Out of memory displaying model UVs."; goto done; }
    for (DWORD i = 0; i < count; i++)
    {
        DWORD face = faces[i].faceid - 1;
        if (faces[i].room != 1 || face >= source->count) goto done;
        const ModelMaterialFace *material = &source->materials.faces[face];
        if (material->slot >= source->materials.count) goto done;
        if (source->flags[face] & BG_RENDER_ENVIRONMENT_MASK)
        { *why = "Reflection UVs are generated in game. Select faces with authored UVs to edit."; goto done; }
        DWORD texture = source->materials.slots[material->slot].texture;
        /* No Texture keeps normalized UVs in the editor trailer. A 32x32
         * workspace basis permits editing them before assigning an image. */
        int width = 32, height = 32;
        if (texture != BG_TEX_NONE && !TexGetProjectImageSize(project, texture, &width, &height))
        { *why = "A selected model image is unavailable in this project."; goto done; }
        if (width <= 0 || height <= 0 || width > 256 || height > 256) goto done;
        UVCanvasTriangle *triangle = &triangles[i];
        triangle->face = faces[i]; triangle->width = width; triangle->height = height;
        for (DWORD k = 0; k < 3; k++)
        {
            DWORD corner = face * 3 + k;
            const BgVertex *v = &source->vertices[corner];
            ModelUVNode *node = &nodes[i * 3 + k];
            node->offset = source->vertexoffsets[corner]; node->corner = corner; node->triangle = i * 3 + k;
            node->width = width; node->height = height;
            node->position[0] = v->x; node->position[1] = v->y; node->position[2] = v->z;
            for (int axis = 0; axis < 3; axis++)
            {
                if (!isfinite(node->position[axis])) goto done;
                triangle->position[k][axis] = node->position[axis];
            }
            for (int axis = 0; axis < 2; axis++)
            {
                node->uv[axis] = material->uv[k * 2 + axis];
                double st = round((double)node->uv[axis] * 32 * (axis ? height : width));
                if (!isfinite(st) || st < -32768 || st > 32767)
                { *why = "A selected UV exceeds the workspace's signed 16-bit texture-coordinate range."; goto done; }
                triangle->uv[k][axis] = node->uv[axis];
                if (axis) triangle->source[k].t = (int)st;
                else triangle->source[k].s = (int)st;
            }
        }
    }
    qsort(nodes, (size_t)count * 3, sizeof(*nodes), NodeOrder);
    DWORD first = 0;
    for (DWORD i = 0; i < count * 3; i++)
    {
        if (NodeCompare(&nodes[first], &nodes[i])) first = i;
        BgDocumentUVEdit *edit = &triangles[nodes[i].triangle / 3].source[nodes[i].triangle % 3];
        edit->vertex.room = 1; edit->vertex.index = nodes[first].corner;
        edit->vertexid = nodes[first].corner + 1;
    }
    *out = triangles; triangles = NULL; *why = ""; ok = TRUE;
done:
    free(triangles); free(nodes); return ok;
}

static int EditOrder(const void *a, const void *b)
{
    DWORD x = ((const BgDocumentUVEdit *)a)->vertexid, y = ((const BgDocumentUVEdit *)b)->vertexid;
    return x < y ? -1 : x > y;
}
static int FaceOrder(const void *a, const void *b)
{
    DWORD x = ((const BgDocumentFaceUVEdit *)a)->face.faceid, y = ((const BgDocumentFaceUVEdit *)b)->face.faceid;
    return x < y ? -1 : x > y;
}
BOOL ModelUVConvert(const UVCanvasTriangle *triangles, DWORD count,
    const UVCanvasEdit *vertices, const UVCanvasFaceEdit *faces,
    ModelUVEdit **out, DWORD *outcount, const char **why)
{
    ModelUVEdit *edits = NULL;
    BgDocumentUVEdit *nodes = NULL;
    BgDocumentFaceUVEdit *unwrap = NULL;
    unsigned char *seen = NULL;
    DWORD used = 0, requests;
    BOOL ok = FALSE;
    *out = NULL; *outcount = 0;
    *why = "The model UV selection changed. Select its faces again.";
    if ((!vertices == !faces) || (count && !triangles) || count > INT_MAX / 3) return FALSE;
    requests = vertices ? vertices->count : faces->count;
    if (requests > count * 3 || (requests && !(vertices ? (const void *)vertices->vertices : (const void *)faces->faces))) return FALSE;
    if (!requests) { *why = ""; return TRUE; }
    edits = malloc((size_t)count * 3 * sizeof(*edits));
    seen = calloc(requests, 1);
    if (vertices) nodes = malloc((size_t)requests * sizeof(*nodes));
    else unwrap = malloc((size_t)requests * sizeof(*unwrap));
    if (!edits || !seen || (!nodes && !unwrap)) { *why = "Out of memory applying model UVs."; goto done; }
    if (nodes)
    {
        memcpy(nodes, vertices->vertices, (size_t)requests * sizeof(*nodes));
        qsort(nodes, requests, sizeof(*nodes), EditOrder);
        for (DWORD i = 0; i < requests; i++)
            if (nodes[i].vertex.room != 1 || nodes[i].vertexid != nodes[i].vertex.index + 1
                || (i && nodes[i-1].vertexid == nodes[i].vertexid)) goto done;
    }
    else
    {
        memcpy(unwrap, faces->faces, (size_t)requests * sizeof(*unwrap));
        qsort(unwrap, requests, sizeof(*unwrap), FaceOrder);
        for (DWORD i = 0; i < requests; i++)
            if (unwrap[i].face.room != 1 || (i && unwrap[i-1].face.faceid == unwrap[i].face.faceid)) goto done;
    }
    for (DWORD i = 0; i < count; i++)
    {
        const UVCanvasTriangle *triangle = &triangles[i];
        BgDocumentFaceUVEdit key = {0}; key.face = triangle->face;
        const BgDocumentFaceUVEdit *face = unwrap ? bsearch(&key, unwrap, requests, sizeof(*unwrap), FaceOrder) : NULL;
        if (face) seen[face - unwrap] = 1;
        for (DWORD k = 0; k < 3; k++)
        {
            const BgDocumentUVEdit *source = &triangle->source[k];
            const BgDocumentUVEdit *vertex = nodes ? bsearch(source, nodes, requests, sizeof(*nodes), EditOrder) : NULL;
            if (!face && !vertex) continue;
            if (face && face->vertexids[k] != source->vertexid) goto done;
            if (vertex) seen[vertex - nodes] = 1;
            int s = face ? face->s[k] : vertex->s, t = face ? face->t[k] : vertex->t;
            if (s < -32768 || s > 32767 || t < -32768 || t > 32767
                || triangle->width <= 0 || triangle->height <= 0)
            { *why = "A model UV exceeds the signed 16-bit texture-coordinate range."; goto done; }
            if (s == source->s && t == source->t) continue;
            edits[used].corner = (triangle->face.faceid - 1) * 3 + k;
            /* Preserve untouched authored precision, including No Texture. */
            edits[used].uv[0] = s == source->s ? triangle->uv[k][0] : s / (32.0 * triangle->width);
            edits[used].uv[1] = t == source->t ? triangle->uv[k][1] : t / (32.0 * triangle->height);
            used++;
        }
    }
    for (DWORD i = 0; i < requests; i++) if (!seen[i]) goto done;
    *out = edits; *outcount = used; edits = NULL; *why = ""; ok = TRUE;
done:
    free(edits); free(nodes); free(unwrap); free(seen); return ok;
}
