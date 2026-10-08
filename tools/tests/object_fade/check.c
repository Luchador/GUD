#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <src/objectfadeformat.h>
#include <src/glassopacityformat.h>
#include <stdbool.h>
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
    int type; u32 flags2, runtime_bitflags; Model *model; Mtxf mtx; coord3d position; Colour shadecol;
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
#define PROP_TYPE_PLAYER 5
#define PROP_TYPE_MAX 9
#define TRUE true
#define FALSE false
#include "constants.inc"
static Mtxf *currentPlayerGetViewToWorldMtxf(void) { return cameraMissing ? NULL : &camera; }
static Mtxf *camGetWorldToViewMtxf(void) { projections++; return &view; }
static int boxes, occlusionOn, covered, sphereTests;
static float testedRadius;
static int occlusionCount(void) { return boxes; }
static int occlusionEnabled(void) { return occlusionOn; }
static int occlusionTestSphere(const float *pos,float radius)
{ sphereTests++; testedRadius=radius; return covered; }
static int envGetPropDistColor(PropRecord *p,struct rgba_f32 *c) { memset(c,0,sizeof(*c)); return envVisible; }
static float modelGetInstSize(Model *m) { sizes++; return m->obj->BoundingVolumeRadius; }
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

static void MonitorOcclusion(void)
{
    Header header={1,100}; Model model={&header,NULL};
    ObjectRecord obj={.type=PROPDEF_MONITOR,.model=&model};
    PropRecord prop={.type=PROP_TYPE_OBJ,.obj=&obj};
    g_PropFadeStartPx=-1; envVisible=1;
    obj.mtx.m[0][0]=2; obj.mtx.m[1][1]=3; obj.mtx.m[2][2]=4;
    obj.mtx.m[0][1]=1; /* Non-uniform scaling plus shear. */
    boxes=occlusionOn=covered=1;
    for(int type=PROPDEF_MONITOR;type<=PROPDEF_MULTI_MONITOR;type++) {
        obj.type=type;
        for(int hidden=0;hidden<2;hidden++) for(int pass=0;pass<2;pass++) {
            covered=hidden;
            assert(Render(&prop,pass));
            assert(rendered.flags==((pass?2:1)|(hidden?MODEL_RENDER_OCCLUDED:0)));
            assert(fabsf(testedRadius-100*sqrtf(30))<.001f);
        }
        covered=1;
        for(int exclusion=0;exclusion<7;exclusion++) {
            switch(exclusion) {
            case 0: boxes=0; break;
            case 1: occlusionOn=0; break;
            case 2: prop.parent=&prop; break;
            case 3: prop.child=&prop; break;
            case 4: model.anim=&model; break;
            case 5: header.numMatrices=2; break;
            case 6: prop.type=99; break;
            }
            sphereTests=0;
            assert(Render(&prop,0) && rendered.flags==1 && !sphereTests);
            boxes=occlusionOn=1; prop.parent=prop.child=NULL; model.anim=NULL;
            header.numMatrices=1; prop.type=PROP_TYPE_OBJ;
        }
        prop.objectFadeStart=2000; prop.objectFadeEnd=3000; prop.pos.z=2500;
        memset(&camera,0,sizeof(camera));
        assert(!Render(&prop,0) && Render(&prop,1));
        assert(rendered.flags==(3|MODEL_RENDER_OCCLUDED) && rendered.envcolour.word==127);
        prop.objectFadeStart=prop.objectFadeEnd=0; prop.pos.z=0;
        obj.flags2=PROPFLAG2_DISABLE_ZBUFFER;
        assert(!Render(&prop,0) && Render(&prop,1) && rendered.flags==(3|MODEL_RENDER_OCCLUDED));
        obj.flags2=0;
    }
    obj.type=PROPDEF_PROP; assert(!Render(&prop,0)); /* Existing fast skip. */
    struct DoorRecord door={.base=obj}; door.base.type=PROPDEF_DOOR; prop.obj=&door.base;
    assert(Render(&prop,0) && rendered.flags==1); prop.obj=&obj;
    obj.type=PROPDEF_GLASS; assert(Render(&prop,0) && rendered.flags==1);
    boxes=occlusionOn=covered=0;
    puts("PASS: monitor occlusion, both passes, partial cover, scale/shear bounds, fades and safety exclusions.");
}

