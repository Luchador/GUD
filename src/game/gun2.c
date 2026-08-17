#include <ultra64.h>
#include "include/limits.h"
#include <bondconstants.h>
#include <bondtypes.h>
#include <bondgame.h>
#include <music.h>
#include <snd.h>
#include "bondview.h"
#include "bondinv.h"
#include "gun.h"
#include "chrobjdata.h"
#include "game/propobj.h"
#include "game/objective_status.h"
#include "quaternion.h"
#include "image_bank.h"
#include "bondwalk2.h"
#include "othermodemicrocode.h"
#include "player.h"
#include "lv.h"
#include "random.h"
#include "math_asinfacosf.h"
#include "loadobjectmodel.h"
#include "objecthandler.h"
#include "image.h"
#include "tex.h"
#include "debugmenu_handler.h"
#include "fr.h"
#include "assets/obseg/text/LgunE.h"
#include "textrelated.h"
#include "chrai.h"
#include "model.h"
#include "options.h"
#include "mpmenu.h"
#include "joy.h"
#include "matrixmath.h"
#include "bondinv.h"
#include "stan.h"
#include "gbi_extension.h"

extern f32 g_GunScreenAspectRatio;
#if VERSION_EU
extern f32 g_GunSightAspectRatio;
#endif
extern u32 D_80035EA4;
extern ALSoundState *g_CasingSfxState;
extern u32 g_DefaultCasingModelRenderData[];

#define AMMO_RELATED_MAX 30
extern AmmoStats ammo_related[AMMO_RELATED_MAX];
extern const char g_GunHudIntegerFormat[];
extern const char aSD[];
extern const char g_GunDeathCountFormat[];
extern const char aSD_0[];


#if defined(VERSION_US)
/**
 * Address 0x7F068190.
*/
void sub_GAME_7F068190(coord3d *zeropos, coord3d *vec)
{
    zeropos->x = 0.0f;
    zeropos->y = 0.0f;
    zeropos->z = 0.0f;

    transformAndNormalizeByLength2Dto3D(&g_CurrentPlayer->crosshair_angle, vec, 1.0f);
}


/*
* Address: 0x7f0681cc
* This function computes the angle the player's bullets are fired at
*/
void bullet_path_from_screen_center(coord3d* arg0, coord3d* result, enum GUNHAND arg2)
{
    coord2d crosspos;
    s32 unused;
    f32 inaccuracy;
    f32 scaledspread;
    f32 randfactor;

    inaccuracy = get_ptr_item_statistics(getCurrentPlayerWeaponId(arg2))->Inaccuracy;
    if ((bondwalkItemCheckBitflags(get_item_in_hand_or_watch_menu(arg2), WEAPONSTATBITFLAG_FIRST_SHOT_ACCURACY) != 0) && (g_CurrentPlayer->hands[arg2].volley == 1))
    {
        // Single shots are four times more accurate
        inaccuracy *= 0.25f;
    }

    scaledspread = (120.0f * inaccuracy) / viGetFovY();

    randfactor = (RANDOMFRAC() - 0.5f) * RANDOMFRAC();
    crosspos.x = g_CurrentPlayer->crosshair_angle.f[0] + randfactor * scaledspread * getPlayer_c_screenwidth() * g_GunScreenAspectRatio
        / (getPlayer_c_perspaspect() * 320.0f);

    randfactor = (RANDOMFRAC() - 0.5f) * RANDOMFRAC();
    crosspos.y =  g_CurrentPlayer->crosshair_angle.f[1] + randfactor * scaledspread * getPlayer_c_screenheight()
        / (PAL ? (f32)(SCREEN_HEIGHT_272) : (f32)(SCREEN_HEIGHT_240));

    arg0->x = 0.0f;
    arg0->y = 0.0f;
    arg0->z = 0.0f;

    // Result is a normalized vector describing the path the bullet will follow
    // Can be used to compute x,y,z displacement off the center of the screen if done for a projectile
    transformAndNormalizeByLength2Dto3D(&crosspos, result, 1.0f);
}


/*
* Address: 0x7f068420
*/
CasingRecord* casingCreate(ModelFileHeader* header, Mtxf* mtx)
{
    CasingRecord* entry = g_Casings;
    CasingRecord* end = g_Casings + ARRAYCOUNT(g_Casings);

    while (entry < end && entry->header != NULL)
    {
        entry++;
    }

    if (entry < end)
    {
        entry->header = header;

        entry->pos.x = mtx->m[3][0];
        entry->pos.y = mtx->m[3][1];
        entry->pos.z = mtx->m[3][2];
#if VERSION_EU
        matrix_7f05842c_eu(mtx, entry->rot_mtx);
#else
        entry->rot_mtx.m[0][0] = mtx->m[0][0];
        entry->rot_mtx.m[0][1] = mtx->m[0][1];
        entry->rot_mtx.m[0][2] = mtx->m[0][2];
        entry->rot_mtx.m[0][3] = 0.0f;

        entry->rot_mtx.m[1][0] = mtx->m[1][0];
        entry->rot_mtx.m[1][1] = mtx->m[1][1];
        entry->rot_mtx.m[1][2] = mtx->m[1][2];
        entry->rot_mtx.m[1][3] = 0.0f;

        entry->rot_mtx.m[2][0] = mtx->m[2][0];
        entry->rot_mtx.m[2][1] = mtx->m[2][1];
        entry->rot_mtx.m[2][2] = mtx->m[2][2];
        entry->rot_mtx.m[2][3] = 0.0f;

        entry->rot_mtx.m[3][0] = 0.0f;
        entry->rot_mtx.m[3][1] = 0.0f;
        entry->rot_mtx.m[3][2] = 0.0f;
        entry->rot_mtx.m[3][3] = 1.0f;
#endif
        return entry;
    }

    return NULL;
}
#endif


#if VERSION_EU
#define THROWMTX_OFFSET      0xAD0
#define THROWPOS_OFFSET      0xB00
#define THROWPOS_PREV_OFFSET 0xB40
#else
#define THROWMTX_OFFSET      0xAD8
#define THROWPOS_OFFSET      0xB08
#define THROWPOS_PREV_OFFSET 0xB48
#endif
 
#define THROWMTX     ((Mtxf *) ((u8 *) g_CurrentPlayer + handoffset + THROWMTX_OFFSET))
#define THROWPOS(k)  (((f32 *) ((u8 *) g_CurrentPlayer + handoffset + THROWPOS_OFFSET))[k])
#define THROWPREV(k) (((f32 *) ((u8 *) g_CurrentPlayer + handoffset + THROWPOS_PREV_OFFSET))[k])
 
