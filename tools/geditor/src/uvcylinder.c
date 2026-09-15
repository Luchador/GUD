#include "uvprojection.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define CYLINDER_PI 3.14159265358979323846
typedef struct CylinderPoint { double p[3]; } CylinderPoint;
typedef struct CylinderFit {
    double axis[3], u[3], v[3], center[2], radius, low, high, score;
} CylinderFit;

static double CylinderDot(const double a[3], const double b[3])
{ return a[0]*b[0] + a[1]*b[1] + a[2]*b[2]; }
static void CylinderCross(const double a[3], const double b[3], double c[3])
{ c[0]=a[1]*b[2]-a[2]*b[1]; c[1]=a[2]*b[0]-a[0]*b[2]; c[2]=a[0]*b[1]-a[1]*b[0]; }
static int CylinderCompare(const void *a, const void *b)
{
    const CylinderPoint *x=a, *y=b;
    for (int k=0; k<3; k++) { if (x->p[k]!=y->p[k]) { return x->p[k]<y->p[k] ? -1 : 1; } }
    return 0;
}
static int CylinderCompareAngle(const void *a, const void *b)
{ double x=*(const double *)a,y=*(const double *)b; return (x>y)-(x<y); }

/* Symmetric 3x3 Jacobi eigensolver. Use every eigenvector as a candidate:
 * a cylinder's axis need not be its longest dimension. */
static void CylinderEigenvectors(double a[3][3], double vectors[3][3])
{
    memset(vectors,0,9*sizeof(double));
    for (int i=0; i<3; i++) { vectors[i][i]=1; }
    for (int iteration=0; iteration<32; iteration++)
    {
        int p=0,q=1;
        for (int i=0; i<3; i++) for (int j=i+1; j<3; j++)
        { if (fabs(a[i][j])>fabs(a[p][q])) { p=i; q=j; } }
        if (fabs(a[p][q])<1e-12) { break; }
        double angle=0.5*atan2(2*a[p][q],a[q][q]-a[p][p]);
        double c=cos(angle),s=sin(angle),app=a[p][p],aqq=a[q][q],apq=a[p][q];
        a[p][p]=c*c*app-2*s*c*apq+s*s*aqq;
        a[q][q]=s*s*app+2*s*c*apq+c*c*aqq;
        a[p][q]=a[q][p]=0;
        for (int k=0; k<3; k++)
        {
            if (k!=p && k!=q)
            {
                double x=a[k][p],y=a[k][q];
                a[k][p]=a[p][k]=c*x-s*y; a[k][q]=a[q][k]=s*x+c*y;
            }
            double x=vectors[k][p],y=vectors[k][q];
            vectors[k][p]=c*x-s*y; vectors[k][q]=s*x+c*y;
        }
    }
}

static int CylinderSolve(double matrix[3][4], double result[3])
{
    for (int k=0; k<3; k++)
    {
        int pivot=k;
        for (int i=k+1; i<3; i++) { if (fabs(matrix[i][k])>fabs(matrix[pivot][k])) { pivot=i; } }
        if (fabs(matrix[pivot][k])<1e-12) { return 0; }
        for (int j=k; j<4; j++)
        { double t=matrix[k][j]; matrix[k][j]=matrix[pivot][j]; matrix[pivot][j]=t; }
        double divisor=matrix[k][k];
        for (int j=k; j<4; j++) { matrix[k][j]/=divisor; }
        for (int i=0; i<3; i++) if (i!=k)
        {
            double factor=matrix[i][k];
            for (int j=k; j<4; j++) { matrix[i][j]-=factor*matrix[k][j]; }
        }
    }
    for (int k=0; k<3; k++) { result[k]=matrix[k][3]; }
    return 1;
}

