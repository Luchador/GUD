#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "../../../../src/game/skybodytriangle.h"

static void Cover(const SkyBodyVertex *p,int n)
{
    SkyBodyTriangleVertex t[3];float bounds[4];
    int count=skyBodyCover(p,n,t,bounds);
    if(!n){assert(!count);return;}
    if(!count)return; /* A polygon clipped to exactly a line has no pixels. */
    assert(count==3);
    float dx=t[1].x-t[0].x,dy=t[1].y-t[0].y;
    float ex=t[2].x-t[0].x,ey=t[2].y-t[0].y,area=dx*ey-ex*dy;
    assert(area!=0);
    for(int i=0;i<3;i++)
    {
        assert(isfinite(t[i].x)&&isfinite(t[i].y)&&isfinite(t[i].s)&&isfinite(t[i].t)&&isfinite(t[i].q));
        assert(fabsf(t[i].q)<=1.00001f);
        /* N64 signed screen coordinates remain in range at 320x240. */
        assert(fabsf((t[i].x+1)*160)<1023&&fabsf((1-t[i].y)*120)<1023);
    }
    /* Every visible original vertex and edge midpoint remains covered, with
     * the same perspective-correct UVs, even when clipping made 5+ vertices. */
    for(int i=0;i<n;i++)for(int midpoint=0;midpoint<2;midpoint++)
    {
        int j=midpoint?(i+1)%n:i;
        float w=(p[i].w+p[j].w)*.5f;
        float x=(p[i].x+p[j].x)*.5f/w,y=(p[i].y+p[j].y)*.5f/w;
        float b=((x-t[0].x)*ey-ex*(y-t[0].y))/area;
        float c=(dx*(y-t[0].y)-(x-t[0].x)*dy)/area,a=1-b-c;
        assert(a>=-.0002f&&b>=-.0002f&&c>=-.0002f);
        float q=a*t[0].q+b*t[1].q+c*t[2].q;
        assert(q>0);
        assert(fabsf((a*t[0].s+b*t[1].s+c*t[2].s)/q-(p[i].s+p[j].s)*.5f)<.001f);
        assert(fabsf((a*t[0].t+b*t[1].t+c*t[2].t)/q-(p[i].t+p[j].t)*.5f)<.001f);
    }
}

static void View(float yaw, float pitch, float aspect, float fov, float m[4][4])
{
    float sy=sinf(yaw),cy=cosf(yaw),sp=sinf(pitch),cp=cosf(pitch),h=tanf(fov*.5f);
    float right[]={cy,0,sy},up[]={-sy*sp,cp,cy*sp},forward[]={sy*cp,sp,-cy*cp};
    memset(m,0,16*sizeof(float));
    for(int i=0;i<3;i++){m[i][0]=right[i]/(h*aspect);m[i][1]=up[i]/h;m[i][3]=forward[i];}
}
static void Bounds(const SkyBodyVertex *p,int n)
{
    Cover(p,n);
    assert(n==0||(n>=3&&n<=SKY_BODY_MAX_VERTICES));
    for(int i=0;i<n;i++)
    {
        assert(isfinite(p[i].x)&&isfinite(p[i].y)&&isfinite(p[i].w)&&p[i].w>0);
        assert(fabsf(p[i].x/p[i].w)<=1.0001f&&fabsf(p[i].y/p[i].w)<=1.0001f);
        assert(p[i].s>=-1e-5f&&p[i].s<=1.00001f&&p[i].t>=-1e-5f&&p[i].t<=1.00001f);
        assert(p[i].height>=-1e-5f);
    }
}
int main(void)
{
    float m[4][4],d[]={0,.5f,-1},scaled[]={0,FLT_MAX*.5f,-FLT_MAX};
    SkyBodyVertex p[SKY_BODY_MAX_VERTICES],q[SKY_BODY_MAX_VERTICES];
    View(0,atanf(.5f),4.0f/3,1.04719755f,m);
    int n=skyBodyBuild(d,8,m,0,p);assert(n==4);Bounds(p,n);
    float diameter=fabsf(p[1].x/p[1].w-p[0].x/p[0].w);
    assert(fabsf(diameter-2*tanf(4*.01745329252f)/(tanf(.5235987756f)*4/3))<1e-5f);
    assert(skyBodyBuild(scaled,8,m,0,q)==n);
    for(int i=0;i<n;i++)assert(fabsf(p[i].x-q[i].x)<1e-6f&&fabsf(p[i].y-q[i].y)<1e-6f);
    /* Even huge camera translations leave position and angular size exact. */
    m[3][0]=1e30f;m[3][1]=-1e30f;m[3][2]=300;m[3][3]=25;
    assert(skyBodyBuild(d,8,m,0,q)==n&&!memcmp(p,q,n*sizeof(*p)));
    assert(skyBodyBuild(d,8,m,.2f,q)==n);
    for(int i=0;i<n;i++)assert(fabsf(q[i].y/q[i].w-p[i].y/p[i].w-.2f)<1e-6f);
    float bad[][3]={{0,0,0},{NAN,1,1},{0,INFINITY,1},{0,-1,-1}};
    for(unsigned i=0;i<sizeof(bad)/sizeof(bad[0]);i++)assert(!skyBodyBuild(bad[i],8,m,0,p));
    assert(!skyBodyBuild(d,0,m,0,p)&&!skyBodyBuild(d,91,m,0,p)&&!skyBodyBuild(d,NAN,m,0,p));
    assert(!skyBodyBuild(d,8,m,INFINITY,p));
    View(3.14159265f,0,4.0f/3,1.04719755f,m);assert(!skyBodyBuild(d,8,m,0,p));
    /* Whole-disc and partial horizon clipping, zenith, viewport edges,
     * nearly-behind views, zoom, widescreen and split-screen proportions. */
    float directions[][3]={{0,.5f,-1},{0,0,-1},{0,1,0},{1,0,0},{0,-.05f,-1}};
    int visible=0,clipped=0;
    for(int a=0;a<5;a++)for(int yaw=-180;yaw<=180;yaw+=9)for(int pitch=-90;pitch<=90;pitch+=9)
        for(int size=1;size<=90;size+=89)for(int aspect=1;aspect<=3;aspect++)
        {
            View(yaw*.01745329252f,pitch*.01745329252f,aspect*.75f,.8f,m);
            n=skyBodyBuild(directions[a],size,m,.1f,p);Bounds(p,n);
            visible+=n>0;clipped+=n>4;
        }
    assert(visible&&clipped);
    /* Tilted horizon: the entire covering triangle must stay on the sky
     * side, not merely contain the clipped image. */
    float sr=sinf(.7f),cr=cosf(.7f),half=tanf(.5235987756f),low[]={0,.05f,-1};
    View(0,0,4.0f/3,1.04719755f,m);
    for(int i=0;i<3;i++)
    {
        float right=m[i][0]*half*4/3,up=m[i][1]*half;
        m[i][0]=(cr*right+sr*up)/(half*4/3);m[i][1]=(-sr*right+cr*up)/half;
    }
    n=skyBodyBuild(low,60,m,0,p);Bounds(p,n);
    SkyBodyTriangleVertex tri[3];float bounds[4];assert(skyBodyCover(p,n,tri,bounds)==3);
    for(int i=0;i<3;i++)assert(sr*tri[i].x*half*4/3+cr*tri[i].y*half>=-.00001f);
    puts("PASS: one triangle covers the clipped sky disc with unchanged projective UVs; angular diameter, translation/direction-scale invariance, poles, FOV/aspect changes and horizon/frustum clipping.");
    return 0;
}