extern f32 D_800543B4;
extern f32 D_800543B8;
extern f32 D_800543BC;
extern f32 D_800543C0;
extern f32 D_800543C4;
extern f32 D_800543C8;
extern f32 D_800543CC;
extern f32 D_800543D0;
extern f32 D_800543D4;
extern f32 expended_shell_initial_gravity_modifier_pistol;
extern f32 D_800543DC;
extern f32 D_800543E0;
extern f32 D_800543E4;
extern f32 D_800543E8;
extern f32 D_800543EC;
extern f32 D_800543F0;
extern f32 D_800543F4;
extern f32 D_800543F8;
extern f32 D_800543FC;
extern f32 expended_shell_initial_gravity_modifier_non_pistol;
 
 
/**
 * Address: 7F068508
 * 
 * Ejects a spent cartridge casing from the gun in the given hand.
 */
#if !defined(VERSION_EU)
void sub_GAME_7F068508(GUNHAND handnum, f32 floor_y_pos)
{
    CasingRecord *casing;
    Mtxf mtx;
    ITEM_IDS weaponid;
    coord3d *switchdata;
    ModelFileHeader *cartridge_header;
    coord3d switchpos;
    ModelNode *switch0;
    coord3d rot;
#if VERSION_EU
    Mtxf rotmtx;
#endif
    f32 rand;
    s32 new_var; /* dead but declared on EU — still reserves its frame slot */
    f32 frac;
#if VERSION_EU
    s32 randlimit;
#endif
    f32 oldvely;
    f32 newvely;
#ifndef VERSION_EU
    s32 randlimit;
#endif
    s32 handoffset;
    u32 randval;
#if VERSION_EU
    s32 pad[2];
#endif
 
    weaponid = getCurrentPlayerWeaponId(handnum);
    cartridge_header = get_ptr_item_statistics(weaponid)->ptr_cartridge_struct;
 
    // Do not create ejected casings in multiplayer.
    if ((cartridge_header == NULL) || (getPlayerCount() >= 2))
    {
        return;
    }
 
    handoffset = handnum * sizeof(struct hand);
    switch0 = g_CurrentPlayer->copy_of_body_obj_header[handnum].Switches[0];
 
    if (switch0 != NULL)
    {
        switchdata = (coord3d *) switch0->Data;
 
        switchpos.x = switchdata->x * D_800543B4;
        switchpos.y = switchdata->y * D_800543B4;
        switchpos.z = switchdata->z * D_800543B4;
 
        matrix_4x4_set_identity_and_position(&switchpos, &mtx);
        matrix_4x4_multiply_in_place(THROWMTX, &mtx);
    }
    else
    {
        matrix_4x4_copy(THROWMTX, &mtx);
    }
 
    casing = casingCreate(cartridge_header, &mtx);
 
    if (casing == NULL)
    {
        return;
    }
 
    rot = *((coord3d *) (&D_80035EA4));
    casing->floor_y_pos = floor_y_pos;
 
    if (((((weaponid == ITEM_WPPK) || (weaponid == ITEM_WPPKSIL)) || (weaponid == ITEM_TT33)) || (weaponid == ITEM_SILVERWPPK)) || (weaponid == ITEM_GOLDWPPK))
    {
        rand = ((f32) ((u32) randomGetNext())) * 2.3283064e-10f;
        newvely = 0.0625f;
        casing->vel.x = -(((rand * D_800543B8) * newvely) + D_800543B8);
 
        rand = ((f32) ((u32) randomGetNext())) * 2.3283064e-10f;
        casing->vel.y = ((rand * 2.5f) * 0.0625f) + 2.5f;
        casing->vel.z = frac * 0.0f;
 
        mtx4RotateVecInPlace(THROWMTX, &casing->vel);
 
        rand = ((f32) ((u32) randomGetNext())) * 2.3283064e-10f;
        rot.x = (((rand + rand) * D_800543BC) * newvely) - D_800543C0;
 
        rand = ((f32) ((u32) randomGetNext())) * 2.3283064e-10f;
        rot.y = (((rand + rand) * D_800543C4) * newvely) - D_800543C8;
 
        rand = ((f32) ((u32) randomGetNext())) * 2.3283064e-10f;
        rot.z = (((rand + rand) * D_800543CC) * newvely) - D_800543D0;
 
#if VERSION_EU
        matrix_4x4_set_rotation_around_xyz(&rot, &rotmtx);
        matrix_7f05842c_eu(&rotmtx, casing->rot_velocity_mtx);
        if (handoffset);
#else
        matrix_4x4_set_rotation_around_xyz(&rot, &casing->rot_velocity_mtx);
#endif
 
#if VERSION_EU
        randlimit = ((s32) ((randomGetNext() >> 24) * 0x158679)) >> 10;
        randlimit = randlimit + 0x158679;
        randval = randomGetNext();
        oldvely = casing->vel.y;
        frac = ((f32) ((u32) (randval % randlimit))) / D_800543D4;
#else
        randlimit = (((s32) ((randomGetNext() >> 24) * 0x158679)) >> 10) + 0x158679;
        new_var = randlimit;
        randval = randomGetNext();
        oldvely = casing->vel.y;
        frac = ((f32) ((u32) (randval % new_var))) / D_800543D4;
#endif
        newvely = oldvely - (frac * expended_shell_initial_gravity_modifier_pistol);
 
        casing->vel.y = newvely;
        casing->pos.y += (frac * (oldvely + newvely)) * 0.5f;
        casing->pos.x += frac * casing->vel.x;
        casing->pos.z += frac * casing->vel.z;
 
        // Keep the 0 + 1 for matching.
        if (g_ClockTimer >= (0 + 1))
        {
            casing->vel.x += (THROWPOS(0) - THROWPREV(0)) / g_GlobalTimerDelta;
            casing->vel.y += (THROWPOS(1) - THROWPREV(1)) / g_GlobalTimerDelta;
            casing->vel.z += (THROWPOS(2) - THROWPREV(2)) / g_GlobalTimerDelta;
        }
    }
    else
    {
        rand = ((f32) ((u32) randomGetNext())) * 2.3283064e-10f;
        casing->vel.x = -(((rand * D_800543DC) * 0.125f) + D_800543DC);
 
        rand = ((f32) ((u32) randomGetNext())) * 2.3283064e-10f;
        casing->vel.y = ((rand * D_800543E0) * 0.125f) + D_800543E0;
        casing->vel.z = 0.0f;
 
        mtx4RotateVecInPlace(THROWMTX, &casing->vel);
 
        rand = ((f32) ((u32) randomGetNext())) * 2.3283064e-10f;
        rot.x = (((rand + rand) * D_800543E4) * 0.0625f) - D_800543E8;
 
        rand = ((f32) ((u32) randomGetNext())) * 2.3283064e-10f;
        rot.y = (((rand + rand) * D_800543EC) * 0.0625f) - D_800543F0;
 
        rand = ((f32) ((u32) randomGetNext())) * 2.3283064e-10f;
        rot.z = (((rand + rand) * D_800543F4) * 0.0625f) - D_800543F8;
 
#if VERSION_EU
        matrix_4x4_set_rotation_around_xyz(&rot, &rotmtx);
        matrix_7f05842c_eu(&rotmtx, casing->rot_velocity_mtx);
        if (handoffset);
#else
        matrix_4x4_set_rotation_around_xyz(&rot, &casing->rot_velocity_mtx);
#endif
        randval = ((s32) ((randomGetNext() >> 24) * 0x158679)) >> 10;
#if VERSION_EU
        randval = randval + 0x158679;
        randlimit = randomGetNext();
        oldvely = (&casing->vel)->y;
        frac = ((f32) ((u32) (randlimit % randval))) / D_800543FC;
#else
        randlimit = randval + 0x158679;
        randval = randomGetNext();
        oldvely = (&casing->vel)->y;
        frac = ((f32) ((u32) (randval % randlimit))) / D_800543FC;
#endif
        newvely = (casing->vel.y = oldvely - (frac * expended_shell_initial_gravity_modifier_non_pistol));
 
        casing->pos.y += (frac * (oldvely + newvely)) * 0.5f;
        casing->pos.x += frac * casing->vel.x;
        casing->pos.z += frac * casing->vel.z;
 
        if (g_ClockTimer > 0)
        {
            casing->vel.x += (THROWPOS(0) - THROWPREV(0)) / g_GlobalTimerDelta;
            casing->vel.y += (THROWPOS(1) - THROWPREV(1)) / g_GlobalTimerDelta;
            casing->vel.z += (THROWPOS(2) - THROWPREV(2)) / g_GlobalTimerDelta;
        }
    }
 
    if (handoffset);
}
#endif