static int CylinderTryAxis(const CylinderPoint *points, int count,
    const double axis[3], CylinderFit *fit)
{
    double helper[3]={0}, maxradius=0, matrix[3][4]={{0}}, circle[3], sum=0;
    int dominant=0, smallest=0, used=0;
    *fit=(CylinderFit){0};
    for (int k=1; k<3; k++)
    {
        if (fabs(axis[k])>fabs(axis[dominant])) { dominant=k; }
        if (fabs(axis[k])<fabs(axis[smallest])) { smallest=k; }
    }
    for (int k=0; k<3; k++) { fit->axis[k]=axis[k]*(axis[dominant]<0 ? -1 : 1); }
    helper[smallest]=1; CylinderCross(helper,fit->axis,fit->u);
    double length=sqrt(CylinderDot(fit->u,fit->u));
    if (length<1e-8) { return 0; }
    for (int k=0; k<3; k++) { fit->u[k]/=length; }
    CylinderCross(fit->axis,fit->u,fit->v);
    for (int i=0; i<count; i++)
    {
        double x=CylinderDot(points[i].p,fit->u),y=CylinderDot(points[i].p,fit->v);
        double h=CylinderDot(points[i].p,fit->axis);
        maxradius=fmax(maxradius,hypot(x,y));
        if (!i || h<fit->low) { fit->low=h; }
        if (!i || h>fit->high) { fit->high=h; }
    }
    if (maxradius<1e-7 || fit->high-fit->low<1e-7) { return 0; }
    for (int i=0; i<count; i++)
    {
        double x=CylinderDot(points[i].p,fit->u),y=CylinderDot(points[i].p,fit->v);
        /* Ignore cap centers when fitting the circular rim. */
        if (hypot(x,y)<maxradius*0.25) { continue; }
        double row[3]={x,y,1}, square=x*x+y*y;
        for (int a=0; a<3; a++)
        {
            for (int b=0; b<3; b++) { matrix[a][b]+=row[a]*row[b]; }
            matrix[a][3]+=row[a]*square;
        }
        used++;
    }
    if (used<6 || !CylinderSolve(matrix,circle)) { return 0; }
    fit->center[0]=circle[0]*0.5; fit->center[1]=circle[1]*0.5;
    for (int i=0; i<count; i++)
    {
        double x=CylinderDot(points[i].p,fit->u),y=CylinderDot(points[i].p,fit->v);
        if (hypot(x,y)<maxradius*0.25) { continue; }
        sum+=hypot(x-fit->center[0],y-fit->center[1]);
    }
    fit->radius=sum/used;
    if (fit->radius<1e-7) { return 0; }
    for (int i=0; i<count; i++)
    {
        double x=CylinderDot(points[i].p,fit->u),y=CylinderDot(points[i].p,fit->v);
        if (hypot(x,y)<maxradius*0.25) { continue; }
        double error=(hypot(x-fit->center[0],y-fit->center[1])-fit->radius)/fit->radius;
        fit->score+=error*error/used;
    }
    return isfinite(fit->score);
}

