#include "bgdocument.h"
#include <math.h>

static BOOL BgSeamPointEqual(const BgDocumentRoom *a,const BgDocumentVertex *av,
    const BgDocumentRoom *b,const BgDocumentVertex *bv)
{
    const short ap[3]={av->x,av->y,av->z},bp[3]={bv->x,bv->y,bv->z};
    for (int k=0;k<3;k++)
    { if (fabs((double)a->origin[k]+ap[k]-b->origin[k]-bp[k])>1e-5) { return FALSE; } }
    return TRUE;
}

BOOL BgDocumentSetEdgeSeam(BgDocument *document,const BgDocumentEdgeRef *edge,
    BOOL marked,DWORD *changed,const char **reason)
{
    const BgDocumentRoom *source;
    const BgDocumentFace *picked;
    *changed=0; *reason="The selected edge is no longer available.";
    if (!document || !edge || edge->corner>=3
        || !(picked=BgDocumentFindFace(document,&edge->face,&source))) { return FALSE; }
    DWORD ai=picked->vertexindices[edge->corner],bi=picked->vertexindices[(edge->corner+1)%3];
    if (ai>=source->vertexcount || bi>=source->vertexcount) { return FALSE; }
    const BgDocumentVertex *a=&source->vertices[ai],*b=&source->vertices[bi];
    /* Mark both sides, including native splits caused by UV/color/material
     * differences and edges that cross a room boundary. */
    for (DWORD r=1;r<=document->roomcount;r++)
    {
        BgDocumentRoom *room=&document->rooms[r];
        for (DWORD f=0;f<room->facecount;f++) for (int c=0;c<3;c++)
        {
            BgDocumentFace *face=&room->faces[f];
            DWORD xi=face->vertexindices[c],yi=face->vertexindices[(c+1)%3];
            if (xi>=room->vertexcount || yi>=room->vertexcount) { continue; }
            const BgDocumentVertex *x=&room->vertices[xi],*y=&room->vertices[yi];
            if ((BgSeamPointEqual(source,a,room,x) && BgSeamPointEqual(source,b,room,y))
                || (BgSeamPointEqual(source,a,room,y) && BgSeamPointEqual(source,b,room,x)))
            {
                unsigned char mask=(unsigned char)(1u<<c),old=face->uvseams;
                if (marked) { face->uvseams|=mask; } else { face->uvseams&=(unsigned char)~mask; }
                *changed+=old!=face->uvseams;
            }
        }
    }
    if (*changed) { document->dirty=TRUE; }
    *reason=""; return TRUE;
}