extern f32 g_CasingGravity;
extern f32 g_CasingModelScale;
extern f32 g_CasingMinMatrixTranslation;
extern f32 g_CasingMaxMatrixTranslation;

void update_bullet_casing(CasingRecord* casing)
{
    f32 new_val_y;
    f32 delta;
    s32 i;
    struct player* current_player;

    delta = g_GlobalTimerDelta;
    new_val_y = casing->vel.y - (delta * g_CasingGravity);

    casing->pos.y += delta * 0.5f * (casing->vel.y + new_val_y);

    if (casing->pos.y < casing->floor_y_pos)
    {
#if defined(BUGFIX_R1)
        if (g_CasingSfxState == 0 && (g_ClockTimer > 0))
#else
        if (g_CasingSfxState == 0)
#endif
        {
            if ((g_CurrentPlayer->hands[0].weapon_action_state != GUN_ANIM_STATE_FIRE) && (g_CurrentPlayer->hands[1].weapon_action_state != GUN_ANIM_STATE_FIRE))
            {
                // Play bullet casing rolling on floor sound
                sndPlaySfx((struct ALBankAlt_s* ) g_musicSfxBufferPtr, CART_SPENT_SFX, (ALSoundState* ) &g_CasingSfxState);
            }
        }

        // This casing is removed and not updated anymore
        casing->header = NULL;
        return;
    }

    casing->vel.y = new_val_y;
    casing->pos.x += delta * casing->vel.x;
    casing->pos.z += delta * casing->vel.z;

    for (i = 0; i < g_ClockTimer; i++)
    {
#if defined(VERSION_US) || defined(VERSION_JP)
        matrix_4x4_multiply_homogeneous_in_place(&casing->rot_velocity_mtx, &casing->rot_mtx);
#else
        matrix_4x4_multiply_homogeneous_in_place_eu(casing->rot_velocity_mtx, casing->rot_mtx);
#endif
    }
}


void update_bullet_casings(void)
{
    CasingRecord* end = g_Casings + ARRAYCOUNT(g_Casings);
    CasingRecord* entry = g_Casings;

    while (entry < end)
    {
        if (entry->header)
        {
            update_bullet_casing(entry);
        }

        entry++;
    }
}

typedef struct ModelHead {
    s16 unk00;
    s16 rwdatalen;
    void *chr;
    ModelFileHeader *obj;
    RenderPosView *render_pos;
    union ModelRwData **datas;
    f32 scale;
    Model *attachedto;
    ModelNode *attachedto_objinst;
} ModelHead;