static void ScreenSizeFade(void)
{
    Header header={1,100}; Model model={&header,NULL};
    ObjectRecord obj={.type=PROPDEF_PROP,.model=&model};
    PropRecord prop={.type=PROP_TYPE_OBJ,.obj=&obj};
    const float diameters[]={20,50,100,199,200,400};
    view.m[2][2]=-1;
    g_PropFadeStartPx=g_PropFadeEndPx=0;
    for (unsigned i=0;i<sizeof(diameters)/sizeof(*diameters);i++)
    {
        header.BoundingVolumeRadius=diameters[i]/2;
        prop.pos.z=diameters[i]*100/12.5f;
        assert(Render(&prop,0) && rendered.flags==1 && rendered.PropType==9);
        prop.pos.z=diameters[i]*100/11.25f;
        assert(!Render(&prop,0) && Render(&prop,1));
        assert(rendered.flags==3 && rendered.PropType==5
            && rendered.envcolour.word>=126 && rendered.envcolour.word<=127);
        prop.pos.z=diameters[i]*100/10;
        assert(!Render(&prop,0) && !Render(&prop,1));
    }
    /* At the same distance, small props now disappear before larger props. */
    prop.pos.z=1000; header.BoundingVolumeRadius=25;
    assert(!Render(&prop,0) && !Render(&prop,1));
    header.BoundingVolumeRadius=100; assert(Render(&prop,0));
    /* Camera projection and per-level thresholds still determine apparent size. */
    header.BoundingVolumeRadius=25; prop.pos.z=500;
    assert(!Render(&prop,0) && !Render(&prop,1));
    player.c_recipscaley=200; assert(Render(&prop,0));
    player.c_recipscaley=100;
    g_PropFadeStartPx=20; g_PropFadeEndPx=15;
    prop.pos.z=250; assert(Render(&prop,0));
    prop.pos.z=5000/17.5f;
    assert(!Render(&prop,0) && Render(&prop,1) && rendered.envcolour.word==127);
    prop.pos.z=500; assert(!Render(&prop,0) && !Render(&prop,1));
    g_PropFadeStartPx=-1; assert(Render(&prop,0));
    g_PropFadeStartPx=g_PropFadeEndPx=0;
    puts("PASS: small/large models share fade boundaries; actual size, projection, level settings and zero-alpha render skipping.");
}

static void GlassOpacity(void)
{
    Header header={1,100}; Model model={&header,NULL};
    ObjectRecord obj={.type=PROPDEF_GLASS,.model=&model};
    PropRecord prop={.type=PROP_TYPE_OBJ,.obj=&obj,.objectFadeStart=2000,.objectFadeEnd=3000};
    g_PropFadeStartPx=-1; camera.m[3][2]=0;
    for(u32 alpha=0;alpha<=255;alpha+=51)
    {
        obj.runtime_bitflags=0x54321; /* Existing runtime flags survive. */
        SetWord(&obj.mtx.m[1][2],GLASS_OPACITY_TAG|alpha);
        objInitGlassOpacity(&obj);
        memset(&obj.mtx,0,sizeof(obj.mtx));
        assert((obj.runtime_bitflags&0xfffff)==0x54321);
        assert(obj.runtime_bitflags&RUNTIMEBITFLAG_GLASS_OPACITY);
        prop.pos.z=1000;
        assert(!Render(&prop,0));
        assert(Render(&prop,1)==(alpha!=0));
        if(alpha) assert(rendered.flags==(3|MODEL_RENDER_GLASS_OPACITY)
            && rendered.PropType==5 && rendered.envcolour.word==alpha);
        prop.pos.z=2500;
        assert(Render(&prop,1)==(alpha!=0));
        if(alpha) assert(rendered.envcolour.word==127*alpha/255);
        prop.pos.z=3000; assert(!Render(&prop,1));
        prop.pos.z=1000; prop.timetoregen=30;
        assert(Render(&prop,1)==(alpha!=0));
        if(alpha) assert(rendered.envcolour.word==127*alpha/255);
        prop.timetoregen=0;
    }
    objInitGlassOpacity(&obj); /* Untagged glass restores the native material. */
    assert(obj.runtime_bitflags==0x54321);
    assert(Render(&prop,0) && rendered.flags==1 && rendered.PropType==9);
    obj.type=PROPDEF_TINTED_GLASS;
    SetWord(&obj.mtx.m[1][2],GLASS_OPACITY_TAG|128);
    objInitGlassOpacity(&obj); assert(obj.runtime_bitflags==0x54321);
    puts("PASS: per-pane opacity initialization, default/tinted isolation, native flags, render passes, zero/full opacity, fade and regeneration.");
}

int main(void)
{
    MonitorOcclusion();
    GlassOpacity();
    ScreenSizeFade();
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
