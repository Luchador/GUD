#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef uint8_t u8;
typedef int16_t s16;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int32_t s32;
typedef float f32;
typedef struct { f32 x, y, z; } coord3d;
typedef struct { u8 r, g, b, a; } rgba_u8;
typedef struct { f32 m[4][4]; } Mtxf;
typedef union { struct { s16 ob[3]; u16 flag; s16 tc[2]; u8 cn[4]; } v; uint64_t align; } Vtx;
typedef struct { s32 op; uintptr_t address; s32 a, b, c; } Gfx;
enum { CULL, MATRIX, VERTICES, TRIANGLES, BIND };
#define G_CULL_BACK 0x2000
#define G_MTX_PROJECTION 1
#define G_MTX_LOAD 2
#define G_MTX_NOPUSH 0

static Vtx vertices[280];
static s32 vertexCount;
static Mtxf view = {{{1, 0, 0, 0}, {0, 1, 0, 0}}}, projection, projectionView, roomMatrices[4];
static coord3d roomPositions[4] = {{0, 0, 0}, {10, 20, 30}, {-5, 7, 13}, {1, 2, 3}};

static void Emit(Gfx *out, s32 op, uintptr_t address, s32 a, s32 b, s32 c)
{ *out = (Gfx){op, address, a, b, c}; }
#define gSPSetGeometryMode(p, bits) Emit(p, CULL, 0, bits, 0, 0)
#define gSPMatrix(p, address, flags) Emit(p, MATRIX, (uintptr_t)(address), flags, 0, 0)
#define gSPVertex(p, address, n, v0) Emit(p, VERTICES, (uintptr_t)(address), n, v0, 0)
#define gSP2Triangles(p, a, b, c, d, e, f, g, h) \
    Emit(p, TRIANGLES, 0, (a) | ((b) << 8) | ((c) << 16), (e) | ((f) << 8) | ((g) << 16), (d) | ((h) << 8))
#define osVirtualToPhysical(p) ((uintptr_t)(p))
static s32 camIsPosInScreen(coord3d *position, f32 radius) { (void)radius; return position->z >= 0; }
static Mtxf *currentPlayerGetViewToWorldMtxf(void) { return &view; }
static Mtxf *camGetPlayerProjViewMtx(void) { return &projectionView; }
static Mtxf *camGetPlayerProjMtx(void) { return &projection; }
static coord3d *getRoomPositionByID(s32 room) { assert(room >= 0 && room < 4); return &roomPositions[room]; }
static f32 bgGetRoomScale(void) { return 2.0f; }
static Vtx *dynAllocateVertices(s32 count)
{
    Vtx *out = vertices + vertexCount;
    vertexCount += count;
    assert(vertexCount <= 280);
    return out;
}
/* The production room helper emits only this matrix command. */
static Gfx *applyRoomMatrixToDisplayList(Gfx *gdl, s32 room)
{ gSPMatrix(gdl++, &roomMatrices[room], G_MTX_LOAD); return gdl; }

/* PRODUCTION */

typedef struct Draw {
    Vtx vertices[4];
    sImageTableEntry image;
    uintptr_t modelMatrix, projectionMatrix;
    s32 style, depth, offset, geometry, triangle1, triangle2, triangleFlags;
} Draw;

static s32 Decode(Gfx *start, Gfx *end, Draw *draws, s32 *bindings)
{
    Draw state = {0};
    s32 count = 0;
    *bindings = 0;
    for (Gfx *cmd = start; cmd < end; cmd++)
    {
        switch (cmd->op)
        {
        case BIND:
            state.image = *(sImageTableEntry *)cmd->address;
            state.style = cmd->a; state.depth = cmd->b; state.offset = cmd->c;
            (*bindings)++;
            break;
        case CULL: state.geometry |= cmd->a; break;
        case MATRIX:
            if (cmd->a & G_MTX_PROJECTION) state.projectionMatrix = cmd->address;
            else state.modelMatrix = cmd->address;
            break;
        case VERTICES:
            assert(cmd->a == 4 && cmd->b == 0);
            memcpy(state.vertices, (void *)cmd->address, sizeof(state.vertices));
            break;
        case TRIANGLES:
            assert(*bindings > 0 && count < 70);
            state.triangle1 = cmd->a; state.triangle2 = cmd->b; state.triangleFlags = cmd->c;
            draws[count++] = state;
            break;
        default: assert(0);
        }
    }
    if (count) assert(state.projectionMatrix == (uintptr_t)&projection);
    return count;
}

static sImageTableEntry images[] = {
    {0x10000, 64, 64, 0, 3, 1, 0, 0, 0},
    {0x11000, 32, 32, 4, 0, 2, 0, 0, 0},
    /* Same pixels, different sampling/material settings must rebind. */
    {0x10000, 64, 64, 0, 3, 1, 2, 2, 0},
    {0x12000, 32, 16, 0, 2, 1, 0, 0, 0}
};

