/* Preserve texture scale when a boundary vertex/edge is translated.
 * Work in native coordinates, so correction follows the snapped geometry. */
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "bgdocument.h"

static BOOL BgCorrectMoved(const BgDocumentVertex *a, const BgDocumentVertex *b)
{ return a->x != b->x || a->y != b->y || a->z != b->z; }

static double BgCorrectDot(const double a[3], const double b[3])
{ return a[0]*b[0] + a[1]*b[1] + a[2]*b[2]; }

static void BgCorrectSubtract(const BgDocumentVertex *a, const BgDocumentVertex *b, double out[3])
{ out[0] = a->x - (double)b->x; out[1] = a->y - (double)b->y; out[2] = a->z - (double)b->z; }

/* Two corners have the same displacement; their edge is the hinge. Unfold
 * the third corner onto the original plane, retaining its distance from the
 * edge even for out-of-plane moves, just like edge extrusion. */
static void BgCorrectFace(const BgDocumentRoom *old, const BgDocumentRoom *now,
    const BgDocumentFace *face, double (*sum)[3])
{
    const BgDocumentVertex *v[3], *n[3];
    BOOL moved[3];
    int count = 0, a = -1, b = -1, c = -1;
    double edge[3], across[3], next[3], length, projection, newprojection, height, newheight, weight;
    if (face->textureid == BG_TEX_NONE) { return; }
    for (int i = 0; i < 3; i++)
    {
        v[i] = &old->vertices[face->vertexindices[i]];
        n[i] = &now->vertices[face->vertexindices[i]];
        moved[i] = BgCorrectMoved(v[i], n[i]); count += moved[i];
    }
    if (!count || count == 3) { return; }
    for (int i = 0; i < 3; i++)
    {
        if (moved[i] == (count == 2)) { if (a < 0) { a = i; } else { b = i; } }
        else { c = i; }
    }
    BgCorrectSubtract(v[b], v[a], edge);
    BgCorrectSubtract(v[c], v[a], across);
    BgCorrectSubtract(n[c], n[a], next);
    length = sqrt(BgCorrectDot(edge, edge));
    if (length < 1e-9) { return; }
    for (int i = 0; i < 3; i++) { edge[i] /= length; }
    projection = BgCorrectDot(across, edge); newprojection = BgCorrectDot(next, edge);
    for (int i = 0; i < 3; i++)
    { across[i] -= projection*edge[i]; next[i] -= newprojection*edge[i]; }
    height = sqrt(BgCorrectDot(across, across));
    if (height < 1e-9) { return; }
    newheight = sqrt(BgCorrectDot(next, next));
    if (BgCorrectDot(across, next) < 0) { newheight = -newheight; }
    weight = length*height;
    for (int axis = 0; axis < 2; axis++)
    {
        double av = axis ? v[a]->t : v[a]->s, bv = axis ? v[b]->t : v[b]->s;
        double cv = axis ? v[c]->t : v[c]->s;
        double along = (bv-av)/length, perpendicular = (cv-av-along*projection)/height;
        double shift = along*(newprojection-projection) + perpendicular*(newheight-height);
        if (count == 2) { shift = -shift; }
        for (int i = 0; i < 3; i++) if (moved[i])
        { sum[face->vertexindices[i]][axis] += shift*weight; }
    }
    for (int i = 0; i < 3; i++) if (moved[i]) { sum[face->vertexindices[i]][2] += weight; }
}

BOOL BgDocumentCorrectMovedUVs(const BgDocument *before, BgDocument *document,
    const char **reasonout)
{
    BgDocumentUVEdit *edits = NULL;
    double (*sum)[3] = NULL;
    DWORD count = 0, at = 0, changed;
    BOOL ok = FALSE;
    *reasonout = "The background topology changed before UV correction.";
    if (!before || !document || !before->rooms || !document->rooms
        || before->roomcount != document->roomcount) { return FALSE; }
    for (DWORD r = 1; r <= document->roomcount; r++)
    {
        const BgDocumentRoom *old = &before->rooms[r], *now = &document->rooms[r];
        if (old->vertexcount != now->vertexcount || old->facecount != now->facecount) { return FALSE; }
        for (DWORD i = 0; i < now->vertexcount; i++)
        {
            if (old->vertices[i].id != now->vertices[i].id) { return FALSE; }
            if (BgCorrectMoved(&old->vertices[i], &now->vertices[i]))
            { if (count == (DWORD)-1 / sizeof(*edits)) { return FALSE; } count++; }
        }
        for (DWORD i = 0; i < now->facecount; i++)
        {
            const BgDocumentFace *a = &old->faces[i], *b = &now->faces[i];
            if (a->id != b->id || memcmp(a->vertexindices, b->vertexindices, sizeof(a->vertexindices))) { return FALSE; }
            for (int c = 0; c < 3; c++) { if (a->vertexindices[c] >= now->vertexcount) { return FALSE; } }
        }
    }
    if (!count) { *reasonout = ""; return TRUE; }
    *reasonout = "Out of memory correcting face UVs.";
    edits = malloc((size_t)count * sizeof(*edits));
    if (!edits) { return FALSE; }
    for (DWORD r = 1; r <= document->roomcount; r++)
    {
        const BgDocumentRoom *old = &before->rooms[r], *now = &document->rooms[r];
        DWORD first;
        for (first = 0; first < now->vertexcount; first++)
        { if (BgCorrectMoved(&old->vertices[first], &now->vertices[first])) { break; } }
        if (first == now->vertexcount) { continue; }
        sum = calloc(now->vertexcount, sizeof(*sum));
        if (!sum) { goto done; }
        for (DWORD f = 0; f < now->facecount; f++) { BgCorrectFace(old, now, &old->faces[f], sum); }
        for (DWORD i = first; i < now->vertexcount; i++) if (sum[i][2] > 0)
        {
            const BgDocumentVertex *v = &old->vertices[i];
            double s = round(v->s + sum[i][0]/sum[i][2]), t = round(v->t + sum[i][1]/sum[i][2]);
            if (!isfinite(s) || !isfinite(t) || s < -32768 || s > 32767 || t < -32768 || t > 32767)
            { *reasonout = "Corrected UVs exceed the native range. Use a smaller move or turn off Correct Face Attributes."; goto done; }
            edits[at++] = (BgDocumentUVEdit){{r, i}, v->id, (int)s, (int)t};
        }
        free(sum); sum = NULL;
    }
    ok = !at || BgDocumentSetVertexUVs(document, edits, at, &changed, reasonout);
    if (ok) { *reasonout = ""; }
done:
    free(sum); free(edits);
    return ok;
}
