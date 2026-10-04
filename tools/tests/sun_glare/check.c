#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "skyglaremath.h"

typedef float f32;
typedef int32_t s32;
typedef uint32_t u32;
typedef union { struct { float x, y, z; }; float f[3]; } coord3d;
typedef struct { float x, y; } coord2d;
typedef struct { float m[4][4]; } Mtxf;
typedef struct { struct { short x, y, z; } coord; } Vertex;
typedef struct { coord3d hitpos, normal; int texturenum; } HitThing;
typedef struct { u32 Type; float AngularSize; unsigned char Red, Green, Blue, Reserved; coord3d Direction; } SkyBodySettings;
typedef struct {
    SkyBodySettings SkyBody;
    struct { float HorizonYOffset; } Sky;
    struct { float FarClipDistance; } Visibility;
} EnvironmentRecord;
typedef struct { int room_rendered; coord3d minbounds, maxbounds; } RoomInfo;
typedef struct {
    float c_halfheight, c_screentop, c_scaley, c_screenleft, c_halfwidth, c_scalex;
} Player;
typedef int Gfx;
#define TRUE true
#define FALSE false
#define G_SC_NON_INTERLACE 0

static Player player, *g_CurrentPlayer = &player;
static int playernum, g_ClockTimer, g_MaxNumRooms = 4, traces, draws;
static float levelscale, renderscale, bodyalpha;
static coord3d eye;
static Mtxf viewtoworld, projection;
static EnvironmentRecord environment;
static RoomInfo g_BgRoomInfo[4];
static Vertex triangles[4][3];
static bool hastriangle[4];
static u32 drawncolor;
static int scissor[4], rectangle[4];
static Gfx commands[32];

static int get_cur_playernum(void) { return playernum; }
static float getPlayer_c_screenwidth(void) { return player.c_halfwidth * 2; }
static float getPlayer_c_screenheight(void) { return player.c_halfheight * 2; }
static float getPlayer_c_screenleft(void) { return player.c_screenleft; }
static float getPlayer_c_screentop(void) { return player.c_screentop; }
static float envGetSkyBodyAlpha(void) { return bodyalpha; }
static float bgGetRoomScale(void) { return levelscale; }
static float bgGetLevelRenderScale(void) { return renderscale; }
static EnvironmentRecord *envGetCurrent(void) { return &environment; }
static coord3d *bondviewGetPlayerPosition(void) { return &eye; }
static Mtxf *currentPlayerGetViewToWorldMtxf(void) { return &viewtoworld; }
static int viGetViewLeft(void) { return player.c_screenleft; }
static int viGetViewTop(void) { return player.c_screentop; }
static int viGetViewWidth(void) { return player.c_halfwidth * 2; }
static int viGetViewHeight(void) { return player.c_halfheight * 2; }
static void mtx4RotateVecInPlace(Mtxf *m, float *v)
{
    float copy[3]; memcpy(copy, v, sizeof(copy));
    for (int j = 0; j < 3; j++) v[j] = copy[0]*m->m[0][j] + copy[1]*m->m[1][j] + copy[2]*m->m[2][j];
}
static Gfx *gfxSetup2DTextureMode(Gfx *gdl) { return gdl; }
static Gfx *gfxRestore3DRenderMode(Gfx *gdl) { return gdl; }
static void recordScissor(Gfx *gdl, int mode, int x, int y, int r, int b)
{
    (void)gdl; (void)mode;
    scissor[0]=x; scissor[1]=y; scissor[2]=r; scissor[3]=b;
}
#define gDPSetScissor recordScissor
static Gfx *gfxDrawTranslucentRect(Gfx *gdl, int x, int y, int r, int b, u32 color)
{
    rectangle[0]=x; rectangle[1]=y; rectangle[2]=r; rectangle[3]=b;
    drawncolor=color; draws++; return gdl;
}
static bool bgTestBulletHitBackground(coord3d *, coord3d *, int, HitThing *);
#include "production.inc"

/* Use the real game's triangle test, with the same world-to-BG scaling as
 * bgTestBulletHitBackground. Room dispatch and range checks are production. */
static bool bgTestBulletHitBackground(coord3d *from, coord3d *to, int room, HitThing *hit)
{
    coord3d start, end, delta, offset = {{0,0,0}};
    traces++;
    if (!hastriangle[room]) return false;
    for (int i=0; i<3; i++) {
        start.f[i]=from->f[i]*levelscale; end.f[i]=to->f[i]*levelscale;
        delta.f[i]=end.f[i]-start.f[i];
    }
    return intersectRayTriangle(&triangles[room][0], &triangles[room][1], &triangles[room][2],
        &offset, &start, &end, &delta, hit);
}

static void setup(void)
{
    memset(&environment,0,sizeof(environment)); memset(&eye,0,sizeof(eye));
    memset(g_BgRoomInfo,0,sizeof(g_BgRoomInfo)); memset(hastriangle,0,sizeof(hastriangle));
    memset(&viewtoworld,0,sizeof(viewtoworld)); memset(&projection,0,sizeof(projection));
    for(int i=0;i<4;i++) { viewtoworld.m[i][i]=1; skyResetGlare(i); }
    projection.m[0][0]=projection.m[1][1]=1; projection.m[2][3]=-1;
    player=(Player){120,0,1.0f/120,0,160,1.0f/160};
    playernum=0; g_ClockTimer=2; levelscale=renderscale=1; bodyalpha=255;
    environment.SkyBody.Type=1; environment.SkyBody.Direction.z=-1;
    environment.SkyBody.Red=240; environment.SkyBody.Green=160; environment.SkyBody.Blue=80;
    environment.Visibility.FarClipDistance=1000; traces=draws=0;
}
static void prepare(void)
{
    g_SunGlare[playernum].target=0;
    skyPrepareSunGlare(&environment,&projection);
}
static void wall(int room, int z)
{
    g_BgRoomInfo[room].room_rendered=1;
    g_BgRoomInfo[room].minbounds=(coord3d){{-100,-100,z}};
    g_BgRoomInfo[room].maxbounds=(coord3d){{100,100,z}};
    triangles[room][0]=(Vertex){{-100,-100,z}};
    triangles[room][1]=(Vertex){{100,-100,z}};
    triangles[room][2]=(Vertex){{0,100,z}};
    hastriangle[room]=true;
}
static void closeEnough(float a,float b) { assert(fabsf(a-b)<0.00002f); }

