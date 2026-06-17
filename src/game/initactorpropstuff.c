#include <ultra64.h>
#include "initactorpropstuff.h"
#include "initanitable.h"
#include "chr.h"
#include "gun.h"
#include "math.h"
#include "math_floor.h"

/**
 * Representation of a model animation entry.
 *
 * This is intentionally smaller than ModelAnimation. 
 * Function sub_GAME_7F00032C walks this table with a 0x1c byte stride 
 * and patches the address field from an offset into ptr_animation_table to an absolute pointer.
 *
 * The runtime ModelAnimation struct is 0x40 bytes, so using ModelAnimation
 * here would make entries++ advance by 0x40 and would not match the original codegen.
 */
typedef struct ModelAnimationEntry {
    s32 address;
    u16 unk04; // next frame
    u8 unk06;
    u8 unk07; // bit 0 is loop flag
    u16 unk08;
    u16 unk0A;
    u16 unk0C;
    u16 unk0E;
    s32 unk10;
    s32 unk14;
    s32 unk18;
} ModelAnimationEntry;

/**
 * Gets the number of currently allocated heads and bodies
 * Note: Compile-time static? why bother with a function?
 */
void reset_counter_rand_body_head(void)
{
    num_bodies = 0;
    while (list_of_bodies[num_bodies] >= 0)
    {
        num_bodies++;
    }
#ifdef ISGOLDFINGER
    return; //return early as we have a new function for heads
#endif
    num_male_heads = 0;
    while (random_male_heads[num_male_heads] >= 0)
    {
        num_male_heads++;
    }

    num_female_heads = 0;
    while (random_female_heads[num_female_heads] >= 0)
    {
        num_female_heads++;
    }
}


u32 sub_GAME_7F0001F0(void *ani, int aniid, int param_3) {
    u16 asStack8[4];
    u16 result = 0;

    while (aniid < param_3) {
        result += sub_GAME_7F06D2E4(0, 0, &skeleton_guard, ani, aniid, asStack8);
        aniid++;
    }
    return result;
}


s32 sub_GAME_7F000290(ModelAnimation *anim, s32 startframe, s32 endframe)
{
    s32 sum;
    s16 out[3];

    sum = 0;

    if (startframe < endframe)
    {
        do
        {
            sub_GAME_7F06D2E4(0, 0, &skeleton_guard, anim, startframe, out);

            startframe++;
            sum += out[2];
        }
        while (startframe < endframe);
    }

    return sum;
}


/**
 * Address: 7F00032C
 * 
 * pd is raceInitAnimGroup
 */
s32 sub_GAME_7F00032C(struct weapon_firing_animation_table *animconfig)
{
    s32 animoffset;
    s32 numconfigs;
    struct weapon_firing_animation_table *config;

    union
    {
        unsigned int offset;
        struct ModelAnimation *anim;
    } *initialanim;

    s32 endframe;
    u32 angle16;
    f32 duration;

    numconfigs = 0;
    config = animconfig;

    if (animconfig->anim.offset != 0)
    {
        f32 fullturn = M_TAU_F;
        f32 angleconv = 0.0000958738f;

        initialanim = (void *)&animconfig->anim;
        animoffset = (*initialanim).offset;

        do
        {
            config->anim.anim = (struct ModelAnimation *)(((0, animoffset)) + ((s32)ptr_animation_table));
            endframe = floorFloatToInt(config->unk04);
            angle16 = sub_GAME_7F0001F0(config->anim.anim, 0, endframe) & 0xffff;
            duration = config->unk04;

            if (duration > 0.0f)
            {
                if (((s32)angle16) < 0x8000)
                {
                    config->turn_angle_per_frame = (angle16 * angleconv) / duration;
                }
                else
                {
                    config->turn_angle_per_frame = ((angle16 * angleconv) - fullturn) / duration;
                }
            }
            else
            {
                config->turn_angle_per_frame = 0.0f;
            }

            animoffset = config[1].anim.offset;
            config++;
            numconfigs++;
        }
        while (animoffset != 0);
    }

    return numconfigs;
}


//pd is raceInitAnimGroups
void sub_GAME_7F00046C(struct anim_group_info **groups)
{
    s32 i;

    for (i = 0; i < 32; i++)
    {
        if (groups[i]->len < 0)
        {
            groups[i]->len = sub_GAME_7F00032C(groups[i]->table);
        }
    }
}


