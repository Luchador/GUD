#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <src/objectfadeformat.h>
#include <src/propconstants.h>
#include <src/doorconstants.h>
typedef float f32;
typedef int32_t s32;
typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t u8;
typedef int Gfx;
typedef struct { union { struct { float x,y,z; }; float f[3]; }; } coord3d;
typedef struct { float m[4][4]; } Mtxf;
typedef union { u32 word; u8 rgba[4]; } Colour;
typedef struct { int numMatrices; float BoundingVolumeRadius; } Header;
typedef struct { Header *obj; void *anim; } Model;
typedef struct ObjectRecord {
    int type; u32 flags2; Model *model; Mtxf mtx; coord3d position; Colour shadecol;
} ObjectRecord;
typedef struct PropRecord {
    int type,flags,timetoregen; ObjectRecord *obj; coord3d pos;
    struct PropRecord *parent,*child,*prev,*next; void *stan; u8 rooms[4];
    u16 objectFadeStart,objectFadeEnd;
} PropRecord;
struct TintedGlassRecord { ObjectRecord base; int calculatedopacity; };
struct DoorRecord { ObjectRecord base; int doorFlags,calculatedopacity; };
struct rgba_f32 { float r,g,b,a; };
struct rgba_s32 { union { struct { int r,g,b,a; }; int rgba[4]; }; };
struct view4f { float left,top,width,height; };
typedef struct { int flags,zbufferenabled,PropType; Gfx *gdl; Colour envcolour,fogcolour; } ModelRenderData;
static ModelRenderData g_DefaultPropRenderData, rendered;
static struct { float c_recipscaley; } player={100}, *g_CurrentPlayer=&player;
static float g_PropFadeStartPx, g_PropFadeEndPx;
static Mtxf camera, view;
static int cameraMissing, renders, projections, sizes, envVisible=1;
static PropRecord *g_FreeProps;
#define PROP_TYPE_OBJ 1
#include "constants.inc"
static Mtxf *currentPlayerGetViewToWorldMtxf(void) { return cameraMissing ? NULL : &camera; }
static Mtxf *camGetWorldToViewMtxf(void) { projections++; return &view; }
static int occlusionCount(void) { return 0; }
static int occlusionEnabled(void) { return 0; }
static int occlusionTestSphere(const float *pos,float radius) { return 0; }
static int envGetPropDistColor(PropRecord *p,struct rgba_f32 *c) { memset(c,0,sizeof(*c)); return envVisible; }
static float modelGetInstSize(Model *m) { sizes++; return 100; }
static int getPropCombinedRoomsBBox2D(PropRecord *p,struct view4f *v) { return 0; }
static Gfx *bgScissorCurrentPlayerViewF(Gfx *g,float l,float t,float w,float h) { return g; }
static Gfx *bgScissorCurrentPlayerViewDefault(Gfx *g) { return g; }
static int objGetShotsTaken(ObjectRecord *o) { return 0; }
static void lerp_rgba_s32_with_rgba_f32(struct rgba_s32 *c,int a,struct rgba_f32 *b) {}
static void objRenderPropModel(PropRecord *p,ModelRenderData *rd,int pass) { rendered=*rd; renders++; }
#include "logic.inc"

static void SetWord(float *dest,u32 value) { memcpy(dest,&value,4); }
static int Render(PropRecord *p,int pass)
{ Gfx commands[1]; renders=0; objRenderProp(p,commands,pass); return renders; }