static BulletSpark Spark(s32 id, s32 image)
{
    BulletSpark spark = {0};
    spark.lifetime = 10; spark.room = id % 4; spark.framesPerTick = 1;
    spark.imageFrames = &images[image]; spark.position = (coord3d){id * 3, id * 2, id + 10};
    spark.radius = 5; spark.rotationCosRadius = 3; spark.rotationSinRadius = 2;
    spark.color = (rgba_u8){id, id + 1, id + 2, 255 - id};
    return spark;
}

static void Clear(void)
{
    memset(g_BulletSparks, 0, sizeof(g_BulletSparks));
    memset(g_MovingBulletSparks, 0, sizeof(g_MovingBulletSparks));
}

static void Check(s32 depth, s32 expectedDraws, s32 expectedBindings)
{
    Gfx commands[1024], *end = commands;
    Draw actual[70], isolated[70];
    s32 bindings, count, referenceCount = 0;
    vertexCount = 0;
    fxRenderAllSparks(&end, depth);
    count = Decode(commands, end, actual, &bindings);
    assert(count == expectedDraws && bindings == expectedBindings);

    /* Render each spark with an independent initial binding. Compare every
     * draw's material, vertices, matrices and triangle order with the batch. */
    for (s32 i = 0; i < BULLET_SPARKS_MAX + BULLET_MOVING_SPARKS_MAX; i++)
    {
        BulletSpark *spark = i < BULLET_SPARKS_MAX ? &g_BulletSparks[i]
            : &g_MovingBulletSparks[i - BULLET_SPARKS_MAX].spark;
        sImageTableEntry *bound = NULL;
        vertexCount = 0; end = commands;
        fxRenderBulletSpark(spark, &end, depth, &bound);
        s32 draws = Decode(commands, end, isolated, &bindings);
        assert(draws <= 1 && bindings == draws);
        if (draws)
        {
            assert(referenceCount < count);
            Draw *batch = &actual[referenceCount++], *single = &isolated[0];
            assert(!memcmp(batch->vertices, single->vertices, sizeof(batch->vertices)));
            assert(!memcmp(&batch->image, &single->image, sizeof(batch->image)));
            assert(batch->modelMatrix == single->modelMatrix && batch->projectionMatrix == single->projectionMatrix);
            assert(batch->style == single->style && batch->depth == single->depth && batch->offset == single->offset);
            assert(batch->geometry == single->geometry && batch->triangle1 == single->triangle1
                && batch->triangle2 == single->triangle2 && batch->triangleFlags == single->triangleFlags);
            assert(isolated[0].image.index == spark->imageFrames[(s32)(spark->age * spark->framesPerTick)].index);
            assert(isolated[0].style == 4 && isolated[0].depth == depth && isolated[0].offset == 2);
        }
    }
    assert(referenceCount == count);
}

int main(void)
{
    Clear(); Check(1, 0, 0);
    /* Reuse spans all 20 stationary and 50 moving sparks, including rooms. */
    for (s32 i = 0; i < BULLET_SPARKS_MAX; i++) g_BulletSparks[i] = Spark(i, 0);
    for (s32 i = 0; i < BULLET_MOVING_SPARKS_MAX; i++) g_MovingBulletSparks[i].spark = Spark(i + 20, 0);
    for (s32 depth = 0; depth <= 2; depth++) Check(depth, 70, 1);
    /* Every invocation, including a later frame with changed image data,
     * must establish a fresh binding. */
    images[0].index += 0x1000; images[0].flagsS = 1;
    Check(1, 70, 1);

    Clear();
    g_BulletSparks[0] = Spark(0, 0);
    g_BulletSparks[1] = Spark(1, 1); g_BulletSparks[1].lifetime = 0;
    g_BulletSparks[2] = Spark(2, 2); g_BulletSparks[2].age = -1;
    g_BulletSparks[3] = Spark(3, 3); g_BulletSparks[3].position.z = -1;
    g_BulletSparks[4] = Spark(4, 0);
    g_MovingBulletSparks[0].spark = Spark(5, 0);
    Check(1, 3, 1);

    Clear();
    g_BulletSparks[0] = Spark(0, 0);
    g_BulletSparks[1] = Spark(1, 1);
    g_MovingBulletSparks[0].spark = Spark(2, 0);
    Check(1, 3, 3); /* A/B/A is not A/A. */
    images[2].index = images[0].index;
    g_BulletSparks[1] = Spark(1, 2);
    Check(1, 3, 3); /* Aliased pixels with different wrapping. */
    g_BulletSparks[1] = Spark(1, 0); g_BulletSparks[1].age = 1;
    Check(1, 3, 3); /* Same animation array, different frame. */

    Clear(); g_MovingBulletSparks[0].spark = Spark(0, 3);
    Check(0, 1, 1); Check(2, 1, 1);
    puts("PASS: spark binding reuse, 70-to-1 setup reduction, texture/frame changes, culled gaps, pass reset, depth modes, draw order and geometry.");
    return 0;
}
