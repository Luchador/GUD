#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef uint8_t u8;
typedef int8_t s8;
typedef uint16_t u16;
typedef int16_t s16;
typedef uint32_t u32;
typedef int32_t s32;
typedef float f32;
typedef int bool;
#define TRUE 1
#define FALSE 0
#define G_VTX 4
#define G_TRI1 (-65)
#define G_TRI4 (-79)
#define G_ENDDL (-72)
#define G_SETTIMG 0xfd
#define M_U32_MAX_VALUE_F 4294967296.0f

typedef struct { union { struct { f32 x, y, z; }; f32 f[3]; }; } coord3d;
typedef struct { struct { s16 x, y, z; } coord; s16 index, s, t; u8 r, g, b, a; } Vertex;
typedef struct { u8 bytes[8]; } Gfx;
typedef struct HitThing {
    coord3d hitpos, normal;
    Vertex *vtx0, *vtx1, *vtx2;
    Gfx *tricmd;
    s16 unk28, texturenum, tileformat, tilesize;
} HitThing;
typedef struct { s32 x, y, z; } BoundVec;
typedef struct { s16 gdlindex; } RoomVtxBatchBounds;
typedef struct { Gfx *primaryGdl; Vertex *vertices; } RoomInfo;
static RoomInfo g_BgRoomInfo[2];
/* Production accesses this table using its N64 24-byte stride. */
static struct { u8 unused[12]; coord3d pos; } ptr_bgdata_room_fileposition_list[2];
static BoundVec D_80044868 = {32767, 32767, 32767}, D_80044874 = {-32768, -32768, -32768};
static BoundVec D_80044880 = {32767, 32767, 32767}, D_8004488C = {-32768, -32768, -32768};
static BoundVec D_8003204C = {32767, 32767, 32767}, D_80032058 = {-32768, -32768, -32768};
static BoundVec D_80032070 = {32767, 32767, 32767}, D_8003207C = {-32768, -32768, -32768};
static coord3d D_80032064, D_80032088;

/* Translate emulated physical/KSEG0 addresses to a host-aligned RAM buffer. */
static union { uint64_t align; u8 bytes[65536]; } ramStorage;
#define ram ramStorage.bytes
static u32 osMemSize = sizeof(ram);
static u32 physical(uintptr_t p) {
    if (p >= (uintptr_t)ram && p < (uintptr_t)(ram + sizeof(ram))) return p - (uintptr_t)ram;
    return p & 0x1fffffff;
}
#define K0_TO_PHYS(p) physical((uintptr_t)(p))
#define PHYS_TO_K0(p) (ram + (p))
#define IS_KSEG0(p) ((uintptr_t)(p) >= (uintptr_t)ram && (uintptr_t)(p) < (uintptr_t)(ram + sizeof(ram)))
static u32 be32(const u8 *p) { return (u32)p[0] << 24 | (u32)p[1] << 16 | (u32)p[2] << 8 | p[3]; }
static u16 be16(const u8 *p) { return (u16)p[0] << 8 | p[1]; }
static void word(u8 *p, u32 v) { p[0] = v >> 24; p[1] = v >> 16; p[2] = v >> 8; p[3] = v; }

/* Observe production impact dispatch without audio, particles or room loading. */
typedef struct { s32 weapon; coord3d dir; } ShotData;
enum { HIT_DEFAULT, HIT_WATER, HIT_SNOW, ITEM_WATCHLASER, ITEM_LASER,
       EXPLOSION_DEF_01, SPARK_LASER, SPARK_WATCHLASER, SPARK_STANDARD };
#define NUM_TEXTURES 4096
static struct { s32 hitTexture; } g_Textures[NUM_TEXTURES];
struct image_sound { u8 *thing2; s32 thing2_len; };
static struct image_sound emptyEffects;
static struct image_sound *g_HitTypeSounds[] = {&emptyEffects, &emptyEffects, &emptyEffects};
static struct { void *stan; } playerProp;
static s32 breaks, brokenRoom, brokenTri, soundTexture;
static Gfx *brokenCommand;
static void lightFixtureBreak(Gfx *cmd, s32 tri, s32 room) {
    breaks++; brokenCommand = cmd; brokenTri = tri; brokenRoom = room;
}
static void gunfirePlaySfxRicochetSounds(s32 weapon, coord3d *pos, s32 tex) { soundTexture = tex; }
static void explosionCreateBulletImpact(coord3d *p, coord3d *n, s32 a, s32 b, s32 c, s32 d, s32 e) {}
static s32 randomGetNext(void) { return 0; }
#define getCurrentPlayerProp() (&playerProp)
static s32 get_cur_playernum(void) { return 0; }
static void explosionCreate(void *p, coord3d *pos, void *stan, s32 a, s32 b, s32 c, u8 *rooms, s32 d) {}
static void gunSetBeamTarget(coord3d *pos) {}
static void fxCreateBulletSpark(coord3d *pos, s32 type, s32 room) {}
