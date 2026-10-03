#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "bglighting.h"

const BgLightingSettings g_BgLightingDefaults = {
    {255,255,255}, {255,255,255}, .25, .75, {-.4,.8,-.6}, 60
};
typedef struct BakeNode { DWORD parent, target; double normal[3]; } BakeNode;
typedef struct BakeEdge { DWORD lo, hi, a, b; unsigned char layer; } BakeEdge;
typedef struct BakeRoom {
    BgDocumentVertex *vertices;
    BgDocumentFace *faces;
    DWORD vertexcount;
    BOOL changed;
} BakeRoom;
static double Dot(const double a[3], const double b[3])
{ return a[0]*b[0]+a[1]*b[1]+a[2]*b[2]; }
static double Normalize(double v[3])
{
    double length=sqrt(Dot(v,v));
    if (length>0) { for (int i=0;i<3;i++) { v[i]/=length; } }
    return length;
}
BOOL BgLightingValidate(const BgLightingSettings *s, const char **why)
{
    *why="Enter light intensities between 0 and 4.";
    if (!s || !isfinite(s->ambientIntensity) || !isfinite(s->directionalIntensity)
        || s->ambientIntensity<0 || s->ambientIntensity>4
        || s->directionalIntensity<0 || s->directionalIntensity>4) { return FALSE; }
    *why="Enter a smoothing angle between 0 and 180 degrees.";
    if (!isfinite(s->smoothAngle) || s->smoothAngle<0 || s->smoothAngle>180) { return FALSE; }
    *why="Enter a finite light direction (each component between -1000000 and 1000000).";
    for (int i=0;i<3;i++)
        if (!isfinite(s->direction[i]) || fabs(s->direction[i])>1000000) { return FALSE; }
    *why="The directional light needs a nonzero direction.";
    if (s->directionalIntensity>0 && Dot(s->direction,s->direction)<1e-20) { return FALSE; }
    *why="";return TRUE;
}
static DWORD Root(BakeNode *nodes, DWORD i)
{
    while (nodes[i].parent!=i)
    { nodes[i].parent=nodes[nodes[i].parent].parent;i=nodes[i].parent; }
    return i;
}
static void Join(BakeNode *nodes, DWORD a, DWORD b)
{
    a=Root(nodes,a);b=Root(nodes,b);
    if (a<b) { nodes[b].parent=a; } else { nodes[a].parent=b; }
}
static int EdgeOrder(const void *a, const void *b)
{
    const BakeEdge *x=a,*y=b;
    if (x->layer!=y->layer) { return x->layer<y->layer ? -1 : 1; }
    if (x->lo!=y->lo) { return x->lo<y->lo ? -1 : 1; }
    return x->hi<y->hi ? -1 : x->hi>y->hi;
}
static BOOL ColorEquals(const BgDocumentVertex *v, const unsigned char rgb[3])
{ return v->r==rgb[0] && v->g==rgb[1] && v->b==rgb[2]; }
static void Shade(const BgLightingSettings *s, const double light[3], double normal[3], unsigned char rgb[3])
{
    Normalize(normal);
    double diffuse=fmax(0,Dot(normal,light))*s->directionalIntensity;
    for (int i=0;i<3;i++)
    {
        double value=s->ambient[i]*s->ambientIntensity+s->directional[i]*diffuse;
        rgb[i]=(unsigned char)floor(fmin(255,fmax(0,value))+.5);
    }
}
static BOOL Bake(const BgDocumentRoom *r, DWORD roomid, const BgLightingSettings *s,
    BakeRoom *out, DWORD *nextid, BgLightingResult *result, const char **why)
{
    BakeNode *nodes=NULL;
    BakeEdge *edges=NULL;
    double (*normals)[3]=NULL;
    unsigned char *used=NULL;
    DWORD edgecount=0,litfaces=0,changed=0,splits=0;
    BOOL ok=FALSE;
    *why="The background room has invalid geometry.";
    if (r->vertexcount>0x100000u || r->facecount>0x100000u
        || (r->vertexcount && !r->vertices) || (r->facecount && !r->faces)) { return FALSE; }
    if (!r->facecount) { return TRUE; }
    DWORD corners=r->facecount*3,capacity=r->vertexcount+corners;
    *why="Out of memory baking background lighting.";
    out->vertices=malloc((size_t)capacity*sizeof(*out->vertices));
    out->faces=malloc((size_t)r->facecount*sizeof(*out->faces));
    nodes=calloc(corners,sizeof(*nodes));edges=malloc((size_t)corners*sizeof(*edges));
    normals=calloc(r->facecount,sizeof(*normals));used=calloc(r->vertexcount,1);
    if (!out->vertices || !out->faces || !nodes || !edges || !normals || !used) { goto done; }
    memcpy(out->vertices,r->vertices,(size_t)r->vertexcount*sizeof(*out->vertices));
    memcpy(out->faces,r->faces,(size_t)r->facecount*sizeof(*out->faces));
    out->vertexcount=r->vertexcount;
    for (DWORD i=0;i<corners;i++) { nodes[i].parent=i;nodes[i].target=UINT32_MAX; }
    for (DWORD f=0;f<r->facecount;f++)
    {
        const BgDocumentFace *face=r->faces+f;
        *why="A background face has invalid vertex references.";
        if (face->room!=roomid || face->layer>1) { goto done; }
        double p[3][3],ab[3],ac[3];
        for (int c=0;c<3;c++)
        {
            DWORD index=face->vertexindices[c];
            if (index>=r->vertexcount || r->vertices[index].room!=roomid) { goto done; }
            const BgDocumentVertex *v=r->vertices+index;
            p[c][0]=v->x;p[c][1]=v->y;p[c][2]=v->z;
        }
        for (int i=0;i<3;i++) { ab[i]=p[1][i]-p[0][i];ac[i]=p[2][i]-p[0][i]; }
        double *n=normals[f];
        n[0]=ab[1]*ac[2]-ab[2]*ac[1];n[1]=ab[2]*ac[0]-ab[0]*ac[2];n[2]=ab[0]*ac[1]-ab[1]*ac[0];
        if (!Normalize(n))
        {
            /* Degenerates contribute no normal and keep their original RGB. */
            for (int c=0;c<3;c++) { used[face->vertexindices[c]]=1; }
            continue;
        }
        litfaces++;
        for (int c=0;c<3;c++)
        {
            DWORD a=face->vertexindices[c],b=face->vertexindices[(c+1)%3];
            edges[edgecount++]=(BakeEdge){a<b?a:b,a<b?b:a,f*3+c,f*3+(c+1)%3,face->layer};
        }
    }
    qsort(edges,edgecount,sizeof(*edges),EdgeOrder);
    double threshold=cos(s->smoothAngle*(3.14159265358979323846/180));
    for (DWORD start=0,end;start<edgecount;start=end)
    {
        for (end=start+1;end<edgecount && !EdgeOrder(edges+start,edges+end);end++) {}
        /* Non-manifold edges and inconsistent winding remain hard. */
        if (end-start!=2) { continue; }
        BakeEdge *a=edges+start,*b=a+1;
        if (r->faces[a->a/3].vertexindices[a->a%3]!=r->faces[b->b/3].vertexindices[b->b%3]
            || Dot(normals[a->a/3],normals[b->a/3])<threshold-1e-10) { continue; }
        Join(nodes,a->a,b->b);Join(nodes,a->b,b->a);
    }
    /* Angle weighting avoids a diagonal or a skinny triangle biasing the
     * normal. Separate source identities and disconnected fans stay separate. */
    for (DWORD f=0;f<r->facecount;f++)
    {
        if (!Dot(normals[f],normals[f])) { continue; }
        for (int c=0;c<3;c++)
        {
            const BgDocumentFace *face=r->faces+f;
            const BgDocumentVertex *v=r->vertices+face->vertexindices[c];
            const BgDocumentVertex *a=r->vertices+face->vertexindices[(c+1)%3];
            const BgDocumentVertex *b=r->vertices+face->vertexindices[(c+2)%3];
            double u[3]={a->x-v->x,a->y-v->y,a->z-v->z},w[3]={b->x-v->x,b->y-v->y,b->z-v->z};
            Normalize(u);Normalize(w);
            double angle=acos(fmax(-1,fmin(1,Dot(u,w))));
            BakeNode *node=nodes+Root(nodes,f*3+c);
            for (int i=0;i<3;i++) { node->normal[i]+=normals[f][i]*angle; }
        }
    }
    double light[3];memcpy(light,s->direction,sizeof(light));Normalize(light);
    for (DWORD f=0;f<r->facecount;f++)
    {
        if (!Dot(normals[f],normals[f])) { continue; }
        for (int c=0;c<3;c++)
        {
            DWORD source=r->faces[f].vertexindices[c];
            BakeNode *node=nodes+Root(nodes,f*3+c);
            if (node->target==UINT32_MAX)
            {
                unsigned char rgb[3];Shade(s,light,node->normal,rgb);
                node->target=source;
                if (used[source] && !ColorEquals(out->vertices+source,rgb))
                {
                    *why="The bake would exceed the background vertex limit.";
                    if (!*nextid || *nextid==UINT32_MAX || out->vertexcount>=0x100000u) { goto done; }
                    node->target=out->vertexcount++;
                    out->vertices[node->target]=r->vertices[source];
                    out->vertices[node->target].id=(*nextid)++;
                    splits++;
                }
                used[source]=1;
                BgDocumentVertex *v=out->vertices+node->target;
                if (!ColorEquals(v,rgb)) { changed++; }
                v->r=rgb[0];v->g=rgb[1];v->b=rgb[2];
            }
            out->faces[f].vertexindices[c]=node->target;
        }
    }
    for (DWORD v=0;v<out->vertexcount;v++) { out->vertices[v].usecount=0; }
    for (DWORD f=0;f<r->facecount;f++) for (int c=0;c<3;c++)
    { out->vertices[out->faces[f].vertexindices[c]].usecount++; }
    out->changed=changed!=0 || splits!=0;
    result->rooms++;result->faces+=litfaces;result->vertices+=changed;result->splits+=splits;
    *why="";ok=TRUE;
done:
    free(nodes);free(edges);free(normals);free(used);return ok;
}
BOOL BgDocumentBakeLighting(BgDocument *doc, const DWORD *rooms, DWORD count,
    const BgLightingSettings *s, BgLightingResult *result, const char **why)
{
    BakeRoom *pending=NULL;BOOL ok=FALSE,changed=FALSE;
    BgLightingResult total={0};
    if (result) { memset(result,0,sizeof(*result)); }
    if (!BgLightingValidate(s,why)) { return FALSE; }
    *why="Add one or more background rooms to bake.";
    if (!doc || !doc->rooms || !doc->roomcount || doc->roomcount>65535
        || !rooms || !count || count>65535) { return FALSE; }
    *why="Out of memory preparing the lighting bake.";
    pending=calloc(doc->roomcount+1,sizeof(*pending));
    if (!pending) { return FALSE; }
    DWORD nextid=doc->nextvertexid;
    for (DWORD i=0;i<count;i++)
    {
        DWORD id=rooms[i];*why="A selected background room no longer exists.";
        if (!id || id>doc->roomcount) { goto done; }
        if (pending[id].vertices) { continue; }
        if (!Bake(doc->rooms+id,id,s,pending+id,&nextid,&total,why)) { goto done; }
    }
    for (DWORD id=1;id<=doc->roomcount;id++) if (pending[id].changed)
    {
        BgDocumentRoom *r=doc->rooms+id;BakeRoom *p=pending+id;
        free(r->vertices);free(r->faces);r->vertices=p->vertices;r->faces=p->faces;
        r->vertexcount=p->vertexcount;r->facecapacity=r->facecount;
        p->vertices=NULL;p->faces=NULL;changed=TRUE;
    }
    if (changed) { doc->nextvertexid=nextid;doc->dirty=TRUE; }
    if (result) { *result=total; }
    *why="";ok=TRUE;
done:
    for (DWORD id=1;id<=doc->roomcount;id++) { free(pending[id].vertices);free(pending[id].faces); }
    free(pending);return ok;
}