void sub_GAME_7F068EC4(CasingRecord *casing, Gfx **gdl)
{
    Gfx             *savedgdl = *gdl;
    ModelFileHeader *model_header = casing->header;
    RenderPosView   *model_matrices = dynAllocate(model_header->numMatrices * sizeof(RenderPosView));
    ModelHead        model;
    ModelRenderData  render_data = *(ModelRenderData *)g_DefaultCasingModelRenderData;
    Mtxf             casing_model_mtx;
    s32              axis_offset;
    s32              matrix_translation_in_range = TRUE;
    f32              max_matrix_translation;
    f32              model_scale_or_min_translation;
    u8              *matrix_axis_ptr;

    modelCalculateRwDataLen(model_header);
    modelInit((Model *)&model, model_header, NULL);

    model.render_pos = model_matrices;

#if defined(VERSION_EU)
    matrix_4x4_copy_eu(casing->rot_mtx, casing_model_mtx.m);
#else
    matrix_4x4_copy(&casing->rot_mtx, &casing_model_mtx);
#endif

    model_scale_or_min_translation = g_CasingModelScale;
    matrix_scalar_multiply(model_scale_or_min_translation, &casing_model_mtx);

    matrix_4x4_set_position(&casing->pos, &casing_model_mtx);

    matrix_4x4_multiply_homogeneous(
        camGetWorldToScreenMtxf(),
        &casing_model_mtx,
        (Mtxf *)model.render_pos);

    model_scale_or_min_translation = g_CasingMinMatrixTranslation;
    max_matrix_translation         = g_CasingMaxMatrixTranslation;

    axis_offset     = 0;
    matrix_axis_ptr = (u8 *)model.render_pos;

    // Offset 0x30 is m[3][0]; advancing the pointer checks translation X, Y and Z.
    while (axis_offset != 12)
    {
        if (max_matrix_translation < *(f32 *)(matrix_axis_ptr + 0x30))
        {
            matrix_translation_in_range = FALSE;
        }
        else if (*(f32 *)(matrix_axis_ptr + 0x30) < model_scale_or_min_translation)
        {
            matrix_translation_in_range = FALSE;
        }

        axis_offset += 4;
        matrix_axis_ptr += 4;
    }

    if (matrix_translation_in_range)
    {
        render_data.zbufferenabled = 0;
        render_data.gdl            = savedgdl;
        render_data.mtxlist        = (Mtxf *)model_matrices;
        render_data.PropType       = PROP_TYPE_WEAPON;

        render_data.envcolour.word =
            ((g_CurrentPlayer->tileColor.a |
              (g_CurrentPlayer->tileColor.r << 24)) |
             (g_CurrentPlayer->tileColor.g << 16)) |
            (g_CurrentPlayer->tileColor.b << 8);

        subdraw(&render_data, (Model *)&model);

        *gdl = render_data.gdl;

        bondviewTransformManyPosToViewMatrix(model_matrices, model_header->numMatrices);
    }
}


// Address: 0x7F06908C
void gunRenderCasings(Gfx **gdl)
{
    CasingRecord* end = g_Casings + ARRAYCOUNT(g_Casings);
    CasingRecord* entry = g_Casings;

    while (entry < end)
    {
        if (entry->header)
        {
            sub_GAME_7F068EC4(entry, gdl);
        }
        
        entry++;
    }
}


void gunSetGunAmmoVisible(s32 reason, bool enable) {

	if (enable)
    {
		g_CurrentPlayer->gunammooff &= ~reason;
		return;
	}

	g_CurrentPlayer->gunammooff |= reason;
}



void give_cur_player_ammo(s32 ammo_type, s32 ammo_amount) {
    enum ITEM_IDS weapon_id;
    s32 max_ammo;

    weapon_id = getCurrentPlayerWeaponId(GUNRIGHT);
    if ((get_ammo_type_for_weapon(weapon_id) == ammo_type) && (bondwalkItemCheckBitflags(weapon_id, WEAPONSTATBITFLAG_AMMO_CLIP_LIMIT) != 0))
    {
        g_CurrentPlayer->hands[0].weapon_ammo_in_magazine += ammo_amount;
        if (get_ptr_item_statistics(weapon_id)->MagSize < g_CurrentPlayer->hands[0].weapon_ammo_in_magazine)
        {
            g_CurrentPlayer->hands[0].weapon_ammo_in_magazine = (s32) get_ptr_item_statistics(weapon_id)->MagSize;
        }
        g_CurrentPlayer->ammoheldarr[ammo_type] = 0;
        return;
    }

    max_ammo = ammo_related[ammo_type].MaxAmmo;
    if (max_ammo < ammo_amount)
    {
        g_CurrentPlayer->ammoheldarr[ammo_type] = max_ammo;
        return;
    }

    g_CurrentPlayer->ammoheldarr[ammo_type] = ammo_amount;
}




s32 check_cur_player_ammo_amount_in_inventory(AMMOTYPE ammotype) {
    return g_CurrentPlayer->ammoheldarr[ammotype];
}

s32 currentPlayerGetAmmoCount(AMMOTYPE ammotype) {

    s32 total_ammo = check_cur_player_ammo_amount_in_inventory(ammotype);

    if (get_ammo_type_for_weapon(getCurrentPlayerWeaponId(GUNRIGHT)) == ammotype) {
        total_ammo += get_ammo_in_hands_magazine(GUNRIGHT);
    }

    if (get_ammo_type_for_weapon(getCurrentPlayerWeaponId(GUNLEFT)) == ammotype) {
        total_ammo += get_ammo_in_hands_magazine(GUNLEFT);
    }

    return total_ammo;
}



s32 get_max_ammo_for_type(s32 arg0)
{
    return ammo_related[arg0].MaxAmmo;
}




void set_max_ammo_for_cur_player(void)
{
    s32 ammo_type;

    for (ammo_type = 0; ammo_type < AMMO_RELATED_MAX; ammo_type++)
    {
        give_cur_player_ammo(ammo_type, ammo_related[ammo_type].MaxAmmo);
    }
}



s32 get_ammo_in_hands_magazine(GUNHAND hand) {
    return g_CurrentPlayer->hands[hand].weapon_ammo_in_magazine;
}



s32 get_ammo_in_hands_weapon(enum GUNHAND hand)
{
    s32 weapon_id;
    s32 ammo_count;

    weapon_id = getCurrentPlayerWeaponId(hand);
    ammo_count = get_ammo_count_for_weapon(weapon_id);

    if ((weapon_id == ITEM_SHOTGUN) || (weapon_id == ITEM_AUTOSHOT))
    {
        s32 other_weapon_id;
        other_weapon_id = getCurrentPlayerWeaponId(1 - hand);

        if ((other_weapon_id == ITEM_SHOTGUN) || (other_weapon_id == ITEM_AUTOSHOT))
        {
            return ammo_count - g_CurrentPlayer->hands[1 - hand].numvisibleshells;
        }

        /* I don't know why there's an extra return here, but it's needed to match */
        return ammo_count;
    }

    return ammo_count;
}



s32 get_ammo_type_for_weapon(ITEM_IDS weapon) {
    return get_ptr_item_statistics(weapon)->AmmoType;
}

s32 get_ammo_count_for_weapon(ITEM_IDS weapon) {
  WeaponStats *weaponstats = get_ptr_item_statistics(weapon);
  return g_CurrentPlayer->ammoheldarr[weaponstats->AmmoType];
}

void add_ammo_to_weapon(ITEM_IDS weapon, s32 ammo) {
    give_cur_player_ammo(get_ptr_item_statistics(weapon)->AmmoType, ammo);
}

s32 get_max_ammo_for_weapon(enum ITEM_IDS weapon)
{
    return ammo_related[get_ptr_item_statistics(weapon)->AmmoType].MaxAmmo;
}


