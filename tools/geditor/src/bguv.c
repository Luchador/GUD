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

typedef struct BgUVCorner {
    DWORD room, face, corner, vertex;
    int s,t;
} BgUVCorner;

static int BgUVCompareCorners(const void *a, const void *b)
{
    const BgUVCorner *x=a,*y=b;
    if (x->room!=y->room) { return x->room<y->room ? -1 : 1; }
    if (x->vertex!=y->vertex) { return x->vertex<y->vertex ? -1 : 1; }
    if (x->s!=y->s) { return x->s<y->s ? -1 : 1; }
    if (x->t!=y->t) { return x->t<y->t ? -1 : 1; }
    return 0;
}
static int BgUVCompareFaces(const void *a, const void *b)
{
    const BgDocumentFaceUVEdit *x=a,*y=b;
    if (x->face.room!=y->face.room) { return x->face.room<y->face.room ? -1 : 1; }
    if (x->face.faceid!=y->face.faceid) { return x->face.faceid<y->face.faceid ? -1 : 1; }
    return 0;
}

BOOL BgDocumentSetFaceUVs(BgDocument *document, const BgDocumentFaceUVEdit *edits,
    DWORD count, DWORD *changed, const char **reason)
{
    BgDocument staged={0};
    BgDocumentFaceUVEdit *sorted=NULL;
    BgUVCorner *corners=NULL;
    DWORD changedcorners=0;
    BOOL ok=FALSE;
    *changed=0; *reason="Invalid face UV edit.";
    if (!document || !document->rooms || !edits || !count
        || count>(DWORD)-1/sizeof(*sorted) || count>(DWORD)-1/(3*sizeof(*corners))) { return FALSE; }
    sorted=malloc((size_t)count*sizeof(*sorted)); corners=malloc((size_t)count*3*sizeof(*corners));
    *reason="Out of memory preparing UV seams.";
    if (!sorted || !corners) { goto done; }
    memcpy(sorted,edits,(size_t)count*sizeof(*sorted));
    qsort(sorted,count,sizeof(*sorted),BgUVCompareFaces);
    for (DWORD i=0; i<count; i++)
    {
        const BgDocumentFaceUVEdit *edit=&sorted[i];
        const BgDocumentRoom *room;
        const BgDocumentFace *face=BgDocumentFindFace(document,&edit->face,&room);
        if (!face || (i && !BgUVCompareFaces(edit,&sorted[i-1])))
        { *reason="A UV face is missing or duplicated in the selection."; goto done; }
        for (DWORD c=0; c<3; c++)
        {
            DWORD v=face->vertexindices[c];
            if (v>=room->vertexcount || room->vertices[v].id!=edit->vertexids[c])
            { *reason="A UV vertex is no longer available."; goto done; }
            if (edit->s[c]<-32768 || edit->s[c]>32767 || edit->t[c]<-32768 || edit->t[c]>32767)
            { *reason="The mapped UVs exceed GoldenEye's texture coordinate range."; goto done; }
            corners[i*3+c]=(BgUVCorner){edit->face.room,(DWORD)(face-room->faces),c,v,edit->s[c],edit->t[c]};
            changedcorners+=room->vertices[v].s!=edit->s[c] || room->vertices[v].t!=edit->t[c];
        }
    }
    if (!changedcorners) { *reason=""; ok=TRUE; goto done; }
    if (!BgDocumentClone(document,&staged,reason)) { goto done; }
    qsort(corners,(size_t)count*3,sizeof(*corners),BgUVCompareCorners);
    *reason="Out of memory creating UV seams.";
    for (DWORD first=0; first<count*3; )
    {
        const BgUVCorner *key=&corners[first];
        BgDocumentRoom *room=&staged.rooms[key->room];
        const BgDocumentVertex source=document->rooms[key->room].vertices[key->vertex];
        DWORD end=first+1,reuse=(DWORD)-1;
        while (end<count*3 && corners[end].room==key->room && corners[end].vertex==key->vertex) { end++; }
        if (source.usecount<end-first) { *reason="A UV vertex has inconsistent face references."; goto done; }
        for (DWORD i=first; i<end; i++)
        { if (corners[i].s==source.s && corners[i].t==source.t) { reuse=i; break; } }
        if (reuse==(DWORD)-1 && source.usecount==end-first) { reuse=first; }
        room->vertices[key->vertex].usecount-=end-first;
        for (DWORD group=first; group<end; )
        {
            DWORD next=group+1,v=key->vertex;
            while (next<end && corners[next].s==corners[group].s && corners[next].t==corners[group].t) { next++; }
            if (!(reuse>=group && reuse<next))
            {
                if (!staged.nextvertexid || staged.nextvertexid==(DWORD)-1 || room->vertexcount>=0x100000u)
                { *reason="The UV seam would exceed the background vertex limit."; goto done; }
                BgDocumentVertex *grown=realloc(room->vertices,((size_t)room->vertexcount+1)*sizeof(*grown));
                if (!grown) { goto done; }
                room->vertices=grown; v=room->vertexcount++;
                room->vertices[v]=source; room->vertices[v].id=staged.nextvertexid++;
                room->vertices[v].usecount=0;
            }
            room->vertices[v].s=(short)corners[group].s; room->vertices[v].t=(short)corners[group].t;
            for (DWORD i=group; i<next; i++)
            {
                room->faces[corners[i].face].vertexindices[corners[i].corner]=v;
                room->vertices[v].usecount++;
            }
            group=next;
        }
        first=end;
    }
    BgDocumentFree(document); *document=staged; memset(&staged,0,sizeof(staged));
    document->dirty=TRUE; *changed=changedcorners; *reason=""; ok=TRUE;
done:
    free(sorted); free(corners); BgDocumentFree(&staged);
    return ok;
}
