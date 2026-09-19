#include "uvprojection.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define CYL_PI 3.14159265358979323846
/* Work on geometric adjacency: native vertices may already be split by UV,
 * material or color. None of these scratch welds change authored geometry. */
typedef struct CylPoint { double p[3]; int source; } CylPoint;
typedef struct CylEdge { int a,b,face,corner,other,seam; } CylEdge;
typedef struct CylRim { double angle,x,y,length; int vertex; } CylRim;
typedef struct CylHeap { double distance; int vertex; } CylHeap;
static double CylDot(const double *a,const double *b)
{ return a[0]*b[0]+a[1]*b[1]+a[2]*b[2]; }
static void CylCross(const double *a,const double *b,double *c)
{ c[0]=a[1]*b[2]-a[2]*b[1]; c[1]=a[2]*b[0]-a[0]*b[2]; c[2]=a[0]*b[1]-a[1]*b[0]; }
static int CylPointCompare(const void *aa,const void *bb)
{
    const CylPoint *a=aa,*b=bb;
    for (int k=0;k<3;k++) if (a->p[k]!=b->p[k]) { return a->p[k]<b->p[k] ? -1 : 1; }
    return 0;
}
static int CylEdgeCompare(const void *aa,const void *bb)
{
    const CylEdge *a=aa,*b=bb;
    if (a->a!=b->a) { return a->a<b->a ? -1 : 1; }
    return (a->b>b->b)-(a->b<b->b);
}
static int CylRimCompare(const void *aa,const void *bb)
{ const CylRim *a=aa,*b=bb; return (a->angle>b->angle)-(a->angle<b->angle); }
static void CylPush(CylHeap *heap,int *count,CylHeap item)
{
    int i=(*count)++;
    while (i && heap[(i-1)/2].distance>item.distance) { heap[i]=heap[(i-1)/2]; i=(i-1)/2; }
    heap[i]=item;
}
static CylHeap CylPop(CylHeap *heap,int *count)
{
    CylHeap out=heap[0],last=heap[--*count]; int i=0;
    while (i*2+1<*count)
    {
        int c=i*2+1;
        if (c+1<*count && heap[c+1].distance<heap[c].distance) { c++; }
        if (last.distance<=heap[c].distance) { break; }
        heap[i]=heap[c]; i=c;
    }
    heap[i]=last; return out;
}

/* Output is surface distance in world units, not normalized UVs. The caller
 * applies one texel density and divides by each image's dimensions. */