Gfx *microcode_generation_ammo_related(Gfx *gdl, struct sImageTableEntry *tconfig, f32 x, f32 y, f32 arg4, s32 arg5, f32 arg6, s32 arg7, s32 red, s32 green, s32 blue, s32 alpha)
{
    f32 xy[2];
    f32 halfed[2];
 
    gDPSetColorDither(gdl++, G_CD_DISABLE);
    gDPSetTexturePersp(gdl++, G_TP_NONE);
    gDPSetAlphaCompare(gdl++, G_AC_NONE);
    gDPSetTextureLOD(gdl++, G_TL_TILE);
    gDPSetTextureFilter(gdl++, G_TF_POINT);
    gDPSetTextureConvert(gdl++, G_TC_FILT);
    gDPSetTextureLUT(gdl++, G_TT_NONE);
 
    xy[0] = ((u32)tconfig->width * 0.5f) - (f32)(tconfig->width / 2);

    if (arg5 != 0) 
    { 
        xy[0] = -xy[0]; 
    }

    xy[0] = xy[0] + x;
 
    if (0.0f <= y)
    {
        xy[1] = y - (((f32) ((u32) tconfig->height)) * 0.5f);
    }
    else
    {
        xy[1] = -((((f32)((u32)tconfig->height)) * 0.5f) - ((f32)(tconfig->height / 2)));
        xy[1] = arg4 + xy[1];
        xy[1] = xy[1] + arg6;
    }
 
    halfed[0] = ((f32) ((u32) tconfig->width)) * 0.5f;
    halfed[1] = ((f32) ((u32) tconfig->height)) * 0.5f;
 
    gDPPipeSync(gdl++);
    gDPSetCycleType(gdl++, G_CYC_1CYCLE);
    gDPSetRenderMode(gdl++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
    gDPSetCombineMode(gdl++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
    gDPSetPrimColor(gdl++, 0, 0, 0, 0, 0, 0);
    gDPFillRectangle(gdl++, ((s32)(xy[0] - halfed[0])) - 1, ((s32)(xy[1] - halfed[1])) - 1, ((s32)(xy[0] + halfed[0])) + 1, ((s32)(xy[1] + halfed[1])) + 1);
 
    texSelect(&gdl, tconfig, (arg7 != 0) ? (2) : (1), 0, 0);
    display_image_at_position(&gdl, xy, halfed, tconfig->width, tconfig->height, 0, 0, 1, red, green, blue, alpha, 0 < tconfig->level, 0);
 
    gDPPipeSync(gdl++);
    gDPSetColorDither(gdl++, G_CD_BAYER);
    gDPSetTexturePersp(gdl++, G_TP_PERSP);
    gDPSetAlphaCompare(gdl++, G_AC_NONE);
    gDPSetTextureLOD(gdl++, G_TL_LOD);
    gDPSetTextureFilter(gdl++, G_TF_BILERP);
    gDPSetTextureConvert(gdl++, G_TC_FILT);
    gDPSetTextureLUT(gdl++, G_TT_NONE);
 
    return gdl;
}


/**
 * Address: TODO
 * WARNING: This function is missing a "return". This will cause bugs on other compilers.
 */
Gfx *set_rgba_redirect_generate_microcode(Gfx *gdl, sImageTableEntry *tconfig, f32 x, f32 y, f32 arg4, s32 arg5, f32 arg6, s32 arg7)
{
    microcode_generation_ammo_related(gdl, tconfig, x, y, arg4, arg5, arg6, arg7, 0xff, 0xff, 0xff, 0xff);
}


/**
 * Address: TODO
 */
Gfx *gunDrawHudString(Gfx *gdl, s8 *text, s32 x, s32 halign, s32 y, s32 valign, bool outline)
{
    s32 x1;
    s32 y1;
    s32 x2;
    s32 y2;
    s32 textheight;
    s32 textwidth;

    x1 = 0;
    y1 = 0;
    x2 = 0;
    y2 = 0;
    textwidth = 0;
    textheight = 0;

    textMeasure(&textheight, &textwidth, text, ptrFontBankGothicChars, ptrFontBankGothic, 0);

    if (halign == HUDHALIGN_LEFT) { // left
		x2 = x + textwidth;
		x1 = x;
	} else if (halign == HUDHALIGN_RIGHT) { // right
		x1 = x - textwidth;
		x2 = x;
	} else if (halign == HUDHALIGN_MIDDLE) { // middle
		x2 = x + textwidth / 2;
		x1 = x2 - textwidth;
	}

    if (valign == HUDVALIGN_TOP) { // top
		y2 = y + textheight;
		y1 = y;
	} else if (valign == HUDVALIGN_BOTTOM) { // bottom
		y1 = y - textheight;
		y2 = y;
	} else if (valign == HUDVALIGN_MIDDLE) { // middle
		y2 = y + textheight / 2;
		y1 = y2 - textheight;
	}

    gdl = draw_blackbox_to_screen(gdl, &x1, &y1, &x2, &y2);

    if (outline) {
        gdl = textRenderOutlined(gdl, &x1, &y1, text, ptrFontBankGothicChars, ptrFontBankGothic, -1, 0x646464FF, (s32) viGetX(), viGetY(), 0, 0);
    } else {
        gdl = textRender(gdl, &x1, &y1, text, ptrFontBankGothicChars, ptrFontBankGothic, 0xFF00B0, (s32) viGetX(), viGetY(), 0, 0);
    }

    return gdl;
}


/**
 * Address: TODO
 */
Gfx *gunDrawHudInteger(Gfx *gdl, s32 value, s32 x, s32 halign, s32 y, s32 valign, bool outline)
{
    char buffer[12];
    sprintf(buffer, g_GunHudIntegerFormat, value);
    return gunDrawHudString(gdl, buffer, x, halign, y, valign, outline);
}


/**
 * Draw magazine ammo number, ammo type icon, and total ammo number at the bottom right of the viewport.
 * Render an additional ammo counter at the bottom left of the viewport when dual wielding.
 */
Gfx *generate_ammo_total_microcode(Gfx *gdl)
{
    ITEM_IDS weapon_left;
    ITEM_IDS weapon_right;
    s32 ammotype;
    s32 leftx;
    s32 rightx;
    s32 reserveammo;
    s32 magammo;
    u32 imageoffset_r;
    s32 textwidth_r;
    u32 imageoffset_l;
    s32 textwidth_l;

    if (g_CurrentPlayer->gunammooff == 0)
    {
        if (g_CurrentPlayer->mpmenuon == 0)
        {
            weapon_left = getCurrentPlayerWeaponId(GUNLEFT);
            weapon_right = getCurrentPlayerWeaponId(GUNRIGHT);

            if (getPlayerCount() < 3)
            {
                leftx = 59;
                rightx = 59;
            }
            else if (get_cur_playernum() & 1)
            {
                leftx = 43;
                rightx = 127;
            }
            else
            {
                leftx = 59;
                rightx = 109;
            }

            if (weapon_right != ITEM_UNARMED)
            {
                ammotype = get_ammo_type_for_weapon(weapon_right);

                if (ammotype != 0
                    && g_CurrentPlayer->hands[0].weapon_action_state != GUN_ANIM_STATE_SWITCH_SWAP
                    && g_CurrentPlayer->hands[0].weapon_action_state != GUN_ANIM_STATE_SWITCH_HOLD
                    && !bondwalkItemCheckBitflags(weapon_right, WEAPONSTATBITFLAG_HIDE_AMMO_DISPLAY))
                {
                    imageoffset_r = ammo_related[ammotype].IconImage;
                    textwidth_r = 5;

                    if (imageoffset_r != 0)
                    {
                        imageoffset_r += globalbank_rdram_offset;
                        gdl = set_rgba_redirect_generate_microcode(gdl, (u8 *)imageoffset_r, (getPlayer_c_screenleft() + getPlayer_c_screenwidth()) - (f32)rightx, -1.0f,
#if defined(VERSION_EU)
                            (viGetViewTop() + viGetViewHeight()) - 30, 0,
#else
                            (viGetViewTop() + viGetViewHeight()) - 20, 0,
#endif
                            ammo_related[ammotype].IconYOffset, 1);
                        textwidth_r = ((u8 *)imageoffset_r)[4];
                    }

                    gdl = microcode_constructor(gdl);

                    if (bondwalkItemCheckBitflags(weapon_right, WEAPONSTATBITFLAG_NO_CLIP_RELOADS))
                    {
                        magammo = 0;
                        reserveammo = g_CurrentPlayer->ammoheldarr[ammotype] + g_CurrentPlayer->hands[0].weapon_ammo_in_magazine;
                        if (weapon_left == weapon_right)
                        {
                            reserveammo += g_CurrentPlayer->hands[1].weapon_ammo_in_magazine;
                        }
                    }
                    else
                    {
                        magammo = g_CurrentPlayer->hands[0].weapon_ammo_in_magazine;
                        reserveammo = g_CurrentPlayer->ammoheldarr[ammotype];
                    }

                    if (!bondwalkItemCheckBitflags(weapon_right, WEAPONSTATBITFLAG_NO_CLIP_RELOADS))
                    {
                        gdl = gunDrawHudInteger(gdl, magammo, (((viGetViewLeft() + viGetViewWidth()) - rightx) - (textwidth_r / 2)) - 4, 0,
#if defined(VERSION_EU)
                            (viGetViewTop() + viGetViewHeight()) - 28, 2, 1);
#else
                            (viGetViewTop() + viGetViewHeight()) - 18, 2, 1);
#endif
                    }

                    if (reserveammo > 0 || bondwalkItemCheckBitflags(weapon_right, WEAPONSTATBITFLAG_NO_CLIP_RELOADS))
                    {
                        gdl = gunDrawHudInteger(gdl, reserveammo, (((viGetViewLeft() + viGetViewWidth()) - rightx) + ((textwidth_r + 1) / 2)) + 3, 1,
#if defined(VERSION_EU)
                            (viGetViewTop() + viGetViewHeight()) - 28, 2, 1);
#else
                            (viGetViewTop() + viGetViewHeight()) - 18, 2, 1);
#endif
                    }

                    gdl = combiner_bayer_lod_perspective(gdl);
                }
            }

            if (weapon_left != ITEM_UNARMED)
            {
                ammotype = get_ammo_type_for_weapon(weapon_left);

                if (ammotype != 0
                    && g_CurrentPlayer->hands[1].weapon_action_state != GUN_ANIM_STATE_SWITCH_SWAP
                    && g_CurrentPlayer->hands[1].weapon_action_state != GUN_ANIM_STATE_SWITCH_HOLD
                    && !bondwalkItemCheckBitflags(weapon_left, WEAPONSTATBITFLAG_HIDE_AMMO_DISPLAY))
                {
                    imageoffset_l = ammo_related[ammotype].IconImage;
                    textwidth_l = 5;

                    if (imageoffset_l != 0)
                    {
                        imageoffset_l += globalbank_rdram_offset;
                        gdl = set_rgba_redirect_generate_microcode(gdl, (u8 *)imageoffset_l, getPlayer_c_screenleft() + (f32)leftx, -1.0f,
#if defined(VERSION_EU)
                            (viGetViewTop() + viGetViewHeight()) - 30, 1,
#else
                            (viGetViewTop() + viGetViewHeight()) - 20, 1,
#endif
                            ammo_related[ammotype].IconYOffset, 1);
                        textwidth_l = ((u8 *)imageoffset_l)[4];
                    }

                    gdl = microcode_constructor(gdl);

                    if (bondwalkItemCheckBitflags(weapon_left, WEAPONSTATBITFLAG_NO_CLIP_RELOADS))
                    {
                        magammo = 0;
                        reserveammo = g_CurrentPlayer->ammoheldarr[ammotype] + g_CurrentPlayer->hands[1].weapon_ammo_in_magazine;
                        if (weapon_left == weapon_right)
                        {
                            reserveammo += g_CurrentPlayer->hands[0].weapon_ammo_in_magazine;
                        }
                    }
                    else
                    {
                        magammo = g_CurrentPlayer->hands[1].weapon_ammo_in_magazine;
                        reserveammo = g_CurrentPlayer->ammoheldarr[ammotype];
                    }

                    if (!bondwalkItemCheckBitflags(weapon_left, WEAPONSTATBITFLAG_NO_CLIP_RELOADS))
                    {
                        gdl = gunDrawHudInteger(gdl, magammo, ((viGetViewLeft() + leftx) + (textwidth_l / 2)) + 3, 1,
#if defined(VERSION_EU)
                            (viGetViewTop() + viGetViewHeight()) - 28, 2, 1);
#else
                            (viGetViewTop() + viGetViewHeight()) - 18, 2, 1);
#endif
                    }

                    if (reserveammo > 0 || bondwalkItemCheckBitflags(weapon_left, WEAPONSTATBITFLAG_NO_CLIP_RELOADS))
                    {
                        gdl = gunDrawHudInteger(gdl, reserveammo, ((viGetViewLeft() + leftx) - ((textwidth_l + 1) / 2)) - 4, 0,
#if defined(VERSION_EU)
                            (viGetViewTop() + viGetViewHeight()) - 28, 2, 1);
#else
                            (viGetViewTop() + viGetViewHeight()) - 18, 2, 1);
#endif
                    }

                    gdl = combiner_bayer_lod_perspective(gdl);
                }
            }
        }
    }

    return gdl;
}


