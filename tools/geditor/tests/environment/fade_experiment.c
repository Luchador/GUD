#include <assert.h>
#include <float.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
typedef float f32;typedef uint8_t u8;typedef uint16_t u16;typedef int16_t s16;
typedef uint32_t u32;typedef int32_t s32;
#define bool int
#define TRUE 1
#define FALSE 0
#include "types.inc"
typedef struct { f32 x,y,z; } coord3d;
typedef struct { f32 m[4][4]; } Mtxf;
typedef struct { coord3d pos;f32 zDepth; } PropRecord;
typedef struct { f32 rgba[4]; } rgba_f32;
static Mtxf view;
static struct { f32 c_recipscaley; } player={150},*g_CurrentPlayer=&player;
static EnvironmentRecord g_CurrentEnvironment;
static f32 g_FarFogIntensity,g_DifferenceFromFarFogIntensity,g_ScaledFarFogIntensity;
static f32 g_PropFadeStartPx,g_PropFadeEndPx,renderScale=1,clipNear,clipFar,worldFar;
static Mtxf *camGetWorldToViewMtxf(void) { return &view; }
static f32 bgGetLevelRenderScale(void) { return renderScale; }
static void bgSetWorldFarClipDistance(f32 value) { worldFar=value; }
static void viSetZRange(f32 near,f32 far) { clipNear=near;clipFar=far; }
#include "fade.inc"
int main(void)
{
    /* The experiment must retain the native ROM layout for rebasing/reversal. */
    assert(sizeof(EnvironmentRecord)==104&&offsetof(EnvironmentRecord,Sky)==44);
    assert(offsetof(EnvironmentRecord,PropVisibility)==96);
    EnvironmentRecord jungle={.FogEnabled=1,
        .Visibility={.NearClipDistance=10,.FarClipDistance=2500,
            .ReservedLegacyPropFade={1500,2500,1000,0},.FogStart=996,.FogEnd=1000},
        .Sky={.Red=24,.Green=32},.PropVisibility={20,17}};
    envLoadCurrentEnvironment(&jungle);view.m[0][0]=1;
    assert(clipNear==10&&clipFar==2500&&worldFar==2500);
    coord3d offset={1600,0,0};
    /* The former Jungle cutoff culled a radius-25 prop here: adjusted depth
     * was 3400, above MaxVisRange 2500. Actual far fog still permits it. */
    assert(envIsPropVisibleThroughFog(&offset,25));
    offset.x=2525;assert(envIsPropVisibleThroughFog(&offset,25));
    offset.x=2525.25f;assert(!envIsPropVisibleThroughFog(&offset,25));
    offset.x=2400;assert(envIsPropVisibleThroughFog(&offset,0));
    PropRecord prop={.zDepth=1600};rgba_f32 color;
    assert(envGetPropDistColor(&prop,&color)==1&&color.rgba[3]>0&&color.rgba[3]<1);
    assert(fabsf(color.rgba[0]-24/255.0f)<1e-6f);
    for(int i=0;i<4;i++)jungle.Visibility.ReservedLegacyPropFade[i]=NAN;
    envLoadCurrentEnvironment(&jungle);assert(envIsPropVisibleThroughFog(&offset,0));
    assert(envGetPropDistColor(&prop,&color)==1&&isfinite(color.rgba[3]));
    renderScale=.5f;envLoadCurrentEnvironment(&jungle);assert(worldFar==5000);
    offset.x=5026;assert(!envIsPropVisibleThroughFog(&offset,25));
    jungle.FogEnabled=0;envLoadCurrentEnvironment(&jungle);
    offset.x=30000;assert(envIsPropVisibleThroughFog(&offset,25)&&envGetPropDistColor(&prop,&color)==2);
    /* Both modern screen-size fade helpers retain per-level settings,
     * engine defaults, disabled fading and their exact alpha boundaries. */
    view.m[2][2]=-1;
    const f32 depths[]={1000,1500,30000/18.5f,2000};
    const int expected[]={255,255,127,0};
    for(int i=0;i<4;i++)
    {
        prop.pos.z=depths[i];assert(chrCalcScreenFadeAlpha(&prop)==expected[i]);
        assert(objCalcScreenFadeAlpha(&prop,200)==expected[i]);
    }
    g_PropFadeStartPx=0;prop.pos.z=2400;
    assert(chrCalcScreenFadeAlpha(&prop)==255&&objCalcScreenFadeAlpha(&prop,200)==255);
    prop.pos.z=3000;assert(chrCalcScreenFadeAlpha(&prop)==0&&objCalcScreenFadeAlpha(&prop,200)==0);
    g_PropFadeStartPx=-1;
    assert(chrCalcScreenFadeAlpha(&prop)==255&&objCalcScreenFadeAlpha(&prop,200)==255);
    puts("PASS: old Jungle size-adjusted cutoff removed; hardware-fog coloring/far culling, render scale and screen-size fade preserved; native layout unchanged.");
    return 0;
}