int UVProjectionCylinder(const UVProjectionVertex *vertices,int vertexcount,
    const UVProjectionFace *faces,int facecount,const unsigned char *seams,
    int axischoice,double (*uv)[3][2],const char **reason)
{
    CylPoint *points=NULL; CylEdge *edges=NULL; CylRim *rim=NULL; CylHeap *heap=NULL;
    int *map=NULL,*loop=NULL,(*boundary)[2]=NULL,*degree=NULL,*head=NULL,*next=NULL;
    int *queue=NULL,*faceedges=NULL,*parent=NULL,*seen=NULL;
    double *raw=NULL,*height=NULL,*distance=NULL,*offset=NULL;
    int n=0,ne=0,nrim=0,ok=0,marked=0,loopcount=0,counts[2]={0};
    double centers[2][3]={{0}},rimnormals[2][3]={{0}},axis[3],u[3],v[3],center[3],extent=0,low[3],high[3];
    double perimeter=0,hmin=0,hmax=0;
    *reason="Select one complete, uncapped cylinder side wall.";
    if (!vertices || !faces || !uv || vertexcount<6 || facecount<6
        || vertexcount>1000000 || facecount>1000000 || axischoice<0 || axischoice>3) { return 0; }
    for (int i=0;i<vertexcount;i++) for (int k=0;k<3;k++)
    {
        double p=vertices[i].position[k];
        if (!isfinite(p)) { *reason="The selected faces contain invalid coordinates."; return 0; }
        if (!i || p<low[k]) { low[k]=p; } if (!i || p>high[k]) { high[k]=p; }
    }
    for (int k=0;k<3;k++) { extent=fmax(extent,high[k]-low[k]); }
    if (!isfinite(extent) || extent<1e-8) { return 0; }
#define ALLOC(name,count) do { name=calloc((size_t)(count),sizeof(*name)); if (!name) { *reason="Out of memory unwrapping the cylinder."; goto done; } } while (0)
    ALLOC(points,vertexcount); ALLOC(map,vertexcount); ALLOC(edges,facecount*3);
    ALLOC(faceedges,facecount*3); ALLOC(loop,vertexcount); ALLOC(boundary,vertexcount);
    ALLOC(degree,vertexcount); ALLOC(head,vertexcount); ALLOC(next,facecount*6);
    ALLOC(queue,vertexcount+facecount); ALLOC(parent,vertexcount); ALLOC(seen,vertexcount+facecount);
    ALLOC(raw,vertexcount); ALLOC(height,vertexcount); ALLOC(distance,vertexcount);
    ALLOC(offset,facecount); ALLOC(rim,vertexcount); ALLOC(heap,facecount*6+1);
#undef ALLOC
    for (int i=0;i<vertexcount;i++)
    {
        points[i].source=i;
        for (int k=0;k<3;k++) { points[i].p[k]=(vertices[i].position[k]-low[k])/extent; }
    }
    qsort(points,vertexcount,sizeof(*points),CylPointCompare);
    for (int i=0;i<vertexcount;i++)
    {
        if (!n || fabs(points[i].p[0]-points[n-1].p[0])>1e-8
            || fabs(points[i].p[1]-points[n-1].p[1])>1e-8
            || fabs(points[i].p[2]-points[n-1].p[2])>1e-8) { points[n++]=points[i]; }
        map[points[i].source]=n-1;
    }
    for (int f=0;f<facecount;f++)
    {
        for (int c=0;c<3;c++) if (faces[f].vertices[c]<0 || faces[f].vertices[c]>=vertexcount) { goto done; }
        const double *a=points[map[faces[f].vertices[0]]].p;
        double ab[3],ac[3],normal[3];
        for (int k=0;k<3;k++)
        { ab[k]=points[map[faces[f].vertices[1]]].p[k]-a[k]; ac[k]=points[map[faces[f].vertices[2]]].p[k]-a[k]; }
        CylCross(ab,ac,normal);
        if (CylDot(normal,normal)<1e-24) { *reason="Remove zero-area triangles before unwrapping."; goto done; }
    }
    for (int f=0;f<facecount;f++) for (int c=0;c<3;c++)
    {
        int a=faces[f].vertices[c],b=faces[f].vertices[(c+1)%3];
        if (a<0 || b<0 || a>=vertexcount || b>=vertexcount) { goto done; }
        a=map[a]; b=map[b]; if (a==b) { *reason="The selection contains a collapsed triangle."; goto done; }
        edges[f*3+c]=(CylEdge){a<b?a:b,a<b?b:a,f,c,-1,seams && (seams[f]&(1<<c))};
    }
    qsort(edges,facecount*3,sizeof(*edges),CylEdgeCompare);
    for (int i=0;i<n;i++) { loop[i]=-1; head[i]=-1; boundary[i][0]=boundary[i][1]=-1; }
    for (int i=0;i<facecount*3;)
    {
        int end=i+1;
        while (end<facecount*3 && edges[end].a==edges[i].a && edges[end].b==edges[i].b) { end++; }
        if (end-i>2) { *reason="More than two faces share an edge. Select one cylinder wall."; goto done; }
        CylEdge e=edges[i];
        if (end-i==2) { e.other=edges[i+1].face*3+edges[i+1].corner; e.seam|=edges[i+1].seam; }
        else
        {
            for (int k=0;k<2;k++)
            {
                int a=k?e.b:e.a,b=k?e.a:e.b;
                if (degree[a]>=2) { *reason="The cylinder has a branched boundary."; goto done; }
                boundary[a][degree[a]++]=b;
            }
            if (e.seam) { *reason="Mark the seam along the side wall, not along an open rim."; goto done; }
        }
        faceedges[e.face*3+e.corner]=ne;
        if (e.other>=0) { faceedges[e.other]=ne; }
        edges[ne]=e;
        next[ne*2]=head[e.a]; head[e.a]=ne*2;
        next[ne*2+1]=head[e.b]; head[e.b]=ne*2+1;
        marked+=e.seam!=0; ne++; i=end;
    }
    for (int i=0;i<n;i++) if (degree[i] && loop[i]<0)
    {
        if (loopcount==2) { *reason="The selection has extra holes or disconnected walls."; goto done; }
        int p=i,previous=-1;
        do {
            if (degree[p]!=2 || loop[p]>=0) { goto done; }
            loop[p]=loopcount; counts[loopcount]++;
            for (int k=0;k<3;k++) { centers[loopcount][k]+=points[p].p[k]; }
            int q=boundary[p][0]==previous ? boundary[p][1] : boundary[p][0]; previous=p; p=q;
        } while (p!=i);
        loopcount++;
    }
    if (loopcount!=2 || counts[0]<3 || counts[1]<3) { goto done; }
    /* Area centroids are insensitive to uneven rim tessellation. A vertex
     * average would tilt the axis when only one rim has extra subdivisions. */
    for (int l=0;l<2;l++)
    {
        int start=0; while (loop[start]!=l) { start++; }
        double normal[3]={0},area=0;
        memset(centers[l],0,sizeof(centers[l]));
        for (int pass=0;pass<2;pass++)
        {
            int previous=start,p=boundary[start][0];
            while (p!=start)
            {
                int q=boundary[p][0]==previous ? boundary[p][1] : boundary[p][0];
                double a[3],b[3],cross[3];
                for (int k=0;k<3;k++) { a[k]=points[p].p[k]-points[start].p[k]; b[k]=points[q].p[k]-points[start].p[k]; }
                CylCross(a,b,cross);
                if (!pass) { for (int k=0;k<3;k++) { normal[k]+=cross[k]; } }
                else
                {
                    double weight=CylDot(cross,normal); area+=weight;
                    for (int k=0;k<3;k++) { centers[l][k]+=weight*(points[start].p[k]+points[p].p[k]+points[q].p[k])/3; }
                }
                previous=p; p=q;
            }
        }
        if (area<1e-20) { *reason="The open rim has no usable area. Choose a complete cylinder wall."; goto done; }
        for (int k=0;k<3;k++) { centers[l][k]/=area; rimnormals[l][k]=normal[k]/sqrt(CylDot(normal,normal)); }
    }
    for (int k=0;k<3;k++)
    {
        center[k]=(centers[0][k]+centers[1][k])*0.5;
        axis[k]=axischoice ? (k==axischoice-1) : centers[1][k]-centers[0][k];
    }
    double length=sqrt(CylDot(axis,axis));
    if (!axischoice && length>1e-8)
    {
        /* On short/wide tubes, sub-unit centroid noise is magnified by the
         * small rim separation. Their broad rim planes give a steadier axis. */
        double radius2=0,normal[3]={0};
        for (int i=0;i<n;i++) if (loop[i]>=0)
        {
            double p[3]; for (int k=0;k<3;k++) { p[k]=points[i].p[k]-centers[loop[i]][k]; }
            radius2=fmax(radius2,CylDot(p,p));
        }
        for (int l=0;l<2;l++)
        {
            double sign=CylDot(rimnormals[l],axis)<0 ? -1 : 1;
            for (int k=0;k<3;k++) { normal[k]+=sign*rimnormals[l][k]; }
        }
        double norm=sqrt(CylDot(normal,normal));
        if (length*length<radius2*4 && norm>1.9 && CylDot(normal,axis)>norm*length*0.95)
        { for (int k=0;k<3;k++) { axis[k]=normal[k]/norm; } length=1; }
    }
    if (length<1e-8) { *reason="The two open rims do not define a cylinder axis."; goto done; }
    int dominant=0,smallest=0;
    for (int k=1;k<3;k++) { if (fabs(axis[k])>fabs(axis[dominant])) { dominant=k; } }
    double sign=axis[dominant]<0 ? -1 : 1;
    for (int k=0;k<3;k++) { axis[k]*=sign/length; }
    for (int k=1;k<3;k++) { if (fabs(axis[k])<fabs(axis[smallest])) { smallest=k; } }
    double helper[3]={0}; helper[smallest]=1; CylCross(helper,axis,u);
    length=sqrt(CylDot(u,u)); for (int k=0;k<3;k++) { u[k]/=length; } CylCross(axis,u,v);
    int bottom=CylDot(centers[0],axis)<CylDot(centers[1],axis) ? 0 : 1;
    for (int i=0;i<n;i++)
    {
        double p[3]; for (int k=0;k<3;k++) { p[k]=points[i].p[k]-center[k]; }
        height[i]=CylDot(p,axis);
        if (!i || height[i]<hmin) { hmin=height[i]; } if (!i || height[i]>hmax) { hmax=height[i]; }
        raw[i]=atan2(CylDot(p,v),CylDot(p,u));
        if (loop[i]==bottom)
        { rim[nrim++]=(CylRim){raw[i],CylDot(p,u),CylDot(p,v),0,i}; }
    }
    if (hmax-hmin<1e-8) { *reason="Choose an axis along the cylinder's height."; goto done; }
    qsort(rim,nrim,sizeof(*rim),CylRimCompare);
    for (int i=0;i<nrim;i++)
    {
        int j=(i+1)%nrim,a=rim[i].vertex,b=rim[j].vertex;
        double angle=(j?rim[j].angle:rim[0].angle+2*CYL_PI)-rim[i].angle;
        if ((boundary[a][0]!=b && boundary[a][1]!=b) || angle<1e-8 || angle>=CYL_PI)
        { *reason="The rim folds across the axis. Choose another axis or select a straight cylinder wall."; goto done; }
        rim[i].length=perimeter;
        perimeter+=hypot(rim[j].x-rim[i].x,rim[j].y-rim[i].y);
    }
    if (perimeter<1e-8) { goto done; }
    /* Intersect each radial direction with the polygonal rim. Chord distance,
     * rather than angle, preserves the widths of unequal polygon sides. */
    for (int i=0;i<n;i++)
    {
        double angle=raw[i]; if (angle<rim[0].angle) { angle+=2*CYL_PI; }
        int begin=0,end=nrim;
        while (begin<end) { int middle=(begin+end)/2; if (rim[middle].angle<=angle) { begin=middle+1; } else { end=middle; } }
        int j=begin-1;
        int k=(j+1)%nrim; double dx=cos(angle),dy=sin(angle);
        double ex=rim[k].x-rim[j].x,ey=rim[k].y-rim[j].y;
        double denominator=dx*ey-dy*ex;
        if (fabs(denominator)<1e-12) { goto done; }
        double t=(rim[j].x*dy-rim[j].y*dx)/denominator;
        raw[i]=(rim[j].length+fmax(0,fmin(1,t))*hypot(ex,ey))/perimeter;
    }
    if (!marked)
    {
        /* Shortest edge path between rims; discourage travel around the tube.
         * A heap keeps large triangulated selections responsive. */
        int start=rim[0].vertex,heapcount=0,target=-1;
        for (int i=0;i<n;i++) { distance[i]=HUGE_VAL; parent[i]=-1; }
        distance[start]=0; CylPush(heap,&heapcount,(CylHeap){0,start});
        while (heapcount)
        {
            CylHeap item=CylPop(heap,&heapcount); int a=item.vertex;
            if (item.distance!=distance[a]) { continue; }
            if (loop[a]==1-bottom) { target=a; break; }
            for (int link=head[a];link>=0;link=next[link])
            {
                int e=link/2,b=edges[e].a==a ? edges[e].b : edges[e].a;
                if (edges[e].other<0) { continue; }
                double delta[3]; for (int k=0;k<3;k++) { delta[k]=points[a].p[k]-points[b].p[k]; }
                double du=fabs(raw[a]-raw[b]); du=fmin(du,1-du);
                double d=distance[a]+sqrt(CylDot(delta,delta))+du*perimeter*4;
                if (d<distance[b]) { distance[b]=d; parent[b]=e; CylPush(heap,&heapcount,(CylHeap){d,b}); }
            }
        }
        if (target<0) { *reason="No connected edge path joins the cylinder's rims."; goto done; }
        for (int a=target;a!=start;)
        { int e=parent[a]; edges[e].seam=1; marked++; a=edges[e].a==a ? edges[e].b : edges[e].a; }
    }
    /* Explicit and automatic cuts must each be one unbranched path from one
     * rim to the other. Never silently move or disregard an authored seam. */
    memset(degree,0,(size_t)n*sizeof(*degree));
    for (int e=0;e<ne;e++) if (edges[e].seam) { degree[edges[e].a]++; degree[edges[e].b]++; }
    int endpoints[2]={-1,-1},endcount=0,pathstart=-1;
    for (int i=0;i<n;i++) if (degree[i])
    {
        pathstart=i;
        if (degree[i]==1 && endcount<2 && loop[i]>=0) { endpoints[endcount++]=i; }
        else if (degree[i]!=2 || loop[i]>=0) { *reason="Mark one continuous, unbranched seam from one open rim to the other."; goto done; }
    }
    if (endcount!=2 || loop[endpoints[0]]==loop[endpoints[1]])
    { *reason="The marked seam must connect the two open rims."; goto done; }
    int read=0,write=0; queue[write++]=pathstart; seen[pathstart]=1;
    while (read<write)
    {
        int a=queue[read++];
        for (int link=head[a];link>=0;link=next[link]) if (edges[link/2].seam)
        {
            int b=edges[link/2].a==a ? edges[link/2].b : edges[link/2].a;
            if (!seen[b]) { seen[b]=1; queue[write++]=b; }
        }
    }
    if (write!=marked+1) { *reason="The marked seam contains disconnected pieces or a loop."; goto done; }
    for (int f=0;f<facecount;f++)
    {
        double sorted[3];
        for (int c=0;c<3;c++) { sorted[c]=raw[map[faces[f].vertices[c]]]; }
        for (int a=0;a<3;a++) for (int b=a+1;b<3;b++) if (sorted[a]>sorted[b])
        { double t=sorted[a]; sorted[a]=sorted[b]; sorted[b]=t; }
        double gap=-1,start=0;
        for (int c=0;c<3;c++)
        { double d=sorted[c]-(c?sorted[c-1]:sorted[2]-1); if (d>gap) { gap=d; start=sorted[c]; } }
        for (int c=0;c<3;c++)
        {
            int a=map[faces[f].vertices[c]];
            uv[f][c][0]=raw[a]+(raw[a]<start-1e-10 ? 1 : 0);
            uv[f][c][1]=(height[a]-hmin)*extent;
        }
    }
    memset(seen,0,(size_t)facecount*sizeof(*seen)); read=write=0; queue[write++]=0; seen[0]=1;
    while (read<write)
    {
        int f=queue[read++];
        for (int c=0;c<3;c++)
        {
            CylEdge *e=&edges[faceedges[f*3+c]];
            if (e->other<0 || e->seam) { continue; }
            int g=e->face==f ? e->other/3 : e->face; double shift=0;
            for (int end=0;end<2;end++)
            {
                int vertex=end?e->b:e->a,fc=0,gc=0;
                while (map[faces[f].vertices[fc]]!=vertex) { fc++; }
                while (map[faces[g].vertices[gc]]!=vertex) { gc++; }
                double d=uv[f][fc][0]+offset[f]-uv[g][gc][0];
                if (end && fabs(d-shift)>1e-7) { *reason="A triangle spans too far around the cylinder to unwrap."; goto done; }
                shift=d;
            }
            if (seen[g] && fabs(offset[g]-shift)>1e-7)
            { *reason="The seam does not open the cylinder into a single strip."; goto done; }
            if (!seen[g]) { offset[g]=shift; seen[g]=1; queue[write++]=g; }
        }
    }
    if (write!=facecount) { *reason="Select one connected cylinder wall."; goto done; }
    double minimum=HUGE_VAL;
    for (int f=0;f<facecount;f++) for (int c=0;c<3;c++) { minimum=fmin(minimum,uv[f][c][0]+offset[f]); }
    for (int f=0;f<facecount;f++) for (int c=0;c<3;c++)
    { uv[f][c][0]=(uv[f][c][0]+offset[f]-minimum)*perimeter*extent; }
    *reason=""; ok=1;
done:
    free(points); free(edges); free(rim); free(heap); free(map); free(loop); free(boundary);
    free(degree); free(head); free(next); free(queue); free(faceedges); free(parent); free(seen);
    free(raw); free(height); free(distance); free(offset);
    return ok;
}
