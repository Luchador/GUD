#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../../../src/game/skygradientmath.h"
typedef float f32;typedef int32_t s32;typedef uint32_t u32;typedef uint8_t u8;
typedef uint16_t u16;typedef int16_t s16;typedef int bool;
typedef union { struct {float x,y,z;}; float f[3]; } coord3d;
#include "types.inc"
typedef struct { struct {s16 ob[3];u16 flag;s16 tc[2];u8 cn[4];} v;} Vtx;
typedef struct {float m[4][4];} Mtx;
typedef int Gfx;
typedef struct {float unk00,unk04,unk08,unk0c,unk10,r,g,b,a;} SkyRelated18;
enum {FALSE=0,TRUE=1,G_CYC_1CYCLE,G_AC_NONE,G_CC_SHADE,G_CD_BAYER,G_RM_OPA_SURF=10,G_RM_OPA_SURF2,
G_ZBUFFER=1,G_FOG=2,G_LIGHTING=4,G_TEXTURE_GEN=8,G_TEXTURE_GEN_LINEAR=16,G_CULL_BOTH=32,
G_SHADE=64,G_SHADING_SMOOTH=128,G_TX_RENDERTILE=0,G_OFF=0,G_ON=1,G_MTX_PROJECTION=4,G_MTX_LOAD=2,G_MTX_NOPUSH=0,G_MTX_MODELVIEW=0,G_MTX_PUSH=1};
static EnvironmentRecord env;
static bool g_SkyGradientActive;
static f32 g_SkyGradientEndSine,g_SkyCloudOffset;
static Vtx vertices[63];static Mtx matrices[2],camera;
static int vertexBytes=10000,gfxFree=10000,allocations,triangles,loads,matrixCount,stackDepth;
static unsigned persp;
static uintptr_t capturedProjection;
static const Vtx *loaded;
static int loadedCount;
static float pitch,roll,aspect=4.0f/3.0f;
static float left,top,width=320,height=240;
static coord3d eye;
static EnvironmentRecord *envGetCurrent(void){return &env;}
static coord3d *bondviewGetPlayerPosition(void){return &eye;}
static int dynGetFreeVertexBytes(void){return vertexBytes;}
static int dynGetFreeGfx(Gfx *g){return gfxFree;}
static Vtx *dynAllocateVertices(int n){assert(n==63);allocations++;return vertices;}
static Mtx *dynAllocateMatrix(void){assert(matrixCount<2);return &matrices[matrixCount++];}
static Mtx *camGetPlayerProjMtx(void){return &camera;}
static int viGetPerspNorm(void){return 123;}
#define osVirtualToPhysical(p) ((uintptr_t)(p))
static void guOrtho(Mtx *m,float l,float r,float b,float t,float n,float f,float scale)
{assert(l==-512&&r==512&&b==-512&&t==512&&n==-1&&f==1&&scale==1);memset(m,0,sizeof(*m));}
static void guMtxIdent(Mtx *m){memset(m,0,sizeof(*m));}
static float getPlayer_c_screenwidth(void){return width;}
static float getPlayer_c_screenheight(void){return height;}
static void skyGetWorldPosFromScreenPos(float x,float y,coord3d *out)
{
    float sx=(2*x/width-1)*aspect*.577350269f;
    float sy=(1-2*(y+env.Sky.HorizonYOffset)/height)*.577350269f;
    float rx=sx*cosf(roll)-sy*sinf(roll),ry=sx*sinf(roll)+sy*cosf(roll);
    out->x=rx;out->y=sinf(pitch)+ry*cosf(pitch);out->z=-cosf(pitch)+ry*sinf(pitch);
}
#define gDPPipeSync(g) (*(g)=1)
#define gDPSetCycleType(g,a) do{assert(a==G_CYC_1CYCLE);*(g)=1;}while(0)
#define gDPSetRenderMode(g,a,b) do{assert(a==G_RM_OPA_SURF&&b==G_RM_OPA_SURF2);*(g)=1;}while(0)
#define gDPSetAlphaCompare(g,a) (*(g)=1)
#define gDPSetCombineMode(g,a,b) do{assert(a==G_CC_SHADE&&b==G_CC_SHADE);*(g)=1;}while(0)
#define gDPSetColorDither(g,a) (*(g)=1)
#define gSPClearGeometryMode(g,a) (*(g)=1)
#define gSPSetGeometryMode(g,a) (*(g)=1)
#define gSPTexture(g,a,b,c,d,e) (*(g)=1)
#define gSPMatrix(g,p,flags) do{if((flags)&G_MTX_PROJECTION)capturedProjection=p;if((flags)&G_MTX_PUSH)stackDepth++;*(g)=1;}while(0)
#define gSPPopMatrix(g,a) do{stackDepth--;*(g)=1;}while(0)
#define gSPPerspNormalize(g,a) do{persp=a;*(g)=1;}while(0)
#define gSPVertex(g,p,n,first) do{assert(n==14&&first==0);loaded=(void *)(p);loadedCount=n;loads++;*(g)=1;}while(0)
static void Triangle(int a,int b,int c)
{
    assert(a>=0&&a<loadedCount&&b>=0&&b<loadedCount&&c>=0&&c<loadedCount);
    const Vtx *v[]={loaded+a,loaded+b,loaded+c};
    for(int i=0;i<3;i++)assert(v[i]->v.ob[0]>=-512&&v[i]->v.ob[0]<=512&&v[i]->v.cn[3]==255);
    assert((v[1]->v.ob[0]-v[0]->v.ob[0])*(v[2]->v.ob[1]-v[0]->v.ob[1])
        -(v[2]->v.ob[0]-v[0]->v.ob[0])*(v[1]->v.ob[1]-v[0]->v.ob[1])<0);
    triangles++;
}
#define gSP1Triangle(g,a,b,c,d) do{Triangle(a,b,c);*(g)=1;}while(0)
#include "runtime.inc"
static void Reset(void){matrixCount=allocations=triangles=loads=0;g_SkyGradientActive=FALSE;}
int main(void)
{
    Gfx commands[256];env.Sky=(SkySettings){.Red=160,.Green=192,.Blue=240,.CloudRed=100,.CloudGreen=150,.CloudBlue=200};
    env.SkyGradient=(SkyGradientSettings){1,90,16,48,96,0};
    for(int mode=0;mode<6;mode++)
    {
        pitch=(mode%3)*.78539816f;roll=mode>=3?1.57079633f:0;
        width=mode>=3?160:320;height=mode>=3?110:240;left=mode>=3?160:0;top=mode>=3?110:0;
        Reset();Gfx *end=skyRenderGradient(commands,&env);
        assert(end>commands&&end-commands<=136&&allocations==1&&matrixCount==2&&triangles==96&&loads==8);
        assert(g_SkyGradientActive&&!stackDepth&&capturedProjection==(uintptr_t)&camera&&persp==123);
        assert(vertices[0].v.ob[0]==-512&&vertices[6].v.ob[0]==512&&vertices[0].v.ob[1]==512&&vertices[56].v.ob[1]==-512);
        for(int y=0;y<9;y++)for(int x=0;x<7;x++)
        {
            coord3d ray;skyGetWorldPosFromScreenPos(width*x/6,height*y/8,&ray);
            float t=skyGradientAmount(ray.f,1);Vtx *v=&vertices[y*7+x];
            assert(abs(v->v.cn[0]-(int)(160-144*t+.5f))<=1);
        }
    }
    vertexBytes=1135;Reset();assert(skyRenderGradient(commands,&env)==commands&&!allocations&&!g_SkyGradientActive);
    vertexBytes=10000;gfxFree=135;Reset();assert(skyRenderGradient(commands,&env)==commands&&!allocations);
    gfxFree=10000;Reset();skyRenderGradient(commands,&env);
    coord3d pos={{0,1000,0}};SkyRelated18 cloud;
    skySetCloudVertex(&cloud,&pos,0,1);
    assert(fabsf(cloud.r-100*(1-16/255.0f))<.01f&&cloud.a==255);
    skySetCloudVertex(&cloud,&pos,1,1);assert(!cloud.r&&!cloud.g&&!cloud.b);
    g_SkyGradientActive=FALSE;skySetCloudVertex(&cloud,&pos,0,1);
    assert(fabsf(cloud.r-(160+100*(1-160/255.0f)))<.01f);
    for(int degrees=1;degrees<=90;degrees++)
    {
        float a=degrees*.01745329252f,end=skyGradientEndSine(degrees),last=0;
        for(int n=0;n<=100;n++)
        {
            float e=a*n/100;float ray[]={0,sinf(e),-cosf(e)};
            float t=skyGradientAmount(ray,end);assert(t>=last-1e-5f&&t>=0&&t<=1);last=t;
        }
        assert(last>.9999f);float below[]={0,-1,0};assert(!skyGradientAmount(below,end));
    }
    puts("PASS: native gradient mesh, all RSP loads/triangles, full viewport coverage, pitch/roll, state restoration, memory guards, cloud contribution and angular falloff.");
}
