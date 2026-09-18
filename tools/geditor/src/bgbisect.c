/* Subdivide one source edge across all incident triangles. Native identities,
 * rather than coincident coordinates, determine which faces share the edge. */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "bgdocument.h"

static short BgBisectAverage(int a, int b)
{
    int sum = a + b;
    return (short)(sum < 0 ? -((-sum + 1)/2) : (sum + 1)/2);
}

static void BgBisectNormal(const BgDocumentVertex *a, const BgDocumentVertex *b,
    const BgDocumentVertex *c, double n[3])
{
    double u[3] = {b->x-a->x,b->y-a->y,b->z-a->z};
    double v[3] = {c->x-a->x,c->y-a->y,c->z-a->z};
    for (int k=0;k<3;k++) { n[k]=u[(k+1)%3]*v[(k+2)%3]-u[(k+2)%3]*v[(k+1)%3]; }
}

BOOL BgDocumentBisectEdge(BgDocument *doc, const BgDocumentEdgeRef *edge,
    BgDocumentEdgeRef out[2], const char **why)
{
    const BgDocumentRoom *source;
    const BgDocumentFace *picked;
    BgDocumentVertex *vertices = NULL, midpoint;
    BgDocumentFace *faces = NULL;
    DWORD a, b, count = 0, at, nextface;
    BOOL ok = FALSE;
    *why = "Select one background edge to bisect.";
    if (!doc || !doc->rooms || !edge || edge->corner >= 3 || !out) { return FALSE; }
    picked = BgDocumentFindFace(doc,&edge->face,&source);
    if (!picked || !source->vertices || !source->faces) { return FALSE; }
    a=picked->vertexindices[edge->corner]; b=picked->vertexindices[(edge->corner+1)%3];
    if (a==b || a>=source->vertexcount || b>=source->vertexcount) { return FALSE; }
    midpoint=source->vertices[a];
#define AVERAGE(field) midpoint.field=BgBisectAverage(source->vertices[a].field,source->vertices[b].field)
    AVERAGE(x); AVERAGE(y); AVERAGE(z); AVERAGE(s); AVERAGE(t);
    AVERAGE(r); AVERAGE(g); AVERAGE(b); AVERAGE(a);
#undef AVERAGE
    midpoint.usecount=0;
    for (DWORD f=0;f<source->facecount;f++)
    {
        const BgDocumentFace *face=&source->faces[f];
        for (int c=0;c<3;c++)
        {
            if (face->vertexindices[c]>=source->vertexcount)
            { *why="A background face has an invalid vertex reference."; return FALSE; }
        }
        for (int c=0;c<3;c++)
        {
            DWORD x=face->vertexindices[c],y=face->vertexindices[(c+1)%3];
            if ((x==a && y==b) || (x==b && y==a))
            {
                const BgDocumentVertex *u=&source->vertices[x], *v=&source->vertices[y];
                const BgDocumentVertex *w=&source->vertices[face->vertexindices[(c+2)%3]];
                double before[3],first[3],second[3];
                BgBisectNormal(u,v,w,before); BgBisectNormal(u,&midpoint,w,first); BgBisectNormal(&midpoint,v,w,second);
                *why="The edge is too small or degenerate to bisect at native coordinate precision.";
                if (before[0]*first[0]+before[1]*first[1]+before[2]*first[2]<=0
                    || before[0]*second[0]+before[1]*second[1]+before[2]*second[2]<=0) { return FALSE; }
                count++; break;
            }
        }
    }
    *why="Bisecting this edge exceeds the native geometry or identity limits.";
    if (!count || source->vertexcount>=0x100000u || !doc->nextvertexid || doc->nextvertexid==UINT32_MAX
        || !doc->nextfaceid || doc->nextfaceid>UINT32_MAX-count || doc->facecount>UINT32_MAX-count
        || source->facecount>UINT32_MAX-count
        || (size_t)source->facecount+count>SIZE_MAX/sizeof(*faces)) { return FALSE; }
    *why="Out of memory bisecting the background edge.";
    vertices=malloc(((size_t)source->vertexcount+1)*sizeof(*vertices));
    faces=malloc(((size_t)source->facecount+count)*sizeof(*faces));
    if (!vertices || !faces) { goto done; }
    memcpy(vertices,source->vertices,source->vertexcount*sizeof(*vertices));
    midpoint.id=doc->nextvertexid; vertices[source->vertexcount]=midpoint;
    /* Insert each child beside its parent so translucent draw ordering stays
     * local, and keep the parent's identity on its first child. */
    at=0; nextface=doc->nextfaceid;
    for (DWORD f=0;f<source->facecount;f++)
    {
        const BgDocumentFace *face=&source->faces[f];
        faces[at]=*face;
        for (int c=0;c<3;c++)
        {
            DWORD x=face->vertexindices[c],y=face->vertexindices[(c+1)%3];
            if ((x!=a || y!=b) && (x!=b || y!=a)) { continue; }
            faces[at+1]=*face; faces[at+1].id=nextface++;
            faces[at].vertexindices[(c+1)%3]=source->vertexcount;
            faces[at+1].vertexindices[c]=source->vertexcount;
            if (face==picked)
            {
                out[0]=(BgDocumentEdgeRef){{face->id,face->room,face->layer,0},(unsigned int)c};
                out[1]=(BgDocumentEdgeRef){{faces[at+1].id,face->room,face->layer,0},(unsigned int)c};
            }
            at++; break;
        }
        at++;
    }
    for (DWORD v=0;v<=source->vertexcount;v++) { vertices[v].usecount=0; }
    for (DWORD f=0;f<at;f++) for (int c=0;c<3;c++) { vertices[faces[f].vertexindices[c]].usecount++; }
    BgDocumentRoom *room=&doc->rooms[edge->face.room];
    free(room->vertices); free(room->faces);
    room->vertices=vertices; vertices=NULL; room->vertexcount++;
    room->faces=faces; faces=NULL; room->facecount=room->facecapacity=at;
    doc->facecount+=count; doc->nextfaceid=nextface; doc->nextvertexid++; doc->dirty=TRUE;
    *why=""; ok=TRUE;
done:
    free(vertices); free(faces); return ok;
}