/**
 * Address: 7F06A334
 */
Gfx *gunDrawWatchAmmoDisplay(Gfx *gdl)
{
    ITEM_IDS offhanditem;
    ITEM_IDS item;
    s32 ammotype;
    s32 reserveammo;
    s32 magammo;
    u32 imageoffset;
    s32 textwidth;
    s32 pad;

    offhanditem = getCurrentPlayerWeaponId(1);
    item = getCurrentPlayerWeaponId(0);

    if (item != ITEM_UNARMED)
    {
        ammotype = get_ammo_type_for_weapon(item);

        if (ammotype != 0
            && g_CurrentPlayer->hands[GUNRIGHT].weapon_action_state != GUN_ANIM_STATE_SWITCH_SWAP
            && g_CurrentPlayer->hands[GUNRIGHT].weapon_action_state != GUN_ANIM_STATE_SWITCH_HOLD
            && !bondwalkItemCheckBitflags(item, WEAPONSTATBITFLAG_HIDE_AMMO_DISPLAY))
        {
            imageoffset = ammo_related[ammotype].IconImage;
            textwidth = 5;

            get_ptr_item_statistics(item);

            if (imageoffset != 0)
            {
                imageoffset += globalbank_rdram_offset;

                // Draw the ammo icon
#if defined(VERSION_EU)
                gdl = set_rgba_redirect_generate_microcode(gdl, (u8 *)imageoffset, 200.0f, 208.0f, (viGetViewTop() + viGetViewHeight()) - 30, 0, ammo_related[ammotype].IconYOffset, 1);
#else
                gdl = set_rgba_redirect_generate_microcode(gdl, (u8 *)imageoffset, 200.0f, 180.0f, (viGetViewTop() + viGetViewHeight()) - 20, 0, ammo_related[ammotype].IconYOffset, 1);
#endif

                textwidth = ((u8 *)imageoffset)[4];
            }

            gdl = microcode_constructor(gdl);

            if (bondwalkItemCheckBitflags(item, WEAPONSTATBITFLAG_NO_CLIP_RELOADS))
            {
                magammo = 0;
                reserveammo = g_CurrentPlayer->ammoheldarr[ammotype] + g_CurrentPlayer->hands[0].weapon_ammo_in_magazine;

                if (offhanditem == item)
                {
                    reserveammo += g_CurrentPlayer->hands[1].weapon_ammo_in_magazine;
                }
            }
            else
            {
                magammo = g_CurrentPlayer->hands[0].weapon_ammo_in_magazine;
                reserveammo = g_CurrentPlayer->ammoheldarr[ammotype];
            }

            if (!bondwalkItemCheckBitflags(item, WEAPONSTATBITFLAG_NO_CLIP_RELOADS))
            {
                // Draw the magazine ammo count.
#if defined(VERSION_EU)
                gdl = gunDrawHudInteger(gdl, magammo, 196 - (textwidth / 2), 0, 205, 2, 0);
#else
                gdl = gunDrawHudInteger(gdl, magammo, 196 - (textwidth / 2), 0, 177, 2, 0);
#endif
            }

            if (reserveammo > 0 || bondwalkItemCheckBitflags(item, WEAPONSTATBITFLAG_NO_CLIP_RELOADS))
            {
                // Draw the reserve ammo count.
#if defined(VERSION_EU)
                gdl = gunDrawHudInteger(gdl, reserveammo, 203 + ((textwidth + 1) / 2), 1, 205, 2, 0);
#else
                gdl = gunDrawHudInteger(gdl, reserveammo, 203 + ((textwidth + 1) / 2), 1, 177, 2, 0);
#endif
            }

            gdl = combiner_bayer_lod_perspective(gdl);
        }
    }

    return gdl;
}


