#ifndef _explosions_H_
#define _explosions_H_
#include <ultra64.h>
#include <bondtypes.h>


#define EXPLOSION_BUFFER_LEN 6
#define EXPLOSION_PARTS_LEN 40
#define SMOKE_BUFFER_LEN 20
#define SMOKE_PARTS_LEN 10
#define SCORCH_BUFFER_LEN 20
#define IMPACT_TYPE_LEN 20
#define BULLET_IMPACT_BUFFER_LEN 100
#define MAX_CORNFLAKES 20

typedef struct s_smoketype {
    s16 duration;
    s16 rateappear;
    s16 ratedissolve;
    s16 size;
    f32 bg_rotrate;
    u8 r;
    u8 g;
    u8 b;
    f32 fg_rotrate;
    s16 propagated_clouds;
} s_smoketype;

typedef struct ExplosionType {
    f32 hrange;
    f32 vrange;
    f32 hchange;
    f32 vchange;
    f32 explosion_size;
    f32 explosion_range;
    f32 dmg_range;
    s16 duration;
    s16 propagationrate;
    f32 flareanimspeed;
    s16 numshrapnelbits;
    f32 shrapnel_size;
    f32 shrapnel_scatter_dist;
    f32 shrapnel_hvel;
    f32 shrapnel_vvel;
    u8 smoketype;
    u8 sndID;
    f32 damage;
} ExplosionType;

typedef struct s_impacttype {
    f32 width;
    f32 height;
    u8 apptype;
    u8 unk1;
    u8 unk2;
} s_impacttype;

struct ExplosionPart
{
    coord3d pos;
    f32 size;
    f32 rot;
    s16 frame;
    u8 bb;
};

struct Explosion {
    PropRecord *prop; // maybe explosion prop
    PropRecord *source; // maybe source prop (e.g., crate)
    struct ExplosionPart parts[EXPLOSION_PARTS_LEN];
    s16 age; // some kind of current tick + total elapsed?
    s16 nextDamageAge; // Apply damage again when age reaches this value.
    s8 explosion_type;
    s8 unk3CD;
    s8 player;
    s8 unk3CF;
    struct coord3d pos;
    s16 room;
    s16 unk3DE;
};

struct SmokePart
{
    coord3d pos;
    f32 size;
    f32 rot;
    f32 deltarot;
    f32 offset1;
    f32 offset2;
    f32 alpha;
    s16 count;
};

struct Smoke {
    PropRecord *prop;
    s16 duration;
    s16 smoke_type;
    struct SmokePart parts[SMOKE_PARTS_LEN];
};

struct Scorch {
    s16 roomid;
    u8 unk02;
    s8 unk03;
    struct coord3d pos;
    f32 explosion_size;
    struct Model *model;
    Vtx vertex_list[4];
};

struct BulletImpact {
    s16 room;
    s16 impact_type;
    u32 unk04;
    Vtx vertex_list[4];
    PropRecord* prop;
    s8 model_render_pos_index;
    s8 room_clear_flag;
    s16 unk4E;
};

struct Cornflakes {
    s32 unk00;
    struct coord3d position;
    struct coord3d rotation;
    struct coord3d position_drift;
    struct coord3d rotation_drift;
    s32 unk34;
    Vtx vertex_list[4];
};

extern f32 g_SpExplosionDamageMult;
extern struct Smoke *g_SmokeBuffer;
extern struct Explosion *g_ExplosionBuffer;

/**
 * Cornflakes are the little bits of debris that fly out of bullet impacts. This is the actual name the Rare developers
 * used for them.
 */
extern s32 g_MaxCornflakes;
extern struct Cornflakes *g_CornflakeBuffer;
extern s32 g_NumCornflakeEntries;

extern struct Scorch *g_ScorchBuffer;
extern struct BulletImpact *g_BulletImpactBuffer;
extern s32 g_NumExplosionEntries;
extern s32 g_NumSmokeEntries;
extern s32 g_NumScorchEntries;
extern s32 g_NumImpactEntries;


void explosionInitScaleMtx(void);
s32 explosionSmokeTick(PropRecord *arg0);
Gfx *explosionRenderCornflakes(Gfx *gdl);
Gfx *explosionCallRenderBulletImpactOnProp(Gfx *gdl);
Gfx *explosionRenderScorchBuffer(Gfx *arg0);
Gfx *explosionRender(PropRecord *prop, Gfx *gdl, s32 withalpha);
Gfx *explosionRenderPropSmoke(PropRecord *arg0, Gfx *arg1, s32 withalpha);
Gfx *explosionRenderBulletImpactOnProp(Gfx *arg0, PropRecord *arg1, s32 arg2);
void explosionCreateSmoke(coord3d *pos, StandTile *stan, s16 smoke_type, u8 *rooms, s32 flags);
void explosionUpdateCornflakes(void);
u8 explosionChrpropSmokeTick(PropRecord* prop);
u8 explosionChrpropExplosionTick(PropRecord* prop);
void explosionScreenShake(coord3d* source_pos, coord3d* source_mag, coord3d* result);
void explosionCreateBulletImpact(struct coord3d *pos, struct coord3d *arg1, s16 impact_type, s16 room, PropRecord *prop, s8 model_render_pos_index, s8 room_clear_flag);
s32 explosionTick(PropRecord* arg0);
void explosionCreate(PropRecord *arg0, struct coord3d *target_pos, StandTile *target_stan, s16 /* enum EXPLOSION_DEF */ explosion_type, s32 arg4, s32 player, u8 *rooms, s32 arg7);

#endif
