#include "bgdocument.h"
#include <stdlib.h>
#include <string.h>

static int BgUVCompareEdits(const void *a, const void *b)
{
    const BgDocumentUVEdit *x = a, *y = b;
    if (x->vertex.room != y->vertex.room) { return x->vertex.room < y->vertex.room ? -1 : 1; }
    if (x->vertex.index != y->vertex.index) { return x->vertex.index < y->vertex.index ? -1 : 1; }
    return 0;
}

BOOL BgDocumentSetVertexUVs(BgDocument *document, const BgDocumentUVEdit *edits,
                           DWORD count, DWORD *changed, const char **reason)
{
    BgDocumentUVEdit *sorted;
    DWORD i;
    *changed = 0;
    *reason = "Invalid UV edit.";
    if (document == NULL || document->rooms == NULL || edits == NULL || count == 0
        || count > (DWORD)-1 / sizeof(*sorted)) { return FALSE; }
    sorted = malloc((size_t)count * sizeof(*sorted));
    if (sorted == NULL) { *reason = "Out of memory editing UVs."; return FALSE; }
    memcpy(sorted, edits, (size_t)count * sizeof(*sorted));
    qsort(sorted, count, sizeof(*sorted), BgUVCompareEdits);
    for (i = 0; i < count; i++)
    {
        const BgDocumentUVEdit *edit = &sorted[i];
        const BgDocumentRoom *room = BgDocumentGetRoom(document, edit->vertex.room);
        if (room == NULL || room->vertices == NULL || edit->vertex.index >= room->vertexcount
            || room->vertices[edit->vertex.index].id != edit->vertexid)
        {
            *reason = "A UV vertex is no longer available.";
            free(sorted); return FALSE;
        }
        if (edit->s < -32768 || edit->s > 32767 || edit->t < -32768 || edit->t > 32767)
        {
            *reason = "UV coordinates exceed GoldenEye's signed 16-bit texture coordinate range.";
            free(sorted); return FALSE;
        }
        if (i > 0 && BgUVCompareEdits(edit, &sorted[i - 1]) == 0
            && (edit->s != sorted[i - 1].s || edit->t != sorted[i - 1].t))
        {
            *reason = "A shared UV vertex received conflicting coordinates.";
            free(sorted); return FALSE;
        }
    }
    for (i = 0; i < count; i++)
    {
        const BgDocumentUVEdit *edit = &sorted[i];
        BgDocumentVertex *vertex = &document->rooms[edit->vertex.room].vertices[edit->vertex.index];
        if (vertex->s != edit->s || vertex->t != edit->t)
        {
            vertex->s = (short)edit->s;
            vertex->t = (short)edit->t;
            (*changed)++;
        }
    }
    if (*changed != 0) { document->dirty = TRUE; }
    free(sorted);
    *reason = "";
    return TRUE;
}