void gunSetSightVisible(s32 reason, bool visible)
{
    if (visible)
    {
        g_CurrentPlayer->gunsightmode &= ~reason;
        return;
    }

    g_CurrentPlayer->gunsightmode |= reason;
}


void gunDrawSight(s32 *gdl) {

    s32 sp54;
    f32 xypos[2];
    f32 halfedxy[2];

    if ((g_CurrentPlayer->gunsightmode == 0) && (g_CurrentPlayer->mpmenuon == FALSE)) {
        sp54 = *gdl;
        texSelect(&sp54, crosshairimage, 4, 0, 0);

        xypos[0] = g_CurrentPlayer->crosshair_angle.f[0];
        xypos[1] = g_CurrentPlayer->crosshair_angle.f[1];
        halfedxy[0] = 16.0f;
        halfedxy[1] = 16.0f;

        if (get_screen_ratio() == SCREEN_RATIO_16_9) {
            halfedxy[0] = halfedxy[0] * 0.75f;
        }
#ifdef VERSION_EU
        halfedxy[1] = halfedxy[1] * g_GunSightAspectRatio;
#endif
        display_image_at_position(&sp54, &xypos, &halfedxy, 0x20, 0x20, 0, 0, 1, 0xFF, 0xFF, 0xFF, 0x6E, (crosshairimage->level > 0), 0);
        *gdl = sp54;
    }
}


void inc_curplayer_hitcount_with_weapon(ITEM_IDS item, SHOT_REGISTER shot_register) {

    if (bondwalkItemCheckBitflags(item, WEAPONSTATBITFLAG_PLAYER_STAT_HIT)) {
        g_playerPerm->shot_count[shot_register] = g_playerPerm->shot_count[shot_register]+1;
    }
}


s32 get_curplayer_shot_register(SHOT_REGISTER shot_register)
{
  return g_playerPerm->shot_count[shot_register];
}


void inc_cur_civilian_casualties(void)
{
    g_playerPerm->killed_civilians++;
}


s32 get_civilian_casualties(void)
{
    return g_playerPerm->killed_civilians;
}


