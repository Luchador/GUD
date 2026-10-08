#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "bgao.h"

#define AO_SAMPLES 64
typedef struct AoTriangle { double p[3], a[3], b[3], min[3], max[3]; } AoTriangle;
typedef struct AoNode { double min[3], max[3]; DWORD first, count, left, right; } AoNode;
struct BgAoScene {
    AoTriangle *triangles;
    AoNode *nodes;
    DWORD count, nodecount;
    double samples[AO_SAMPLES][3];
};
static double AoDot(const double a[3], const double b[3])
{ return a[0]*b[0]+a[1]*b[1]+a[2]*b[2]; }
static void AoCross(const double a[3], const double b[3], double out[3])
{ for (int k=0;k<3;k++) { int j=(k+1)%3,l=(k+2)%3;out[k]=a[j]*b[l]-a[l]*b[j]; } }
static DWORD AoBuildNode(BgAoScene *s, DWORD first, DWORD count, unsigned depth)
{
    DWORD index=s->nodecount++;
    AoNode *node=s->nodes+index;
    double lo[3]={DBL_MAX,DBL_MAX,DBL_MAX},hi[3]={-DBL_MAX,-DBL_MAX,-DBL_MAX};
    node->first=first;node->count=count;
    for (int k=0;k<3;k++) { node->min[k]=DBL_MAX;node->max[k]=-DBL_MAX; }
    for (DWORD i=first;i<first+count;i++) for (int k=0;k<3;k++)
    {
        const AoTriangle *t=s->triangles+i;double center=(t->min[k]+t->max[k])*.5;
        node->min[k]=fmin(node->min[k],t->min[k]);node->max[k]=fmax(node->max[k],t->max[k]);
        lo[k]=fmin(lo[k],center);hi[k]=fmax(hi[k],center);
    }
    if (count<=8 || depth>=48) { return index; }
    int axis=0;for (int k=1;k<3;k++) if (hi[k]-lo[k]>hi[axis]-lo[axis]) { axis=k; }
    double middle=(lo[axis]+hi[axis])*.5;
    DWORD a=first,b=first+count;
    while (a<b)
    {
        AoTriangle *t=s->triangles+a;
        if ((t->min[axis]+t->max[axis])*.5<middle) { a++; }
        else { AoTriangle swap=*t;*t=s->triangles[--b];s->triangles[b]=swap; }
    }
    if (a==first || a==first+count) { a=first+count/2; }
    node->left=AoBuildNode(s,first,a-first,depth+1);
    node->right=AoBuildNode(s,a,first+count-a,depth+1);node->count=0;
    return index;
}
void BgAoFree(BgAoScene *scene)
{ if (scene) { free(scene->triangles);free(scene->nodes);free(scene); } }
BOOL BgAoBuild(const BgDocument *doc, BgAoScene **out, const char **why)
{
    uint64_t capacity=0;BgAoScene *s=NULL;
    *out=NULL;*why="Ambient occlusion needs valid primary background geometry and a positive level scale.";
    if (!doc || !doc->rooms || !isfinite(doc->levelscale) || doc->levelscale<=0) { return FALSE; }
    for (DWORD r=1;r<=doc->roomcount;r++)
    {
        const BgDocumentRoom *room=doc->rooms+r;
        if ((room->facecount && !room->faces) || (room->vertexcount && !room->vertices)) { return FALSE; }
        for (int k=0;k<3;k++) if (!isfinite(room->origin[k])) { return FALSE; }
        for (DWORD f=0;f<room->facecount;f++) if (!room->faces[f].layer) { capacity++; }
    }
    if (!capacity) { *why="";return TRUE; }
    *why="Out of memory building the ambient occlusion acceleration tree.";
    if (capacity>UINT32_MAX/2 || capacity>SIZE_MAX/sizeof(AoTriangle) || capacity>SIZE_MAX/(2*sizeof(AoNode))) { return FALSE; }
    s=calloc(1,sizeof(*s));if (!s) { return FALSE; }
    s->triangles=malloc((size_t)capacity*sizeof(*s->triangles));
    s->nodes=calloc((size_t)capacity*2,sizeof(*s->nodes));
    if (!s->triangles || !s->nodes) { goto fail; }
    for (DWORD r=1;r<=doc->roomcount;r++)
    {
        const BgDocumentRoom *room=doc->rooms+r;
        for (DWORD f=0;f<room->facecount;f++)
        {
            const BgDocumentFace *face=room->faces+f;
            if (face->layer) { continue; }
            AoTriangle *t=s->triangles+s->count;double p[3][3],normal[3];
            for (int c=0;c<3;c++)
            {
                *why="A primary background face has invalid vertex references.";
                if (face->vertexindices[c]>=room->vertexcount) { goto fail; }
                const BgDocumentVertex *v=room->vertices+face->vertexindices[c];
                p[c][0]=(v->x+(double)room->origin[0])/doc->levelscale;
                p[c][1]=(v->y+(double)room->origin[1])/doc->levelscale;
                p[c][2]=(v->z+(double)room->origin[2])/doc->levelscale;
            }
            for (int k=0;k<3;k++)
            {
                t->p[k]=p[0][k];t->a[k]=p[1][k]-p[0][k];t->b[k]=p[2][k]-p[0][k];
                t->min[k]=fmin(p[0][k],fmin(p[1][k],p[2][k]));
                t->max[k]=fmax(p[0][k],fmax(p[1][k],p[2][k]));
            }
            AoCross(t->a,t->b,normal);
            if (AoDot(normal,normal)>0) { s->count++; }
        }
    }
    if (s->count) { AoBuildNode(s,0,s->count,0); }
    /* Deterministic cosine-weighted hemisphere: no bake-to-bake noise. */
    for (int i=0;i<AO_SAMPLES;i++)
    {
        double r=sqrt((i+.5)/AO_SAMPLES),angle=i*2.39996322972865332;
        s->samples[i][0]=r*cos(angle);s->samples[i][1]=r*sin(angle);s->samples[i][2]=sqrt(1-r*r);
    }
    *out=s;*why="";return TRUE;
fail:
    BgAoFree(s);return FALSE;
}
static BOOL AoBox(const AoNode *n, const double p[3], const double d[3], double distance)
{
    double nearhit=0,farhit=distance;
    for (int k=0;k<3;k++)
    {
        if (fabs(d[k])<1e-15) { if (p[k]<n->min[k] || p[k]>n->max[k]) { return FALSE; } }
        else
        {
            double a=(n->min[k]-p[k])/d[k],b=(n->max[k]-p[k])/d[k];
            nearhit=fmax(nearhit,fmin(a,b));farhit=fmin(farhit,fmax(a,b));if (nearhit>farhit) { return FALSE; }
        }
    }
    return TRUE;
}
static void AoTrace(const BgAoScene *s, DWORD index, const double p[3], const double d[3], double bias, double *nearest)
{
    const AoNode *node=s->nodes+index;
    if (!AoBox(node,p,d,*nearest)) { return; }
    if (!node->count)
    { AoTrace(s,node->left,p,d,bias,nearest);AoTrace(s,node->right,p,d,bias,nearest);return; }
    for (DWORD i=node->first;i<node->first+node->count;i++)
    {
        const AoTriangle *t=s->triangles+i;double h[3],q[3],delta[3];
        AoCross(d,t->b,h);double det=AoDot(t->a,h);
        if (fabs(det)<1e-14) { continue; }
        for (int k=0;k<3;k++) { delta[k]=p[k]-t->p[k]; }
        double u=AoDot(delta,h)/det;if (u<0 || u>1) { continue; }
        AoCross(delta,t->a,q);double v=AoDot(d,q)/det;if (v<0 || u+v>1) { continue; }
        double distance=AoDot(t->b,q)/det;
        /* Two-sided blockers, regardless of their display culling/material. */
        if (distance>bias && distance<*nearest) { *nearest=distance; }
    }
}
double BgAoOcclusion(const BgAoScene *s, const double position[3], const double normal[3], double radius)
{
    if (!s || !s->count || !(radius>0) || !isfinite(radius) || AoDot(normal,normal)<.5) { return 0; }
    double tangent[3],bitangent[3],axis[3]={0,0,0},origin[3];
    axis[fabs(normal[1])<.9 ? 1 : 0]=1;AoCross(axis,normal,tangent);
    double length=sqrt(AoDot(tangent,tangent));for (int k=0;k<3;k++) { tangent[k]/=length; }
    AoCross(normal,tangent,bitangent);
    double bias=fmin(.01,radius*.0001),occlusion=0;
    for (int k=0;k<3;k++) { origin[k]=position[k]+normal[k]*bias; }
    for (int i=0;i<AO_SAMPLES;i++)
    {
        double direction[3],nearest=radius;
        for (int k=0;k<3;k++)
        { direction[k]=tangent[k]*s->samples[i][0]+bitangent[k]*s->samples[i][1]+normal[k]*s->samples[i][2]; }
        AoTrace(s,0,origin,direction,bias,&nearest);
        occlusion+=1-nearest/radius;
    }
    return occlusion/AO_SAMPLES;
}