int main(void)
{
    Header header={1,100}; Model model={&header,NULL};
    ObjectRecord obj={.type=PROPDEF_PROP,.model=&model};
    PropRecord prop={.type=PROP_TYPE_OBJ,.obj=&obj};
    SetWord(&obj.mtx.m[1][0],OBJECT_FADE_TAG);
    SetWord(&obj.mtx.m[1][1],(2000u<<16)|3000u);
    objInitFadeDistances(&prop,&obj);
    assert(prop.objectFadeStart==2000 && prop.objectFadeEnd==3000);
    memset(&obj.mtx,0,sizeof(obj.mtx)); /* Normal placement overwrites the metadata. */
    assert(prop.objectFadeStart==2000 && prop.objectFadeEnd==3000);
    const int types[]={PROPDEF_PROP,PROPDEF_MONITOR,PROPDEF_AUTOGUN};
    view.m[2][2]=-1;
    for(unsigned i=0;i<sizeof(types)/sizeof(*types);i++)
    {
        obj.type=types[i];
        prop.pos=(coord3d){.x=0,.y=0,.z=2000};
        assert(objCalcDistanceFadeAlpha(&prop)==255);
        g_PropFadeStartPx=1000; g_PropFadeEndPx=900; /* Global fade would completely hide this prop. */
        projections=sizes=0;
        assert(Render(&prop,0) && rendered.flags==1 && rendered.PropType==9);
        assert(Render(&prop,1) && rendered.flags==2);
        assert(!projections && !sizes); /* Custom fade replaces the global path. */
        prop.pos.z=2500;
        assert(objCalcDistanceFadeAlpha(&prop)==127);
        assert(!Render(&prop,0) && Render(&prop,1));
        assert(rendered.flags==3 && rendered.PropType==5 && rendered.envcolour.word==127);
        prop.timetoregen=30;
        assert(Render(&prop,1) && rendered.envcolour.word==63); /* Existing regeneration multiplies in. */
        prop.timetoregen=0;
        prop.pos.z=3000;
        assert(!Render(&prop,0) && !Render(&prop,1));
        prop.pos.z=5000; assert(!Render(&prop,1));
        camera.m[3][2]=2500; assert(Render(&prop,1) && rendered.envcolour.word==127);
        camera.m[3][2]=0;
        prop.pos=(coord3d){.x=1500,.y=2000,.z=0}; /* Radial 25m, not camera depth. */
        assert(objCalcDistanceFadeAlpha(&prop)==127);
        prop.pos=(coord3d){.z=2500};
        g_PropFadeStartPx=-1; assert(Render(&prop,1) && rendered.envcolour.word==127);
        envVisible=0; assert(!Render(&prop,1)); envVisible=1;
    }
    prop.objectFadeStart=0; prop.objectFadeEnd=65535;
    prop.pos=(coord3d){0}; assert(objCalcDistanceFadeAlpha(&prop)==255);
    prop.pos.x=65535; assert(objCalcDistanceFadeAlpha(&prop)==0);
    cameraMissing=1; assert(objCalcDistanceFadeAlpha(&prop)==255); cameraMissing=0;
    objInitFadeDistances(&prop,&obj); assert(!prop.objectFadeStart && !prop.objectFadeEnd);
    g_PropFadeStartPx=1000; g_PropFadeEndPx=900; prop.pos=(coord3d){.z=2500};
    projections=sizes=0; assert(!Render(&prop,1) && projections && sizes);
    g_PropFadeStartPx=-1; assert(Render(&prop,0)); /* Untagged default behavior. */
    SetWord(&obj.mtx.m[1][0],OBJECT_FADE_TAG); SetWord(&obj.mtx.m[1][1],(3000u<<16)|2000u);
    objInitFadeDistances(&prop,&obj); assert(!prop.objectFadeEnd); /* Invalid tag payload falls back. */
    SetWord(&obj.mtx.m[1][1],(32767u<<16)|40000u); /* These integer bits encode a NaN as float. */
    objInitFadeDistances(&prop,&obj); assert(prop.objectFadeStart==32767 && prop.objectFadeEnd==40000);
    prop.objectFadeStart=2000; prop.objectFadeEnd=3000;
    g_FreeProps=NULL; chrpropFree(&prop); assert(chrpropAllocate()==&prop);
    assert(!prop.objectFadeStart && !prop.objectFadeEnd && !chrpropAllocate());
    puts("PASS: native metadata, radial camera distance, fade endpoints/midpoint, both passes, console/drone types, defaults, regeneration, fog and prop reuse.");
}