void increment_num_kills_display_text_in_MP(void)
{
    s8 buffer[256];
    s32 time_since_kill;
    s32 recent_kill_count;
    s32 mission_time;
    s32 unused; // needed this variable to match

    g_playerPerm->kill_count += 1;
    g_CurrentPlayer->kills_this_life += 1;

    if (getPlayerCount() < 2) { return; }

    mission_time = getMissiontimer();
    sprintf(&buffer, aSD, langGet(getStringID(LGUN, GUN_STR_DA_KILLCOUNT)), g_playerPerm->kill_count); // "kill count"

#if defined(VERSION_US)
    hudmsgBottomShow(&buffer);
#elif defined(VERSION_JP) || defined(VERSION_EU)
    jp_hudmsgBottomShow(&buffer);
#endif

    if (g_playerPerm->kill_count >= 2)
    {
        time_since_kill = mission_time - g_CurrentPlayer->last_kill_time[0];
        if (g_playerPerm->max_time_between_kills < time_since_kill)
        {
            g_playerPerm->max_time_between_kills = time_since_kill;
        }

        if (time_since_kill < g_playerPerm->min_time_between_kills)
        {
            g_playerPerm->min_time_between_kills = time_since_kill;
        }
    }

    recent_kill_count = 1;
    g_CurrentPlayer->last_kill_time[3] = g_CurrentPlayer->last_kill_time[2];
    g_CurrentPlayer->last_kill_time[2] = g_CurrentPlayer->last_kill_time[1];
    g_CurrentPlayer->last_kill_time[1] = g_CurrentPlayer->last_kill_time[0];
    g_CurrentPlayer->last_kill_time[0] = mission_time;

    // I tried to turn this into a loop but it didn't match
    if (g_CurrentPlayer->last_kill_time[1] != -1 && (g_CurrentPlayer->last_kill_time[0] - g_CurrentPlayer->last_kill_time[1]) < 0x78)
    {
        recent_kill_count++;
        if ((g_CurrentPlayer->last_kill_time[2] != -1) && ((g_CurrentPlayer->last_kill_time[0] - g_CurrentPlayer->last_kill_time[2]) < 0x78))
        {
            recent_kill_count++;
            if ((g_CurrentPlayer->last_kill_time[3] != -1) && ((g_CurrentPlayer->last_kill_time[0] - g_CurrentPlayer->last_kill_time[3]) < 0x78))
            {
                recent_kill_count++;
            }
        }
    }

    if (g_playerPerm->most_killed_one_time < recent_kill_count)
    {
        g_playerPerm->most_killed_one_time = recent_kill_count;
    }
}



s32 get_curplay_killcount(void) {
    return g_playerPerm->kill_count;
}

void increment_num_times_killed_MwtGC(void){
    g_playerPerm->killed_gg_owner_count++;
}

s32 get_times_killed_mwtgx(void) {
    return g_playerPerm->killed_gg_owner_count;
}


void increment_num_deaths(void)
{
	char buffer[256];
    g_CurrentPlayer->deathcount = (s32) (g_CurrentPlayer->deathcount + 1);
    if (getPlayerCount() >= 2)
    {
        if (g_CurrentPlayer->deathcount == 1)
        {
            sprintf(buffer, langGet(getStringID(LGUN, GUN_STR_DB_DIEDONCE_LF))); //died once
        }
        else
        {
            sprintf(buffer, g_GunDeathCountFormat, langGet(getStringID(LGUN, GUN_STR_DC_DIED)), g_CurrentPlayer->deathcount, langGet(getStringID(LGUN, GUN_STR_DD_TIMES))); //died times
        }
#if defined(VERSION_JP) || defined(VERSION_EU)
		jp_hudmsgBottomShow(buffer);
#else
		hudmsgBottomShow(buffer);
#endif
    }
}


s32 get_curplayer_numdeaths(void) {
    return g_CurrentPlayer->deathcount;
}

void increment_num_suicides_display_MP(void) {
    char buffer[256];
    s32 time_diff;
    s32 recent_kill_count;
    s32 currentTime;

    g_CurrentPlayer->num_suicides += 1;
    if (getPlayerCount() >= 2) {

        currentTime = getMissiontimer();

        sprintf(&buffer, &aSD_0, langGet(getStringID(LGUN, GUN_STR_DE_SUICIDECOUNT)), g_CurrentPlayer->num_suicides); // "suicide count"

#if defined(VERSION_JP) || defined(VERSION_EU)
		jp_hudmsgBottomShow(&buffer);
#else
		hudmsgBottomShow(&buffer);
#endif

        if (g_playerPerm->kill_count >= 2) {
            time_diff = currentTime - g_CurrentPlayer->last_kill_time[0];
            if (g_playerPerm->max_time_between_kills < time_diff) {
                g_playerPerm->max_time_between_kills = time_diff;
            }
            if (time_diff < g_playerPerm->min_time_between_kills) {
                g_playerPerm->min_time_between_kills = time_diff;
            }
        }
        recent_kill_count = 1;
        g_CurrentPlayer->last_kill_time[3] = g_CurrentPlayer->last_kill_time[2];
        g_CurrentPlayer->last_kill_time[2] = g_CurrentPlayer->last_kill_time[1];
        g_CurrentPlayer->last_kill_time[1] = g_CurrentPlayer->last_kill_time[0];
        g_CurrentPlayer->last_kill_time[0] = currentTime;

        if ( g_CurrentPlayer->last_kill_time[1] != -1) {

            if ((g_CurrentPlayer->last_kill_time[0] - g_CurrentPlayer->last_kill_time[1]) < 0x78) {

                recent_kill_count += 1;

                if ((g_CurrentPlayer->last_kill_time[2] != -1) && ((g_CurrentPlayer->last_kill_time[0] - g_CurrentPlayer->last_kill_time[2]) < 0x78)) {

                    recent_kill_count += 1;

                    if ((g_CurrentPlayer->last_kill_time[3] != -1) && ((g_CurrentPlayer->last_kill_time[0] - g_CurrentPlayer->last_kill_time[3]) < 0x78)) {
                        recent_kill_count += 1;
                    }
                }
            }
        }

        if (g_playerPerm->most_killed_one_time < recent_kill_count) {
            g_playerPerm->most_killed_one_time = recent_kill_count;
        }
    }
}

s32 get_curplayer_numsuicides(void) {
    return g_CurrentPlayer->num_suicides;
}