/**
 * Address: 7F0004D0
 */
s32 sub_GAME_7F0004D0(ModelAnimationEntry *entries)
{
    s32 count;
    ModelAnimationEntry *entry;
    s32 address;
    ModelAnimationEntry *ptr_animation_table_addr;

    count = 0;
    entry = entries;
    ptr_animation_table_addr = (ModelAnimationEntry *)(&ptr_animation_table);

    if (1);

    if (entry->address != 0)
    {
        do
        {
            address = (*entry).address;
            entries = ptr_animation_table_addr;
            count++;
            entry++;
            ptr_animation_table_addr = (ModelAnimationEntry *)(&ptr_animation_table);
            entry[-1].address = (*((s32 *)entries)) + (0, address);
        }
        while (entry->address != 0);
    }

    return count;
}


#define ANIM_PTR(anim) \
    ((ModelAnimation *)((s32)&anim + ((s32)ptr_animation_table)))

#define ANIM_FRAC(anim) \
    ((((f32)sub_GAME_7F000290(ANIM_PTR(anim), 0, ANIM_PTR(anim)->unk04 - 1)) * 0.10000001f) / \
        ((f32)((u32)ANIM_PTR(anim)->unk04)))

#define ANIM_FRAC_MUL_FIRST(anim) \
    ((0.10000001f * ((f32)sub_GAME_7F000290(ANIM_PTR(anim), 0, ANIM_PTR(anim)->unk04 - 1))) / \
        ((f32)((u32)ANIM_PTR(anim)->unk04)))

void initWeaponAnimGroups(void)
{
    s32 i;

    if (D_8002C914[0].id != (-1))
    {
        i = 0;

        do
        {
            if (D_8002C914[i].field_1C != NULL)
            {
                D_8002C914[i].field_20 = sub_GAME_7F0004D0((ModelAnimationEntry *)D_8002C914[i].field_1C);
            }

            if (D_8002C914[i].field_24 != NULL)
            {
                D_8002C914[i].field_28 = sub_GAME_7F0004D0((ModelAnimationEntry *)D_8002C914[i].field_24);
            }

            i++;
        }
        while (D_8002C914[i].id != (-1));
    }

    sub_GAME_7F0004D0((ModelAnimationEntry *)D_8002DEBC);

    sub_GAME_7F00046C(ptr_rifle_firing_animation_groups);
    sub_GAME_7F00046C(ptr_pistol_firing_animation_groups);
    sub_GAME_7F00046C(ptr_doubles_firing_animation_groups);
    sub_GAME_7F00046C(ptr_crouched_rifle_firing_animation_groups);
    sub_GAME_7F00046C(ptr_crouched_pistol_firing_animation_groups);
    sub_GAME_7F00046C(ptr_crouched_doubles_firing_animation_groups);

    sub_GAME_7F00032C(D_80030078);
    sub_GAME_7F00032C(D_80030660);

    D_80030984 = ANIM_FRAC(ANIM_DATA_walking);
    D_80030988 = ANIM_FRAC(ANIM_DATA_running);
    D_8003098C = ANIM_FRAC(ANIM_DATA_sprinting);
    D_80030990 = ANIM_FRAC(ANIM_DATA_walking_unarmed);
    D_80030994 = ANIM_FRAC(ANIM_DATA_running_one_handed_weapon);
    D_80030998 = ANIM_FRAC(ANIM_DATA_sprinting_one_handed_weapon);
    D_8003099C = ANIM_FRAC(ANIM_DATA_walking_female);
    D_800309A0 = ANIM_FRAC_MUL_FIRST(ANIM_DATA_running_female);
    D_800309A4 = ANIM_FRAC(ANIM_DATA_sprinting_one_handed_weapon);
}

#undef ANIM_PTR
#undef ANIM_FRAC
#undef ANIM_FRAC_MUL_FIRST


/**
 * Address: 7F000980
 */
void casingsInit(void) {
    initCasingPool();
}


/**
 * Address: 7F0009A0
 * 
 * Sets the header field of every g_Casings entry to NULL.
 */
void initCasingPool(void) {
    CasingRecord *end = &g_Casings[20];
    CasingRecord *ptr = &g_Casings[0];
    
    while (end > ptr) 
    {
        ptr->header = NULL;
        ptr++;
    }
}