int UVProjectionCylinder(const UVProjectionVertex *vertices, int vertexcount,
    const UVProjectionFace *faces, int facecount, double (*uv)[3][2], const char **reason)
{
    CylinderPoint *points=NULL, *corners=NULL;
    double *angles=NULL;
    CylinderFit best={0};
    double low[3],high[3],extent=0,mean[3]={0}, covariance[2][3][3]={{{0}}};
    double candidates[9][3]={{1,0,0},{0,1,0},{0,0,1}}, seam=2*CYLINDER_PI;
    int unique=0,ok=0;
    *reason="Select the faces of one cylinder to map.";
    if (!vertices || !faces || !uv || vertexcount<6 || facecount<6) { return 0; }
    for (int i=0; i<vertexcount; i++) for (int k=0; k<3; k++)
    {
        double p=vertices[i].position[k];
        if (!isfinite(p)) { *reason="The selected faces contain invalid coordinates."; return 0; }
        if (!i || p<low[k]) { low[k]=p; } if (!i || p>high[k]) { high[k]=p; }
    }
    for (int f=0; f<facecount; f++) for (int c=0; c<3; c++)
    { if (faces[f].vertices[c]<0 || faces[f].vertices[c]>=vertexcount) { return 0; } }
    for (int k=0; k<3; k++) { extent=fmax(extent,high[k]-low[k]); }
    if (!isfinite(extent) || extent<=0) { return 0; }
    points=malloc((size_t)vertexcount*sizeof(*points)); corners=malloc((size_t)vertexcount*sizeof(*corners));
    if (!points || !corners) { *reason="Out of memory fitting a cylinder."; goto done; }
    for (int i=0; i<vertexcount; i++) for (int k=0; k<3; k++)
    { points[i].p[k]=corners[i].p[k]=(vertices[i].position[k]-low[k])/extent; }
    qsort(points,vertexcount,sizeof(*points),CylinderCompare);
    for (int i=0; i<vertexcount; i++)
    {
        if (unique && fabs(points[i].p[0]-points[unique-1].p[0])<1e-9
            && fabs(points[i].p[1]-points[unique-1].p[1])<1e-9
            && fabs(points[i].p[2]-points[unique-1].p[2])<1e-9) { continue; }
        points[unique++]=points[i];
    }
    if (unique<6) { goto done; }
    for (int i=0; i<unique; i++) for (int k=0; k<3; k++) { mean[k]+=points[i].p[k]/unique; }
    for (int i=0; i<vertexcount; i++) for (int k=0; k<3; k++) { corners[i].p[k]-=mean[k]; }
    for (int i=0; i<unique; i++)
    {
        for (int k=0; k<3; k++) { points[i].p[k]-=mean[k]; }
        for (int a=0; a<3; a++) for (int b=0; b<3; b++)
        { covariance[0][a][b]+=points[i].p[a]*points[i].p[b]/unique; }
    }
    for (int f=0; f<facecount; f++)
    {
        double ab[3],ac[3],normal[3];
        for (int k=0; k<3; k++)
        {
            ab[k]=corners[faces[f].vertices[1]].p[k]-corners[faces[f].vertices[0]].p[k];
            ac[k]=corners[faces[f].vertices[2]].p[k]-corners[faces[f].vertices[0]].p[k];
        }
        CylinderCross(ab,ac,normal); double length=sqrt(CylinderDot(normal,normal));
        if (length<1e-12) { continue; }
        for (int a=0; a<3; a++) for (int b=0; b<3; b++)
        { covariance[1][a][b]+=normal[a]*normal[b]/(length*length*facecount); }
    }
    for (int m=0; m<2; m++)
    {
        double vectors[3][3]; CylinderEigenvectors(covariance[m],vectors);
        for (int a=0; a<3; a++) for (int b=0; b<3; b++) { candidates[3+m*3+a][b]=vectors[b][a]; }
    }
    best.score=1e30;
    for (int i=0; i<9; i++)
    {
        CylinderFit fit;
        if (!CylinderTryAxis(points,unique,candidates[i],&fit)) { continue; }
        /* A short hexagon can also fit a circle when viewed sideways, after
           its apparent center points are excluded. Its face normals resolve
           that ambiguity: sides are perpendicular to the axis, caps parallel. */
        for (int f=0; f<facecount; f++)
        {
            double ab[3],ac[3],normal[3];
            for (int k=0; k<3; k++)
            {
                ab[k]=corners[faces[f].vertices[1]].p[k]-corners[faces[f].vertices[0]].p[k];
                ac[k]=corners[faces[f].vertices[2]].p[k]-corners[faces[f].vertices[0]].p[k];
            }
            CylinderCross(ab,ac,normal);
            double square=CylinderDot(normal,normal),dot=CylinderDot(normal,fit.axis);
            if (square>1e-24)
            {
                double alignment=fmin(1,dot*dot/square);
                fit.score+=0.01*fmin(alignment,1-alignment)/facecount;
            }
        }
        if (fit.score<best.score-1e-18) { best=fit; }
    }
    if (best.score>0.04) { *reason="Could not fit one cylinder. Select its side faces, with optional end caps."; goto done; }
    /* Start at a rim column after a large angular gap. Choosing the smallest
       raw angle can cut through a column that straddles zero after native
       integer rounding. The gap keeps that column together. */
    angles=malloc((size_t)unique*sizeof(*angles));
    if (!angles) { *reason="Out of memory placing the UV seam."; goto done; }
    int anglecount=0;
    for (int i=0; i<unique; i++)
    {
        double x=CylinderDot(points[i].p,best.u)-best.center[0], y=CylinderDot(points[i].p,best.v)-best.center[1];
        if (hypot(x,y)<best.radius*0.25) { continue; }
        double angle=atan2(y,x); if (angle<0) { angle+=2*CYLINDER_PI; }
        angles[anglecount++]=angle;
    }
    if (!anglecount) { goto done; }
    qsort(angles,anglecount,sizeof(*angles),CylinderCompareAngle);
    double gap=-1;
    for (int i=0; i<anglecount; i++)
    {
        double previous=i ? angles[i-1] : angles[anglecount-1]-2*CYLINDER_PI;
        if (angles[i]-previous>gap+1e-9) { gap=angles[i]-previous; seam=angles[i]; }
    }
    for (int f=0; f<facecount; f++)
    {
        double ab[3],ac[3],normal[3],minimum=1,maximum=0;
        for (int k=0; k<3; k++)
        {
            ab[k]=corners[faces[f].vertices[1]].p[k]-corners[faces[f].vertices[0]].p[k];
            ac[k]=corners[faces[f].vertices[2]].p[k]-corners[faces[f].vertices[0]].p[k];
        }
        CylinderCross(ab,ac,normal);
        double capdot=CylinderDot(normal,best.axis), length=sqrt(CylinderDot(normal,normal));
        int cap=length>0 && fabs(capdot)>0.9*length;
        for (int c=0; c<3; c++)
        {
            const double *p=corners[faces[f].vertices[c]].p;
            double x=CylinderDot(p,best.u)-best.center[0],y=CylinderDot(p,best.v)-best.center[1];
            if (cap)
            {
                uv[f][c][0]=0.5+(capdot<0 ? -x : x)/(2*best.radius);
                uv[f][c][1]=0.5+y/(2*best.radius);
            }
            else
            {
                double angle=atan2(y,x)-seam;
                angle=fmod(angle,2*CYLINDER_PI);
                if (angle<0) { angle+=2*CYLINDER_PI; }
                uv[f][c][0]=angle/(2*CYLINDER_PI);
                uv[f][c][1]=(CylinderDot(p,best.axis)-best.low)/(best.high-best.low);
                minimum=fmin(minimum,uv[f][c][0]); maximum=fmax(maximum,uv[f][c][0]);
            }
        }
        if (!cap && maximum-minimum>0.5)
        { for (int c=0; c<3; c++) if (uv[f][c][0]<0.5) { uv[f][c][0]+=1; } }
    }
    *reason=""; ok=1;
done:
    free(points); free(corners); free(angles);
    return ok;
}
