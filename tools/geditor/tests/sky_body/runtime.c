#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../../../../src/game/skybodymath.h"
typedef float f32;typedef uint32_t u32;typedef int32_t s32;typedef unsigned char u8;
typedef struct {float m[4][4];} Mtxf;
typedef union {struct{float x,y,z;};float f[3];} coord3d;
typedef struct {u32 Type;f32 AngularSize;u8 Red,Green,Blue,Reserved;coord3d Direction;} SkyBodySettings;
typedef struct {struct{float HorizonYOffset;} Sky;SkyBodySettings SkyBody;} EnvironmentRecord;
typedef struct {u32 index;u8 width,height,level,format,depth,flagsS,flagsT,pad;} sImageTableEntry;
typedef struct {f32 unk00,unk04,unk08,unk0c,r,g,b,a,unk20,unk24,unk28,unk2c,unk30,unk34;} SkyRelated38;
typedef int Gfx;
struct tex {void *data;u8 width,height,gbiformat,depth;};
enum {TRUE=1,G_IM_FMT_IA=3,G_IM_SIZ_8b=1,G_TX_CLAMP=2,TEX_RENDER_TRANSLUCENT=2,
    G_RM_XLU_SURF=7,G_RM_XLU_SURF2=8,G_TP_PERSP,G_TF_BILERP,G_TC_FILT,G_AC_NONE,G_CC_MODULATEIA};
static unsigned NUM_TEXTURES=0xaa6,requested,triangles,loads;
static int missing;
static struct tex texture={(void*)0x12340,64,64,G_IM_FMT_IA,G_IM_SIZ_8b};
static Mtxf view,projection;
static sImageTableEntry selected;
static float getPlayer_c_screenwidth(void){return 320;}
static float getPlayer_c_screenheight(void){return 240;}
static float getPlayer_c_screenleft(void){return 20;}
static float getPlayer_c_screentop(void){return 10;}
static Mtxf *currentPlayerGetProjectionMatrixF(void){return &projection;}
static Mtxf *camGetWorldToViewMtxf(void){return &view;}
static void matrix_4x4_multiply(Mtxf *p,Mtxf *v,Mtxf *out){*out=*p;}
static void texLoadFromTextureNum(u32 id,void *pool){requested=id;loads++;}
static struct tex *texFindInPool(u32 id,void *pool){assert(id==requested);return missing?NULL:&texture;}
static u32 osVirtualToPhysical(void *p){return (u32)(uintptr_t)p;}
static void texSelect(Gfx **gdl,sImageTableEntry *im,int style,int depth,int origin)
{assert(style==TEX_RENDER_TRANSLUCENT&&depth==0&&origin==0);selected=*im;}
#define gDPPipeSync(g) (*(g)=1)
#define gDPSetRenderMode(g,a,b) do{assert((a)==G_RM_XLU_SURF&&(b)==G_RM_XLU_SURF2);*(g)=2;}while(0)
#define gDPSetTexturePersp(g,a) (*(g)=3)
#define gDPSetTextureFilter(g,a) (*(g)=4)
#define gDPSetTextureConvert(g,a) (*(g)=5)
#define gDPSetAlphaCompare(g,a) (*(g)=6)
#define gDPSetCombineMode(g,a,b) (*(g)=7)
static Gfx *skyRenderTri(Gfx *gdl,SkyRelated38 *a,SkyRelated38 *b,SkyRelated38 *c,float scale,int textured)
{
    SkyRelated38 *v[]={a,b,c};assert(scale==130&&textured);triangles++;
    for(int i=0;i<3;i++)
    {
        assert(v[i]->unk28>=80&&v[i]->unk28<=1360&&v[i]->unk2c>=40&&v[i]->unk2c<=1000);
        assert(v[i]->unk20>=-16&&v[i]->unk20<=2032&&v[i]->unk24>=-16&&v[i]->unk24<=2032);
        assert(v[i]->unk0c>0&&v[i]->unk34>0);
        assert(v[i]->r==255&&v[i]->g==210&&v[i]->b==160&&v[i]->a==255);
    }
    return gdl;
}
#include "runtime.inc"
int main(void)
{
    EnvironmentRecord env={.SkyBody={1,8,255,210,160,0,{{0,.5f,-1}}}};
    Gfx list[100];projection.m[0][0]=1;projection.m[1][1]=1;projection.m[2][3]=-1;
    assert(skyRenderBody(list,&env)>list&&triangles==2&&requested==0xaa4);
    assert(selected.index==0x12340&&selected.width==64&&selected.height==64&&!selected.level);
    assert(selected.flagsS==G_TX_CLAMP&&selected.flagsT==G_TX_CLAMP);
    env.SkyBody.Type=2;triangles=0;skyRenderBody(list,&env);assert(triangles==2&&requested==0xaa5);
    env.SkyBody.Type=0;triangles=loads=0;assert(skyRenderBody(list,&env)==list&&!loads&&!triangles);
    env.SkyBody.Type=2;NUM_TEXTURES=0xaa5;assert(skyRenderBody(list,&env)==list&&!loads);
    NUM_TEXTURES=0xaa6;missing=1;assert(skyRenderBody(list,&env)==list&&!triangles);missing=0;
    texture.depth=2;assert(skyRenderBody(list,&env)==list&&!triangles);texture.depth=1;
    texture.width=128;assert(skyRenderBody(list,&env)==list&&!triangles);texture.width=64;
    env.SkyBody.Direction.z=1;loads=0;assert(skyRenderBody(list,&env)==list&&!loads);
    puts("PASS: production N64 adapter: IA8 dimensions, sun/moon IDs, tint, perspective UVs, no depth, missing images/pool failure and offscreen early exit.");
}
