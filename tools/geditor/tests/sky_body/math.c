#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "../../../../src/game/skybodymath.h"

static void View(float yaw, float pitch, float aspect, float fov, float m[4][4])
{
    float sy=sinf(yaw),cy=cosf(yaw),sp=sinf(pitch),cp=cosf(pitch),h=tanf(fov*.5f);
    float right[]={cy,0,sy},up[]={-sy*sp,cp,cy*sp},forward[]={sy*cp,sp,-cy*cp};
    memset(m,0,16*sizeof(float));
    for(int i=0;i<3;i++){m[i][0]=right[i]/(h*aspect);m[i][1]=up[i]/h;m[i][3]=forward[i];}
}
static void Bounds(const SkyBodyVertex *p,int n)
{
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
    puts("PASS: sky disc angular diameter, translation/direction-scale invariance, tint UV orientation, poles, FOV/aspect changes and horizon/frustum clipping.");
    return 0;
}