int main(void)
{
    setup();
    for(int angle=0;angle<=180;angle++) {
        float radians=angle*0.01745329252f, ray[3]={sinf(radians),0,-cosf(radians)};
        closeEnough(skyGlareStrength(ray),angle<20 ? 0.2f*(1-angle/20.0f) : 0);
    }
    /* Equal elapsed time at 60, 30 and 15 FPS, and a single dropped frame. */
    for(int ticks=1;ticks<=4;ticks*=2) {
        float a=0;
        for(int t=0;t<60;t+=ticks) a=skyGlareSmooth(a,0.2f,ticks);
        closeEnough(a,skyGlareSmooth(0,0.2f,60)); assert(a<=0.2f);
        closeEnough(skyGlareSmooth(a,0,0),a);
        assert(skyGlareSmooth(a,0,2)>0 && skyGlareSmooth(a,0,2)<a);
    }
    prepare(); closeEnough(g_SunGlare[0].target,0.2f);
    for(int i=0;i<60;i++) skyRenderSunGlare(commands);
    assert((drawncolor>>8)==0xf0a050 && (drawncolor&255)==51);
    wall(1,-100); assert(skySunIsBlocked(&g_SunGlare[0]));
    skyRenderSunGlare(commands); assert((drawncolor&255)>0 && (drawncolor&255)<51);
    for(int i=0;i<60;i++) skyRenderSunGlare(commands);
    assert(g_SunGlare[0].opacity<0.00001f);
    wall(1,100); assert(!skySunIsBlocked(&g_SunGlare[0])); /* Behind camera. */
    wall(1,-2000); assert(!skySunIsBlocked(&g_SunGlare[0])); /* Beyond far clip. */
    wall(1,-100); g_BgRoomInfo[1].room_rendered=0;
    assert(!skySunIsBlocked(&g_SunGlare[0]));
    wall(1,-100); g_BgRoomInfo[1].minbounds.x=10; g_BgRoomInfo[1].maxbounds.x=20;
    traces=0; assert(!skySunIsBlocked(&g_SunGlare[0]) && !traces);
    /* A room may span the segment while its actual triangle lies past it. */
    wall(1,-2000); g_BgRoomInfo[1].maxbounds.z=-10;
    traces=0; assert(!skySunIsBlocked(&g_SunGlare[0]) && traces==1);
    setup(); levelscale=0.25f; renderscale=2; prepare(); wall(2,-100);
    assert(skySunIsBlocked(&g_SunGlare[0]));
    wall(2,-200); assert(!skySunIsBlocked(&g_SunGlare[0]));
    /* Reconstructed ray follows horizon offset and camera rotation. */
    setup(); environment.Sky.HorizonYOffset=24; prepare();
    assert(g_SunGlare[0].direction.y>0.19f && g_SunGlare[0].target<0.1f);
    viewtoworld.m[2][0]=-1; viewtoworld.m[0][0]=viewtoworld.m[2][2]=0; viewtoworld.m[0][2]=1;
    prepare(); assert(g_SunGlare[0].direction.x>0.98f);
    environment.SkyBody.Direction.y=-1; prepare(); assert(g_SunGlare[0].target==0);
    environment.SkyBody.Direction=(coord3d){{0,0,0}}; prepare(); assert(g_SunGlare[0].target==0);
    environment.SkyBody.Direction=(coord3d){{NAN,0,-1}}; prepare(); assert(g_SunGlare[0].target==0);
    environment.SkyBody.Direction=(coord3d){{0,0,1}}; prepare(); assert(g_SunGlare[0].target==0);
    setup(); environment.SkyBody.Direction=(coord3d){{1,0,-1}}; prepare();
    traces=0; skyRenderSunGlare(commands); assert(g_SunGlare[0].target==0 && !traces && !draws);
    setup(); bodyalpha=127.5f; prepare(); closeEnough(g_SunGlare[0].target,0.1f);
    skyRenderSunGlare(commands); float first=g_SunGlare[0].opacity;
    playernum=1; player.c_screenleft=160; player.c_halfwidth=80; player.c_scalex=1.0f/80;
    player.c_screentop=120; player.c_halfheight=60; player.c_scaley=1.0f/60;
    bodyalpha=255; prepare(); skyRenderSunGlare(commands);
    closeEnough(g_SunGlare[0].opacity,first); closeEnough(g_SunGlare[1].opacity,first*2);
    assert(scissor[0]==160 && scissor[1]==120 && scissor[2]==320 && scissor[3]==240);
    assert(!memcmp(scissor,rectangle,sizeof(scissor)));
    environment.SkyBody.Type=2; prepare(); draws=traces=0; skyRenderSunGlare(commands);
    assert(!draws && !traces && g_SunGlare[1].opacity==0); /* No moon glare or sun afterglow. */
    playernum=0; environment.SkyBody.Type=0; skyRenderSunGlare(commands);
    assert(!draws && g_SunGlare[0].opacity==0);
    puts("Sun glare: angles, timing, occlusion, scale, horizon, fade, HUD order and split-screen passed.");
    return 0;
}
