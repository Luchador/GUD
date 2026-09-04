#include <ultra64.h>
#include <memp.h>
#include "game/mp_weapon.h"
#include "game/bondview_r.h"
#include "bg.h"
#include "bondview_r.h"
#include "chr.h"
#include "chrai.h"
#include "chraction.h"
#include "inititemslots.h"
#include "initobjects.h"
#include "initpathtablesomething.h"
#include "language.h"
#include "limits.h"
#include "loadobjectmodel.h"
#include "lv.h"
#include "math_atan2f.h"
#include "matrixmath.h"
#include "model.h"
#include "mp_weapon.h"
#include "ob.h"
#include "objective.h"
#include "objective_status.h"
#include "objecthandler.h"
#include "player.h"
#include "propobj.h"
#include "setup.h"
#include "stan.h"


/**
 * Temporary state used while constructing mp weapons pickups. The setup format encodes mp weapon locations as placeholder weapon IDs 0xF0-0xF7 in weaponAssignToHome.
 * When the setup encounters the associated PROPDEF_AMMO record it uses g_MpSetupWeaponSlot to select the weapon set entry and configure the crate's ammo type and quantity.
 */
s32 g_MpSetupWeaponSlot = -1;

f32 g_DoorScale = 1.0f;

// redeclare with the element count so ARRAYCOUNT works in setupLoadFiles
extern ItemModelFileRecord PitemZ_entries[341];

// Begin forward declarations.

void padGetCenter(struct BoundPadRecord *pad, struct coord3d *centerPoint);
void weaponAssignToHome(s32 arg0, WeaponObjRecord* weapon, s32 cmdindex);
void setupHat(s32 arg0, ObjectRecord* hat, s32 cmdindex);
void setupKey(s32 arg0, ObjectRecord* key, s32 cmdindex);
void setupCctv(s32 arg0, CCTVRecord *arg1, s32 cmdindex);
void setupAutogun(s32 stageID, AutogunRecord *autogun, s32 cmdindex);
void setupHangingMonitors(s32 arg0, ObjectRecord* rack, s32 cmdindex);
void setupSingleMonitor(s32 stageID, MonitorObjRecord *monitor, s32 cmdindex);
void setupMultiMonitor(s32 stageID, MultiMonitorObjRecord* monitor, s32 cmdindex);
void setupGetDoorAdjacentRooms(struct BoundPadRecord *pad, s32 *frontRoom, s32 *backRoom, struct coord3d *frontProbePos, struct coord3d *backProbePos);
void setupDoor(struct DoorRecord *door, s32 cmdindex);

// End forward declarations.


struct SetupSlotCounts
{
    s32 guards;
    s32 modelSlots;
    s32 animatedModelSlots;
    s32 itemLinks;
};


static void setupCountRequiredSlots(struct SetupSlotCounts *counts)
{
    PropDefHeaderRecord *command = g_CurrentSetup.propDefs;

    counts->guards = 0;
    counts->modelSlots = 0;
    counts->animatedModelSlots = 0;
    counts->itemLinks = 0;

    while ((command != NULL) && (command->type != PROPDEF_END))
    {
        switch (command->type)
        {
            case PROPDEF_GUARD:
                counts->guards++;
                counts->animatedModelSlots++;
                break;
            case PROPDEF_AIRCRAFT:
                counts->animatedModelSlots++;
                break;
            case PROPDEF_LINK:
                counts->itemLinks++;
                break;
            case PROPDEF_COLLECTABLE:
            case PROPDEF_KEY:
            case PROPDEF_HAT:
            case PROPDEF_DOOR:
            case PROPDEF_CCTV:
            case PROPDEF_AUTOGUN:
            case PROPDEF_RACK:
            case PROPDEF_MONITOR:
            case PROPDEF_MULTI_MONITOR:
            case PROPDEF_ARMOUR:
            case PROPDEF_PROP:
            case PROPDEF_GLASS:
            case PROPDEF_TINTED_GLASS:
            case PROPDEF_SAFE:
            case PROPDEF_UNK41:
            case PROPDEF_GAS_RELEASING:
            case PROPDEF_ALARM:
            case PROPDEF_MAGAZINE:
            case PROPDEF_AMMO:
            case PROPDEF_VEHICLE:
            case PROPDEF_TANK:
                counts->modelSlots++;
                break;
        }

        command = &command[sizepropdef(command)];
    }
}


static bool setupShouldLoadObject(const ObjectRecord *object, u32 exclusionFlags)
{
    return (object->flags2 & exclusionFlags) == 0;
}


void padGetCenter(struct BoundPadRecord *pad, struct coord3d *centerPoint)
{
    struct coord3d normal;
    f32 scale;
    struct bbox bb;
    f32 temp;

    bb.zmax = pad->bbox.xmin;
    bb.zmin = pad->bbox.xmax;
    bb.ymax = pad->bbox.ymin;
    bb.ymin = pad->bbox.ymax;
    bb.xmax = pad->bbox.zmin;
    bb.xmin = pad->bbox.zmax;

    normal.f[0] = (pad->up.f[1] * pad->look.f[2]) - (pad->up.f[2] * pad->look.f[1]);
    normal.f[1] = (pad->up.f[2] * pad->look.f[0]) - (pad->up.f[0] * pad->look.f[2]);
    normal.f[2] = (pad->up.f[0] * pad->look.f[1]) - (pad->up.f[1] * pad->look.f[0]);

    temp = (normal.f[0] * normal.f[0]) + (normal.f[1] * normal.f[1]) + (normal.f[2] * normal.f[2]);
    scale = 1.0f / sqrtf(temp);

    normal.f[0] *= scale;
    normal.f[1] *= scale;
    normal.f[2] *= scale;

    centerPoint->f[0] = pad->pos.f[0] + (
			(bb.zmax + bb.zmin) * normal.f[0] +
			(bb.ymax + bb.ymin) * pad->up.f[0] +
			(bb.xmax + bb.xmin) * pad->look.f[0]) * 0.5f;

	centerPoint->f[1] = pad->pos.f[1] + (
			(bb.zmax + bb.zmin) * normal.f[1] +
			(bb.ymax + bb.ymin) * pad->up.f[1] +
			(bb.xmax + bb.xmin) * pad->look.f[1]) * 0.5f;

	centerPoint->f[2] = pad->pos.f[2] + (
			(bb.zmax + bb.zmin) * normal.f[2] +
			(bb.ymax + bb.ymin) * pad->up.f[2] +
			(bb.xmax + bb.xmin) * pad->look.f[2]) * 0.5f;

}


void domakedefaultobj(s32 arg0, ObjectRecord *objectRecord, s32 cmdindex)
{
    s32 modelID;
    f32 var_f0;
    struct coord3d spE0;
    struct StandTile *spDC;
    struct coord3d spD0;
    StandTile *spCC;
    Mtxf sp8C;
    struct coord3d sp80;
    struct PropRecord *var_v0;
    f32 sp78;
    s32 enableRespawn;
    struct BoundPadRecord *boundPadRecord;
    ChrRecord *sp6C;
    ModelRoData_BoundingBoxRecord *bboxRecord;
    struct PadRecord *padRecord;
    struct PropRecord *prop;
    f32 sp58;
    f32 sp54;
    f32 sp50;
    f32 sp48;

    modelID = objectRecord->obj;
    boundPadRecord = NULL;

    modelLoad(modelID);

    sp78 = objectRecord->extrascale * 0.00390625f;

    objectRecord->damage = *(s32*)&objectRecord->damage / 65536.0f;

    if (getPlayerCount() >= 2)
    {
        enableRespawn = 1;

        if ((get_scenario() == SCENARIO_TLD) && (objectRecord->obj == PROP_FLAG))
        {
            enableRespawn = 0;
        }
        else if ((get_scenario() == SCENARIO_MWTGG) && (objectRecord->obj == PROP_CHRGOLDEN))
        {
            enableRespawn = 0;
        }

        if (enableRespawn != 0)
        {
            objectRecord->state |= PROPSTATE_RESPAWN; // Enable respawn.
        }
    }

    if (objectRecord->flags & PROPFLAG_INSIDEANOTHEROBJ)
    {
        if (objectRecord->type == PROP_TYPE_SMOKE)
        {
            sub_GAME_7F051DD8(objectRecord, PitemZ_entries[modelID].header);
        }
        else
        {
            objInitWithModelDef(objectRecord, PitemZ_entries[modelID].header);
        }

        modelSetScale(objectRecord->model, objectRecord->model->scale * sp78);
    }
    else if (objectRecord->flags & PROPFLAG_ASSIGNEDTOCHR)
    {
        sp6C = chrFindByLiteralId(objectRecord->pad);

        if ((sp6C != NULL) && (sp6C->prop != NULL) && (sp6C->model != NULL))
        {
            if (objectRecord->type == 8)
            {
                var_v0 = sub_GAME_7F051DD8(objectRecord, PitemZ_entries[modelID].header);
            }
            else
            {
                var_v0 = objInitWithModelDef(objectRecord, PitemZ_entries[modelID].header);
            }

            modelSetScale(objectRecord->model, objectRecord->model->scale * sp78);
            chrpropReparent(var_v0, sp6C->prop);
        }
    }
    else
    {
        if (isNotBoundPad(objectRecord->pad))
        {
            padRecord = &g_CurrentSetup.pads[objectRecord->pad];

            matrix_4x4_set_basis_and_position_target(&sp8C, 0.0f, 0.0f, 0.0f, -padRecord->look.f[0], -padRecord->look.f[1], -padRecord->look.f[2], padRecord->up.f[0], padRecord->up.f[1], padRecord->up.f[2]);

            spD0.f[0] = padRecord->pos.f[0];
            spD0.f[1] = padRecord->pos.f[1];
            spD0.f[2] = padRecord->pos.f[2];

            if (objectRecord->flags & PROPFLAG_ONSCREEN)
            {
                sp80.f[0] = padRecord->pos.f[0];
                sp80.f[1] = padRecord->pos.f[1];
                sp80.f[2] = padRecord->pos.f[2];
            }
            else
            {
                // same as above?

                sp80.f[0] = padRecord->pos.f[0];
                sp80.f[1] = padRecord->pos.f[1];
                sp80.f[2] = padRecord->pos.f[2];
            }

            spCC = padRecord->stan;
        }
        else
        {
            boundPadRecord = &g_CurrentSetup.boundpads[getBoundPadNum(objectRecord->pad)];

            matrix_4x4_set_basis_and_position_target(&sp8C, 0.0f, 0.0f, 0.0f, -boundPadRecord->look.f[0], -boundPadRecord->look.f[1], -boundPadRecord->look.f[2], boundPadRecord->up.f[0], boundPadRecord->up.f[1], boundPadRecord->up.f[2]);

            if (!(objectRecord->flags2 & PROPFLAG2_DRONEGUN))
            {
                padGetCenter(boundPadRecord, &spD0);

                sp80.f[0] = spD0.f[0] + (boundPadRecord->up.f[0] * ((boundPadRecord->bbox.ymin - boundPadRecord->bbox.ymax) * 0.5f));
                sp80.f[1] = spD0.f[1] + (boundPadRecord->up.f[1] * ((boundPadRecord->bbox.ymin - boundPadRecord->bbox.ymax) * 0.5f));
                sp80.f[2] = spD0.f[2] + (boundPadRecord->up.f[2] * ((boundPadRecord->bbox.ymin - boundPadRecord->bbox.ymax) * 0.5f));

                spCC = boundPadRecord->stan;

                if (walkTilesBetweenPoints_NoCallback(&spCC, boundPadRecord->pos.f[0], boundPadRecord->pos.f[2], spD0.f[0], spD0.f[2]) == 0)
                {
                    spD0.f[0] = boundPadRecord->pos.f[0];
                    spD0.f[1] = boundPadRecord->pos.f[1];
                    spD0.f[2] = boundPadRecord->pos.f[2];

                    spCC = boundPadRecord->stan;
                }
            }
            else
            {
                spD0.f[0] = boundPadRecord->pos.f[0];
                spD0.f[1] = boundPadRecord->pos.f[1];
                spD0.f[2] = boundPadRecord->pos.f[2];

                spCC = boundPadRecord->stan;

                padGetCenter(boundPadRecord, &sp80);

                sp80.f[0] += (boundPadRecord->bbox.ymin - boundPadRecord->bbox.ymax) * 0.5f * boundPadRecord->up.f[0];
                sp80.f[1] += (boundPadRecord->bbox.ymin - boundPadRecord->bbox.ymax) * 0.5f * boundPadRecord->up.f[1];
                sp80.f[2] += (boundPadRecord->bbox.ymin - boundPadRecord->bbox.ymax) * 0.5f * boundPadRecord->up.f[2];
            }
        }

        if (getposstan(&spD0, spCC, 0.0f, &spE0, &spDC) != 0)
        {
            if (objectRecord->type == PROP_TYPE_SMOKE)
            {
                prop = sub_GAME_7F051DD8(objectRecord, PitemZ_entries[modelID].header);
            }
            else
            {
                prop = objInitWithAutoModel(objectRecord);
            }

            if (boundPadRecord != NULL)
            {
                bboxRecord = chrobjGetBboxFromObjectRecord(objectRecord);

                if (bboxRecord != NULL)
                {
                    sp58 = 1.0f;
                    sp54 = 1.0f;
                    sp50 = 1.0f;

                    if (objectRecord->flags & (PROPFLAG_00000010 | PROPFLAG_00000020))
                    {
                        if (bboxRecord->Bounds.xmin < bboxRecord->Bounds.xmax)
                        {
                            if (objectRecord->flags & PROPFLAG_ONSCREEN)
                            {
                                sp58 = (boundPadRecord->bbox.xmax - boundPadRecord->bbox.xmin) / ((bboxRecord->Bounds.xmax - bboxRecord->Bounds.xmin) * objectRecord->model->scale);
                            }
                            else
                            {
                                sp58 = (boundPadRecord->bbox.xmax - boundPadRecord->bbox.xmin) / ((bboxRecord->Bounds.xmax - bboxRecord->Bounds.xmin) * objectRecord->model->scale);
                            }
                        }
                    }

                    if (objectRecord->flags & (PROPFLAG_00000010 | PROPFLAG_00000040))
                    {
                        if (bboxRecord->Bounds.ymin < bboxRecord->Bounds.ymax)
                        {
                            if (objectRecord->flags & PROPFLAG_ONSCREEN)
                            {
                                sp50 = (boundPadRecord->bbox.zmax - boundPadRecord->bbox.zmin) / ((bboxRecord->Bounds.ymax - bboxRecord->Bounds.ymin) * objectRecord->model->scale);
                            }
                            else
                            {
                                sp54 = (boundPadRecord->bbox.ymax - boundPadRecord->bbox.ymin) / ((bboxRecord->Bounds.ymax - bboxRecord->Bounds.ymin) * objectRecord->model->scale);
                            }
                        }
                    }

                    if (objectRecord->flags & (PROPFLAG_00000010 | PROPFLAG_00000080))
                    {
                        if (bboxRecord->Bounds.zmin < bboxRecord->Bounds.zmax)
                        {
                            if (objectRecord->flags & PROPFLAG_ONSCREEN)
                            {
                                sp54 = (boundPadRecord->bbox.ymax - boundPadRecord->bbox.ymin) / ((bboxRecord->Bounds.zmax - bboxRecord->Bounds.zmin) * objectRecord->model->scale);
                            }
                            else
                            {
                                sp50 = (boundPadRecord->bbox.zmax - boundPadRecord->bbox.zmin) / ((bboxRecord->Bounds.zmax - bboxRecord->Bounds.zmin) * objectRecord->model->scale);
                            }
                        }
                    }

                    var_f0 = sp58;

                    if (sp54 < var_f0)
                    {
                        var_f0 = sp54;
                    }

                    if (sp50 < var_f0)
                    {
                        var_f0 = sp50;
                    }

                    sp48 = sp58;

                    if (sp58 < sp54)
                    {
                        sp48 = sp54;
                    }

                    if (sp48 < sp50)
                    {
                        sp48 = sp50;
                    }

                    if (objectRecord->flags & PROPFLAG_00000010)
                    {
                        sp50 = var_f0;
                        sp54 = var_f0;
                        sp58 = var_f0;
                    }
                    else
                    {
                        if (!(objectRecord->flags & PROPFLAG_00000020))
                        {
                            if (objectRecord->flags & PROPFLAG_ONSCREEN)
                            {
                                if (bboxRecord->Bounds.xmax == bboxRecord->Bounds.xmin)
                                {
                                    sp58 = sp48;
                                }
                            }
                            else if (bboxRecord->Bounds.xmax == bboxRecord->Bounds.xmin)
                            {
                                sp58 = sp48;
                            }
                        }

                        if (!(objectRecord->flags & PROPFLAG_00000040))
                        {
                            if (objectRecord->flags & PROPFLAG_ONSCREEN)
                            {
                                if (bboxRecord->Bounds.ymax == bboxRecord->Bounds.ymin)
                                {
                                    sp50 = sp48;
                                }
                            }
                            else if (bboxRecord->Bounds.ymax == bboxRecord->Bounds.ymin)
                            {
                                sp54 = sp48;
                            }
                        }

                        if (!(objectRecord->flags & PROPFLAG_00000080))
                        {
                            if (objectRecord->flags & PROPFLAG_ONSCREEN)
                            {
                                if (bboxRecord->Bounds.zmax == bboxRecord->Bounds.zmin)
                                {
                                    sp54 = sp48;
                                }
                            }
                            else if (bboxRecord->Bounds.zmax == bboxRecord->Bounds.zmin)
                            {
                                sp50 = sp48;
                            }
                        }
                    }

                    sp58 /= sp48;
                    sp54 /= sp48;
                    sp50 /= sp48;

                    /**
                     * Object scale is unacceptably small, scale it to 1.0.
                     */
                    if ((sp58 <= 0.000001f) || (sp54 <= 0.000001f) || (sp50 <= 0.000001f))
                    {
                        sp50 = 1.0f;
                        sp54 = 1.0f;
                        sp58 = 1.0f;
                    }

                    matrix_column_1_scalar_multiply(sp58, sp8C.m[0]);
                    matrix_column_2_scalar_multiply(sp54, sp8C.m[0]);
                    matrix_column_3_scalar_multiply_2(sp50, sp8C.m[0]);

                    modelSetScale(objectRecord->model, objectRecord->model->scale * sp48);
                }
            }

            modelSetScale(objectRecord->model, objectRecord->model->scale * sp78);
            matrix_scalar_multiply(objectRecord->model->scale, sp8C.m[0]);

            if (objectRecord->flags & PROPFLAG_ONSCREEN)
            {
                sub_GAME_7F040BA0(objectRecord, &spE0, &sp8C, spDC, &sp80);
            }
            else
            {
                objPlaceAtPad(objectRecord, &spE0, &sp8C, spDC, &sp80);
            }

            setupUpdateObjectRoomPosition(objectRecord);
            chrpropActivate(prop);
            chrpropEnable(prop);
        }
    }
}


void weaponAssignToHome(s32 arg0, WeaponObjRecord* weapon, s32 cmdindex)
{
    s32 padding;
    bool hastoken;
    ChrRecord* chr;
    bool giveweapon;
    s32 temp_a0;
    struct s_mp_weapon_set* weapon_set;

    if ((weapon->flags & PROPFLAG_ASSIGNEDTOCHR))
    {
        chr = chrFindByLiteralId(weapon->pad);

        if (chr && chr->prop && chr->model)
        {
            if (cheatIsActive(CHEAT_ENEMY_ROCKETS))
            {
                switch ((s8)weapon->weaponnum)
                {
                    case ITEM_KNIFE:
                    case ITEM_THROWKNIFE:
                    case ITEM_WPPK:
                    case ITEM_WPPKSIL:
                    case ITEM_TT33:
                    case ITEM_SKORPION:
                    case ITEM_AK47:
                    case ITEM_UZI:
                    case ITEM_MP5K:
                    case ITEM_MP5KSIL:
                    case ITEM_SPECTRE:
                    case ITEM_M16:
                    case ITEM_FNP90:
                    case ITEM_SHOTGUN:
                    case ITEM_AUTOSHOT:
                    case ITEM_SNIPERRIFLE:
                    case ITEM_RUGER:
                    case ITEM_GOLDENGUN:
                    case ITEM_SILVERWPPK:
                    case ITEM_GOLDWPPK:
                    case ITEM_LASER:
                    case ITEM_WATCHLASER:
                    case ITEM_REMOTEMINE:
                    case ITEM_TRIGGER:
                    case ITEM_TASER:
                        weapon->weaponnum = ITEM_ROCKETLAUNCH;
                        weapon->obj = PROP_CHRROCKETLAUNCH;
                        weapon->extrascale = 256;
                        break;
                }
            }

            weaponLoadProjectileModels((s8)weapon->weaponnum);
            sub_GAME_7F052030(weapon, chr);
        }
    }
    else
    {
        hastoken = 1;
        giveweapon = 1;

        if (getPlayerCount() >= 2)
        {
            g_MpSetupWeaponSlot = -1;

            switch ((u8)weapon->weaponnum)
            {
                case ITEM_UNARMED + 0xF0:
                case ITEM_FIST + 0xF0:
                case ITEM_KNIFE + 0xF0:
                case ITEM_THROWKNIFE + 0xF0:
                case ITEM_WPPK + 0xF0:
                case ITEM_WPPKSIL + 0xF0:
                case ITEM_TT33 + 0xF0:
                case ITEM_SKORPION + 0xF0:
                    weapon_set = getPtrMPWeaponSetData();

                    temp_a0 = (u8)weapon->weaponnum - 0xF0;
                    g_MpSetupWeaponSlot = temp_a0;

                    weapon->weaponnum = weapon_set[temp_a0].itemID;
                    weapon->obj = weapon_set[temp_a0].propID;
                    weapon->extrascale = (weapon_set[temp_a0].size * 256.0f);

                    giveweapon = weapon_set[temp_a0].allowpickup;

                    break;

                case ITEM_TOKEN:

                    hastoken = 1;
                    giveweapon = 1;

                    if (get_scenario() != SCENARIO_TLD)
                    {
                        giveweapon = 0;
                    }
                    break;
            }
        }

        if ((weapon->weaponnum != ITEM_UNARMED) && giveweapon)
        {
            weaponLoadProjectileModels(weapon->weaponnum);
            domakedefaultobj(arg0, (struct ObjectRecord*)weapon, cmdindex);
        }
    }
}

//i should be object hat
void setupHat(s32 arg0, ObjectRecord* hat, s32 cmdindex)
{
    if (hat->flags & PROPFLAG_ASSIGNEDTOCHR)
    {
        ChrRecord* chr = chrFindByLiteralId(hat->pad);
        if (chr && chr->prop && chr->model)
        {
            hatAssignToChr(hat, chr);
        }
    } 
    else 
    {
        domakedefaultobj(arg0, hat, cmdindex);
    }
}


//i should be object key
void setupKey(s32 arg0, ObjectRecord* key, s32 cmdindex)
{
    domakedefaultobj(arg0, key, cmdindex);
}


void setupCctv(s32 arg0, CCTVRecord *arg1, s32 cmdindex)
{
    struct coord3d *temp_a2;
    struct PadRecord *sp50;
    struct coord3d sp44;
    Mtxf *sp3C;

    domakedefaultobj(arg0, (struct ObjectRecord*)arg1, cmdindex);

    if (arg1->pad >= 0)
    {
        temp_a2 = (struct coord3d*)arg1->model->obj->Switches[0]->Data;

        if (isNotBoundPad(arg1->pad))
        {
            sp50 = &g_CurrentSetup.pads[arg1->pad];
        }
        else
        {
            sp50 = (struct PadRecord *)&g_CurrentSetup.boundpads[getBoundPadNum(arg1->pad)];
        }

        sp44.f[0] = temp_a2->f[0];
        sp44.f[1] = temp_a2->f[1];
        sp44.f[2] = temp_a2->f[2];

        mtx4RotateVecInPlace(&arg1->mtx, &sp44);

        sp3C = &arg1->unk84;

        sp44.f[0] += arg1->prop->pos.f[0];
        sp44.f[1] += arg1->prop->pos.f[1];
        sp44.f[2] += arg1->prop->pos.f[2];

        matrix_4x4_set_basis_and_position_target(sp3C, 0.0f, 0.0f, 0.0f, sp44.f[0] - sp50->pos.f[0], sp44.f[1] - sp50->pos.f[1], sp44.f[2] - sp50->pos.f[2], 0.0f, 1.0f, 0.0f);
        matrix_scalar_multiply(arg1->model->scale, sp3C->m[0]);

        if (arg1->convert_to_f32 == 0)
        {
            arg1->convert_to_f32 = 1;
            arg1->unkCC = (*(s32*)&arg1->unkCC * M_TAU_F) / 65536.0f;
            arg1->unkD0 = (*(s32*)&arg1->unkD0 * M_TAU_F) / 65536.0f;
            arg1->unkDC = (*(s32*)&arg1->unkDC * M_TAU_F) / 65536.0f;
            arg1->unkE8 = *(s32*)&arg1->unkE8;
        }

        arg1->unkD4 = 0;
        arg1->unkD8 = 0.0f;
        arg1->unkC8 = arg1->unkCC;
        arg1->unkC4 = atan2f(sp44.f[0] - sp50->pos.f[0], sp44.f[2] - sp50->pos.f[2]);
        arg1->timer = 0;
    }
}


void setupAutogun(s32 stageID, AutogunRecord *autogun, s32 cmdindex)
{
    s8 *beam;

    domakedefaultobj(stageID, (ObjectRecord *) autogun, cmdindex);

    autogun->unkAC = 0;
    autogun->unkB8 = -1;
    autogun->unkBC = -1;
    autogun->unkC0 = -1;
    autogun->unkC4 = 0;
    autogun->unkC8 = 0;
    autogun->unk90 = 0.0f;
    autogun->unk94 = 0.0f;
    autogun->rot_related = 0.0f;
    autogun->unk9C = 0.0f;
    autogun->unkA0 = 0.0f;
    autogun->unk98 = 0.0f;
    autogun->unkB0 = 0.0f;
    autogun->unkB4 = 0.0f;
    autogun->speed = ((*((s32 *) (&autogun->speed))) * M_TAU_F) / 65536.0f;
    autogun->aimdist = ((*((s32 *) (&autogun->aimdist))) * 100.0f) / 65536.0f;
    autogun->unk88 = ((*((s32 *) (&autogun->unk88))) * M_TAU_F) / 65536.0f;
    autogun->unk8C = ((*((s32 *) (&autogun->unk8C))) * M_TAU_F) / 65536.0f;

    beam = mempAllocBytesInBank(0x30U, MEMPOOL_STAGE);
    autogun->beam = beam;
    *beam = -1;

    autogun->is_active = FALSE;
    autogun->unkD4 = 0.0f;

    if (autogun->padID >= 0)
    {
        f32 xdiff;
        f32 ydiff;
        f32 zdiff;
        PadRecord *pad;
        PropRecord *prop;

        if (autogun->padID < 0x2710)
        {
            pad = &g_CurrentSetup.pads[autogun->padID];
        }
        else
        {
            pad = &g_CurrentSetup.boundpads[getBoundPadNum(autogun->padID)];
        }

        prop = autogun->prop;

        xdiff = pad->pos.x - prop->pos.x;
        ydiff = pad->pos.y - prop->pos.y;
        zdiff = pad->pos.z - prop->pos.z;

        autogun->rot_related = atan2f(xdiff, zdiff);
        autogun->unk98 = atan2f(ydiff, sqrtf((xdiff * xdiff) + (zdiff * zdiff)));
    }
}


//i should be object rack
void setupHangingMonitors(s32 arg0, ObjectRecord* rack, s32 cmdindex)
{
    domakedefaultobj(arg0, rack, cmdindex);
}


void setupSingleMonitor(s32 stageID, MonitorObjRecord *monitor, s32 cmdindex)
{
    MonitorRecord *record;
    s32 modelnum;
    ObjectRecord *owner;
    PropRecord *prop;
    f32 scale;

    monitor->Monitor = g_MonitorAnimController;
    record = &monitor->Monitor;
    monitorSetImageByNum(&monitor->Monitor, monitor->ImageNum);

    if (monitor->pad < 0 && (monitor->flags & PROPFLAG_INSIDEANOTHEROBJ) == 0)
    {
        modelnum = monitor->obj;
        owner = (struct ObjectRecord *)setupGetPtrToCommandByIndex(cmdindex + monitor->OwnerOffset);

        modelLoad(modelnum);

        scale = monitor->extrascale * (1.0f / 256.0f);
        monitor->damage = *(s32*)&monitor->damage / M_U16_MAX_VALUE_F;

        if (getPlayerCount() >= 2)
        {
            monitor->state |= PROPSTATE_RESPAWN;
        }

        prop = objInitWithAutoModel((ObjectRecord*)monitor);
        monitor->embedment = embedmentAllocate();

        if (prop && monitor->embedment)
        {
            monitor->runtime_bitflags |= RUNTIMEBITFLAG_EMBEDDED;
            modelSetScale(monitor->model, monitor->model->scale * scale);
            monitor->model->attachedto = owner->model;

            if (monitor->OwnerPart == 0)
            {
                monitor->model->attachedto_objinst = owner->model->obj->Switches[0];
            }
            else if (monitor->OwnerPart == 1)
            {
                monitor->model->attachedto_objinst = owner->model->obj->Switches[1];
            }
            else if (monitor->OwnerPart == 2)
            {
                monitor->model->attachedto_objinst = owner->model->obj->Switches[2];
            }
            else
            {
                monitor->model->attachedto_objinst = owner->model->obj->Switches[3];;
            }

            chrpropReparent(prop, owner->prop);
            matrix_4x4_set_rotation_around_x(0.36651915f, (Mtxf*)&monitor->embedment->matrix);
            matrix_scalar_multiply(monitor->model->scale / owner->model->scale, (f32*)&monitor->embedment->matrix);
        }
    }
    else
    {
        domakedefaultobj(stageID, (ObjectRecord*)monitor, cmdindex);
    }

    if ((monitor->flags & PROPFLAG_MONITOR_RENDERPOSTBG) && monitor->prop)
    {
        monitor->prop->flags |= PROPFLAG_RENDERPOSTBG;
    }
}


void setupMultiMonitor(s32 stageID, MultiMonitorObjRecord* monitor, s32 cmdindex)
{
    monitor->Monitor[0] = g_MonitorAnimController;
    monitorSetImageByNum(&monitor->Monitor[0], monitor->ImageNums[0]);

    monitor->Monitor[1] = g_MonitorAnimController;
    monitorSetImageByNum(&monitor->Monitor[1], monitor->ImageNums[1]);

    monitor->Monitor[2] = g_MonitorAnimController;
    monitorSetImageByNum(&monitor->Monitor[2], monitor->ImageNums[2]);

    monitor->Monitor[3] = g_MonitorAnimController;
    monitorSetImageByNum(&monitor->Monitor[3], monitor->ImageNums[3]);

    domakedefaultobj(stageID, monitor, cmdindex);
}


/**
 * Determines which rooms lie on either side of a bound pad by probing 50 units along the pad's normal (up × look) from its center.
 * Used by setupDoor to discover the pair of rooms a door connects, for its portal. If both probes reach the same room *backRoom is set to -1.
 */
void setupGetDoorAdjacentRooms(struct BoundPadRecord *pad, s32 *frontRoom, s32 *backRoom, struct coord3d *frontProbePos, struct coord3d *backProbePos)
{
    StandTile *walkStan;
    struct coord3d normal;
    struct coord3d center;
    StandTile *centerStan;
    f32 scale;

    padGetCenter(pad, &center);
    centerStan = (StandTile *)pad->stan;

    if (walkTilesBetweenPoints_NoCallback(&centerStan, pad->pos.f[0], pad->pos.f[2], center.f[0], center.f[2]) == 0)
    {
        centerStan = (StandTile *)pad->stan;
        center.f[0] = pad->pos.f[0];
        center.f[1] = pad->pos.f[1];
        center.f[2] = pad->pos.f[2];
    }

    normal.f[0] = (pad->up.f[1] * pad->look.f[2]) - (pad->up.f[2] * pad->look.f[1]);
    normal.f[1] = (pad->up.f[2] * pad->look.f[0]) - (pad->up.f[0] * pad->look.f[2]);
    normal.f[2] = (pad->up.f[0] * pad->look.f[1]) - (pad->up.f[1] * pad->look.f[0]);

    scale = 1.0f / sqrtf(((normal.f[0] * normal.f[0]) + (normal.f[1] * normal.f[1])) + (normal.f[2] * normal.f[2]));
    walkStan = centerStan;

    normal.f[0] *= scale;
    normal.f[1] *= scale;
    normal.f[2] *= scale;

    frontProbePos->f[0] = center.f[0] + (normal.f[0] * 50.0f);
    frontProbePos->f[1] = center.f[1];
    frontProbePos->f[2] = center.f[2] + (normal.f[2] * 50.0f);

    walkTilesBetweenPoints_NoCallback(&walkStan, center.f[0], center.f[2], frontProbePos->f[0], frontProbePos->f[2]);

    *frontRoom = (s32) walkStan->room;
    walkStan = centerStan;

    backProbePos->f[0] = center.f[0] - (normal.f[0] * 50.0f);
    backProbePos->f[1] = center.f[1];
    backProbePos->f[2] = center.f[2] - (normal.f[2] * 50.0f);

    walkTilesBetweenPoints_NoCallback(&walkStan, center.f[0], center.f[2], backProbePos->f[0], backProbePos->f[2]);

    *backRoom = (s32) walkStan->room;

    if (*backRoom == *frontRoom)
    {
        *backRoom = -1;
    }
}


/**
 * Creates a door object from its setup definition. It resolves the door's
 * bound pad and discovers the pair of rooms the door connects along with their
 * portal, for CULL_BEHIND_DOOR / NO_PORTAL_CLOSE doors. Then it applies the stage's
 * g_DoorScale to the pad volume, builds the door's transform from the pad frame
 * scaled to fit the pad bounds, computes the open-travel displacement along look
 * for vertical/fallaway doors, along up otherwise. It also converts the setup
 * file's 16.16 fixed point motion fields to floats then initialises, registers,
 * and activates the door prop in its rooms.
 */
void setupDoor(struct DoorRecord *door, s32 cmdindex)
{
    s32 modelnum;
    struct BoundPadRecord *pad;
    StandTile *groundedStan;
    PropRecord *prop;
    struct coord3d groundedPos;
    s32 portalnum;
    s32 frontRoom;
    s32 backRoom;
    struct coord3d frontProbePos;
    struct coord3d backProbePos;
    struct PortalMetric portalMetric;
    struct ModelRoData_BoundingBoxRecord *modelBBox;
    struct coord3d scaledPadPos;
    StandTile *walkStan;
    Mtxf lookUpMtx;
    f32 planeDist;
    ModelFileHeader *modelHeader;
    struct coord3d doorPos;
    StandTile *centerWalkStan;
    Mtxf doorTransform;
    struct coord3d doorCenter;
    Mtxf rotZMtx;
    struct coord3d openDisplacement;
    struct bbox localBBox;
    f32 xscale;
    f32 yscale;
    f32 zscale;
    f32 scale;

    modelnum = door->obj;

    portalnum = -1;
    frontRoom = -1;
    backRoom = -1;

    modelLoad(modelnum);

    pad = &(g_CurrentSetup.boundpads[door->pad]);

    if ((door->flags & PROPFLAG_CULL_BEHIND_DOOR) || (door->flags & PROPFLAG_NO_PORTAL_CLOSE))
    {
        setupGetDoorAdjacentRooms(pad, &frontRoom, &backRoom, &frontProbePos, &backProbePos);

        if ((door->flags & PROPFLAG_CULL_BEHIND_DOOR) && (frontRoom >= 0) && (backRoom >= 0))
        {
            portalnum = bgGetPortalBetweenRooms(frontRoom, backRoom, &frontProbePos, &backProbePos);
        }
    }

    if (g_DoorScale != 1.0f)
    {
        if (portalnum >= 0)
        {
            bgCalcPortalPlane(portalnum, &portalMetric);
            portalMetric.min *= bgGetRoomInverseScale();

            planeDist = (pad->pos.f[0] * portalMetric.normal.f[0]) + (pad->pos.f[1] * portalMetric.normal.f[1]) + (pad->pos.f[2] * portalMetric.normal.f[2]);

            if (g_DoorScale < 1.0f)
            {
                planeDist = (planeDist - portalMetric.min) * (1.0f - g_DoorScale);
                scaledPadPos.f[0] = pad->pos.f[0] - (portalMetric.normal.f[0] * planeDist);
                scaledPadPos.f[1] = pad->pos.f[1] - (portalMetric.normal.f[1] * planeDist);
                scaledPadPos.f[2] = pad->pos.f[2] - (portalMetric.normal.f[2] * planeDist);
            }
            else
            {
                planeDist = (planeDist - portalMetric.min) * (g_DoorScale - 1.0f);
                scaledPadPos.f[0] = pad->pos.f[0] + (portalMetric.normal.f[0] * planeDist);
                scaledPadPos.f[1] = pad->pos.f[1] + (portalMetric.normal.f[1] * planeDist);
                scaledPadPos.f[2] = pad->pos.f[2] + (portalMetric.normal.f[2] * planeDist);
            }

            walkStan = pad->stan;

            if (walkTilesBetweenPoints_NoCallback(&walkStan, pad->pos.f[0], pad->pos.f[2], scaledPadPos.f[0], scaledPadPos.f[2]) != 0)
            {
                pad->stan = walkStan;
                pad->pos.f[0] = scaledPadPos.f[0];
                pad->pos.f[1] = scaledPadPos.f[1];
                pad->pos.f[2] = scaledPadPos.f[2];
                pad->bbox.xmin *= g_DoorScale;
                pad->bbox.xmax *= g_DoorScale;
            }
            // else: pad position unreachable - the door volume keeps its
            // original depth.
        }
        else
        {
            pad->bbox.xmin *= g_DoorScale;
            pad->bbox.xmax *= g_DoorScale;
        }
    }

    if (getposstan(&pad->pos, pad->stan, 0.0f, &groundedPos, &groundedStan) != 0)
    {
        matrix_4x4_set_basis_and_position_target(&lookUpMtx, 0, 0, 0, -pad->look.f[0], -pad->look.f[1], -pad->look.f[2], pad->up.f[0], pad->up.f[1], pad->up.f[2]);
        modelHeader = PitemZ_entries[modelnum].header;
        centerWalkStan = groundedStan;

        localBBox.zmax = pad->bbox.xmin;
        localBBox.zmin = pad->bbox.xmax;
        localBBox.ymax = pad->bbox.ymin;
        localBBox.ymin = pad->bbox.ymax;
        localBBox.xmax = pad->bbox.zmin;
        localBBox.xmin = pad->bbox.zmax;

        matrix_4x4_set_rotation_around_x(M_HALF_PI, &doorTransform);
        matrix_4x4_set_rotation_around_z(M_HALF_PI, &rotZMtx);
        matrix_4x4_multiply_in_place(&rotZMtx, &doorTransform);
        matrix_4x4_multiply_in_place(&lookUpMtx, &doorTransform);
        padGetCenter(pad, &doorPos);

        modelBBox = (struct ModelRoData_BoundingBoxRecord *)modelHeader->RootNode->Child->Data;

        xscale = (localBBox.ymin - localBBox.ymax) / (modelBBox->Bounds.xmax - modelBBox->Bounds.xmin);
        yscale = (localBBox.xmin - localBBox.xmax) / (modelBBox->Bounds.ymax - modelBBox->Bounds.ymin);
        zscale = (localBBox.zmin - localBBox.zmax) / (modelBBox->Bounds.zmax - modelBBox->Bounds.zmin);

        if ((xscale <= 0.000001f) || (yscale <= 0.000001f) || (zscale <= 0.000001f))
        {
            // Degenerate pad bounds - fall back to unscaled model.
            xscale = yscale = zscale = 1.0f;
        }

        matrix_column_1_scalar_multiply(xscale, doorTransform.m[0]);
        matrix_column_2_scalar_multiply(yscale, doorTransform.m[0]);
        matrix_column_3_scalar_multiply_2(zscale, doorTransform.m[0]);

        doorCenter.f[0] = doorPos.f[0];
        doorCenter.f[1] = doorPos.f[1];
        doorCenter.f[2] = doorPos.f[2];

        if (!(door->flags2 & 1))
        {
            if (walkTilesBetweenPoints_NoCallback(&centerWalkStan, groundedPos.f[0], groundedPos.f[2], doorPos.f[0], doorPos.f[2]) != 0)
            {
                groundedStan = centerWalkStan;
            }
            else
            {
                doorPos.f[0] = groundedPos.f[0];
                doorPos.f[2] = groundedPos.f[2];
            }
        }
        else
        {
            doorPos.f[0] = groundedPos.f[0];
            doorPos.f[1] = groundedPos.f[1];
            doorPos.f[2] = groundedPos.f[2];
        }

        if ((door->doorType == DOORTYPE_VERTICAL) || (door->doorType == DOORTYPE_FALLAWAY))
        {
            openDisplacement.f[0] = pad->look.f[0] * (localBBox.xmin - localBBox.xmax);
            openDisplacement.f[1] = pad->look.f[1] * (localBBox.xmin - localBBox.xmax);
            openDisplacement.f[2] = pad->look.f[2] * (localBBox.xmin - localBBox.xmax);
        }
        else
        {
            openDisplacement.f[0] = pad->up.f[0] * (localBBox.ymax - localBBox.ymin);
            openDisplacement.f[1] = pad->up.f[1] * (localBBox.ymax - localBBox.ymin);
            openDisplacement.f[2] = pad->up.f[2] * (localBBox.ymax - localBBox.ymin);
        }

        /**
         * These values are stored in the setup files as integers, but at
         * runtime they are floats. Hence reading a float as an integer,
         * converting it to a float and writing it back to the same property.
         */
        door->maxFrac = *(s32 *) &door->maxFrac / 65536.0f;
        door->perimFrac = *(s32 *) &door->perimFrac / 65536.0f;
        door->accel = (*(s32 *) &door->accel) / 65536.0f;
        door->decel = (*(s32 *) &door->decel) / 65536.0f;
        door->maxSpeed = (*(s32 *) &door->maxSpeed) / 65536.0f;

        prop = doorInit(door, &doorPos, &doorTransform, groundedStan, &openDisplacement, &doorCenter);

        if (door->flags & PROPFLAG_CULL_BEHIND_DOOR)
        {
            door->portalNumber = portalnum;
            if ((portalnum >= 0) && (door->openPosition == 0.0f))
            {
                doorDeactivatePortal(door);
            }
            // else: no portal found between the door's rooms
        }

        prop->rooms[0] = prop->stan->room;
        chrpropRegisterRoom(prop, prop->stan->room);
        prop->rooms[1] = 0xFFU;
        prop->rooms[2] = 0xFFU;

        if ((door->flags & PROPFLAG_CULL_BEHIND_DOOR) || (door->flags & PROPFLAG_NO_PORTAL_CLOSE))
        {
            if (frontRoom != prop->stan->room)
            {
                if (frontRoom >= 0)
                {
                    prop->rooms[1] = frontRoom;
                    chrpropRegisterRoom(prop, frontRoom);
                }
            }
            else if (backRoom >= 0)
            {
                prop->rooms[1] = backRoom;
                chrpropRegisterRoom(prop, backRoom);
            }
        }

        if (door->model != NULL)
        {
            scale = xscale;

            if (scale < yscale)
            {
                scale = yscale;
            }

            if (scale < zscale)
            {
                scale = zscale;
            }

            modelSetScale(door->model, door->model->scale * scale);
        }

        chrpropActivate(prop);
        chrpropEnable(prop);

        if (door->linkedDoorOffset != 0)
        {
            door->linkedDoor = (struct DoorRecord *)setupGetPtrToCommandByIndex(door->linkedDoorOffset + cmdindex);
        }
    }
    else
    {
        // Pad has no floor beneath it. The door cannot be created.
        door->prop = NULL;
    }

}


void setupLoadFiles(enum LEVELID stageId)
{
    ItemModelFileRecord *itemModel;
    struct levelentry *levelInfo;

    g_DoorScale = 1.0f;
    g_MpSetupWeaponSlot = -1;

    /**
     * Mark every prop model as "not resident" so the model loads later in this function actually fetch data. Essentially
     * RootNode is doubling as a loaded flag for function modelLoad().
     * 
     * The last entry in the PitemZ_entries table is a terminator which is why 1 is subtracted from the loop length.
     */
    for (itemModel = PitemZ_entries; itemModel < &PitemZ_entries[ARRAYCOUNT(PitemZ_entries) - 1]; itemModel++)
    {
        itemModel->header->RootNode = NULL;
    }

    levelInfo = lvFindLevelInfo(stageId);

    if ((levelInfo != NULL) && (levelInfo->setupFileName != NULL))
    {
        char setupFileName[0x100] = "";
        struct SetupSlotCounts slotCounts;
        struct stagesetup *setupFile;

        setupFileName[0] = levelInfo->setupFileName[0];
        setupFileName[1] = '\0';

        /**
         * g_LevelInfoTable stores the single-player setup name. The multiplayer name is created
         * by adding "mp_" after the "U", for example "Ump_setuparchZ".
         */
        if (getPlayerCount() >= 2)
        {
            strcat(setupFileName, "mp_");
        }

        strcat(setupFileName, levelInfo->setupFileName + 1);

        g_ptrStageSetupFile = _fileNameLoadToBank(setupFileName, FILELOADMETHOD_DEFAULT, 256, MEMPOOL_STAGE);

        setupFile = g_ptrStageSetupFile;
        langLoadToAddr(langGetLangBankIndexFromStagenum(stageId));

        /**
         * The setup file stores every internal reference as a byte offset from the start of the file,
         * so rebase them all onto the RAM copy at setupFile.
         */
        g_CurrentSetup.pathwaypoints = (void *) ((u32) setupFile + (u32) setupFile->pathwaypoints);
        g_CurrentSetup.waypointgroups = (void *) ((u32) setupFile + (u32) setupFile->waypointgroups);
        g_CurrentSetup.intro = (void *) ((u32) setupFile + (u32) setupFile->intro);
        g_CurrentSetup.propDefs = (void *) ((u32) setupFile + (u32) setupFile->propDefs);
        g_CurrentSetup.patrolpaths = (void *) ((u32) setupFile + (u32) setupFile->patrolpaths);
        g_CurrentSetup.ailists = (void *) ((u32) setupFile + (u32) setupFile->ailists);
        g_CurrentSetup.pads = (void *) ((u32) setupFile + (u32) setupFile->pads);
        g_CurrentSetup.boundpads = (void *) ((u32) setupFile + (u32) setupFile->boundpads);

        // Pad names and bound names are optional. An offset of 0 means absent.
        if (setupFile->padnames != NULL)
        {
            g_CurrentSetup.padnames = (void *) ((u32) setupFile + (u32) setupFile->padnames);
        }
        else
        {
            g_CurrentSetup.padnames = NULL;
        }

        if (setupFile->boundpadnames != NULL)
        {
            g_CurrentSetup.boundpadnames = (void *) ((u32) setupFile + (u32) setupFile->boundpadnames);
        }
        else
        {
            g_CurrentSetup.boundpadnames = NULL;
        }

        if (g_CurrentSetup.pathwaypoints)
        {
            waypoint *waypointEntry;

            for (waypointEntry = g_CurrentSetup.pathwaypoints; waypointEntry->padID >= 0; waypointEntry++)
            {
                waypointEntry->neighbours = (void *) ((u32) setupFile + (u32) waypointEntry->neighbours);
            }
        }

        if (g_CurrentSetup.waypointgroups)
        {
            waygroup *waypointGroup;

            for (waypointGroup = g_CurrentSetup.waypointgroups; waypointGroup->neighbours != NULL; waypointGroup++)
            {
                waypointGroup->neighbours = (void *) ((u32) setupFile + (u32) waypointGroup->neighbours);
                waypointGroup->waypoints = (void *) ((u32) setupFile + (u32) waypointGroup->waypoints);
            }
        }

        // Convert AI list (action block) offsets to pointers.
        if (g_CurrentSetup.ailists)
        {
            AIListRecord *aiList;

            for (aiList = g_CurrentSetup.ailists; aiList->ailist != NULL; aiList++)
            {
                aiList->ailist = (void *) ((u32) setupFile + (u32) aiList->ailist);
            }
        }

        if (g_CurrentSetup.patrolpaths)
        {
            PathRecord *patrolPath;

            for (patrolPath = g_CurrentSetup.patrolpaths; patrolPath->waypoints != NULL; patrolPath++)
            {
                patrolPath->waypoints = (void *) ((u32) setupFile + (u32) patrolPath->waypoints);
                patrolPath->len = 0;
            }
        }

        if (g_CurrentSetup.pads)
        {
            struct PadRecord *pad;
            f32 roomScale = bgGetRoomInverseScale();

            for (pad = g_CurrentSetup.pads; pad->plink != NULL; pad++)
            {
                pad->plink = (void *) ((u32) setupFile + (u32) pad->plink);
                pad->pos.x *= roomScale;
                pad->pos.y *= roomScale;
                pad->pos.z *= roomScale;

                padAssignStanTile(pad, pad->plink, &pad->stan);
            }
        }

        if (g_CurrentSetup.boundpads)
        {
            struct BoundPadRecord *boundPad;
            f32 roomScale = bgGetRoomInverseScale();

            for (boundPad = g_CurrentSetup.boundpads; boundPad->plink != NULL; boundPad++)
            {
                boundPad->plink = (void *) ((u32) setupFile + (u32) boundPad->plink);
                boundPad->pos.x *= roomScale;
                boundPad->pos.y *= roomScale;
                boundPad->pos.z *= roomScale;
                boundPad->bbox.xmin *= roomScale;
                boundPad->bbox.xmax *= roomScale;
                boundPad->bbox.ymin *= roomScale;
                boundPad->bbox.ymax *= roomScale;
                boundPad->bbox.zmin *= roomScale;
                boundPad->bbox.zmax *= roomScale;

                padAssignStanTile((struct PadRecord *) boundPad, boundPad->plink, &boundPad->stan);
            }
        }

        if (g_CurrentSetup.padnames)
        {
            pname *padName;

            for (padName = g_CurrentSetup.padnames; padName->p != NULL; padName++)
            {
                padName->p = (void *) ((u32) setupFile + (u32) padName->p);
            }
        }

        if (g_CurrentSetup.boundpadnames)
        {
            pname *boundPadName;

            for (boundPadName = g_CurrentSetup.boundpadnames; boundPadName->p != NULL; boundPadName++)
            {
                boundPadName->p = (void *) ((u32) setupFile + (u32) boundPadName->p);
            }
        }

        setupCountRequiredSlots(&slotCounts);
        alloc_init_GUARDdata_entries(slotCounts.guards);
        modelmgrAllocateModelSlots(slotCounts.modelSlots);
        modelmgrAllocateAnimModelSlots(slotCounts.animatedModelSlots);

        {
            s32 player;

            for (player = 0; player < getPlayerCount(); player++)
            {
                set_cur_player(player);
                alloc_additional_item_slots(slotCounts.itemLinks);
            }
        }

        if (g_CurrentSetup.propDefs)
        {
            PropDefHeaderRecord *command;
            s32 exclusionFlags;
            s32 commandIndex;

            // PROPFLAG2_00000010/20/40 exclude objects on Agent, Secret Agent, and 00 Agent.
            exclusionFlags = 1 << (lvGetSelectedDifficulty() + 4);

            /**
             * Complete the skip loading mask started on the line above. Checks for:
             * - don't load on 2 players
             * - don't load on 3 players
             * - don't load on 4 players
             * - don't load in multiplayer
             */
            if (getPlayerCount() >= 2)
            {
                exclusionFlags |= 1 << (getPlayerCount() + 20);
            }

            // First pass: create stage entities and register objectives and metadata.
            command = g_CurrentSetup.propDefs;
            commandIndex = 0;

            while (command->type != PROPDEF_END)
            {
                switch (command->type)
                {
                    case PROPDEF_GUARD_ATTRIBUTE:
                    {
                        GuardAttributeRecord *guardAttribute = (GuardAttributeRecord *) command;
                        ChrRecord *guard = chrFindByLiteralId(guardAttribute->chrnum);

                        if (guard && guard->prop && guard->model)
                        {
                            guard->grenadeprob = (u8) guardAttribute->GrenadeProb;
                        }

                        break;
                    }
                    case PROPDEF_GUARD:
                        expand_09_characters(stageId, (struct GuardRecord *) command, commandIndex);
                        break;
                    case PROPDEF_DOOR:
                        if (setupShouldLoadObject((ObjectRecord *) command, exclusionFlags))
                        {
                            setupDoor((struct DoorRecord *) command, commandIndex);
                        }
                        break;
                    case PROPDEF_DOOR_SCALE:
                        g_DoorScale = ((struct GlobalDoorScaleRecord *) command)->Scale / M_U16_MAX_VALUE_F;
                        break;
                    case PROPDEF_COLLECTABLE:
                        if (setupShouldLoadObject((ObjectRecord *) command, exclusionFlags))
                        {
                            weaponAssignToHome(stageId, (struct WeaponObjRecord *) command, commandIndex);
                        }
                        break;
                    case PROPDEF_KEY:
                        if (setupShouldLoadObject((ObjectRecord *) command, exclusionFlags))
                        {
                            setupKey(stageId, (struct ObjectRecord *) command, commandIndex);
                        }
                        break;
                    case PROPDEF_HAT:
                        if (setupShouldLoadObject((ObjectRecord *) command, exclusionFlags))
                        {
                            setupHat(stageId, (struct ObjectRecord *) command, commandIndex);
                        }
                        break;
                    case PROPDEF_CCTV:
                        if (setupShouldLoadObject((ObjectRecord *) command, exclusionFlags))
                        {
                            setupCctv(stageId, (struct CCTVRecord *) command, commandIndex);
                        }
                        break;
                    case PROPDEF_AUTOGUN:
                        if (setupShouldLoadObject((ObjectRecord *) command, exclusionFlags))
                        {
                            setupAutogun(stageId, (struct AutogunRecord *) command, commandIndex);
                        }
                        break;
                    case PROPDEF_RACK:
                        if (setupShouldLoadObject((ObjectRecord *) command, exclusionFlags))
                        {
                            setupHangingMonitors(stageId, (struct ObjectRecord *) command, commandIndex);
                        }
                        break;
                    case PROPDEF_MONITOR:
                        if (setupShouldLoadObject((ObjectRecord *) command, exclusionFlags))
                        {
                            setupSingleMonitor(stageId, (struct MonitorObjRecord *) command, commandIndex);
                        }
                        break;
                    case PROPDEF_MULTI_MONITOR:
                        if (setupShouldLoadObject((ObjectRecord *) command, exclusionFlags))
                        {
                            setupMultiMonitor(stageId, (struct MultiMonitorObjRecord *) command, commandIndex);
                        }
                        break;
                    case PROPDEF_ARMOUR:
                    {
                        struct BodyArmourRecord *armour = (struct BodyArmourRecord *) command;

                        if (setupShouldLoadObject((ObjectRecord *) armour, exclusionFlags))
                        {
                            armour->initialamount = *(s32 *) &armour->initialamount / M_U16_MAX_VALUE_F;
                            armour->amount = armour->initialamount;
                            domakedefaultobj(stageId, (struct ObjectRecord *) armour, commandIndex);
                        }
                        break;
                    }
                    case PROPDEF_TINTED_GLASS:
                    {
                        struct TintedGlassRecord *glass = (struct TintedGlassRecord *) command;

                        if (setupShouldLoadObject((ObjectRecord *) glass, exclusionFlags))
                        {
                            if ((glass->flags & PROPFLAG_GLASS_HASPORTAL) && (glass->pad >= 10000))
                            {
                                struct coord3d lineStart;
                                struct coord3d lineEnd;
                                BoundPadRecord *boundPad = &g_CurrentSetup.boundpads[glass->pad - 10000];

                                padGetCenter(boundPad, &lineStart);
                                lineEnd.x = lineStart.x + 10.0f * boundPad->up.x;
                                lineEnd.y = lineStart.y + 10.0f * boundPad->up.y;
                                lineEnd.z = lineStart.z + 10.0f * boundPad->up.z;
                                lineStart.x -= 10.0f * boundPad->up.x;
                                lineStart.y -= 10.0f * boundPad->up.y;
                                lineStart.z -= 10.0f * boundPad->up.z;

                                glass->portalnum = bgFindPortalCrossedByLine(&lineStart, &lineEnd);
                                glass->unk90 = *(s32 *) &glass->unk90 / M_U16_MAX_VALUE_F;
                            }

                            domakedefaultobj(stageId, (struct ObjectRecord *) glass, commandIndex);
                        }
                        break;
                    }
                    case PROPDEF_PROP:
                    case PROPDEF_ALARM:
                    case PROPDEF_MAGAZINE:
                    case PROPDEF_GAS_RELEASING:
                    case PROPDEF_UNK41:
                    case PROPDEF_GLASS:
                    case PROPDEF_SAFE:
                        if (setupShouldLoadObject((ObjectRecord *) command, exclusionFlags))
                        {
                            domakedefaultobj(stageId, (struct ObjectRecord *) command, commandIndex);
                        }
                        break;
                    case PROPDEF_AMMO:
                    {
                        struct MultiAmmoCrateRecord *ammoCrate = (struct MultiAmmoCrateRecord *) command;
                        s32 ammoQuantity = 1;

                        if (getPlayerCount() >= 2)
                        {
                            ammoQuantity = 0;

                            if (g_MpSetupWeaponSlot >= 0)
                            {
                                struct s_mp_weapon_set *weaponSetEntry = &getPtrMPWeaponSetData()[g_MpSetupWeaponSlot];
                                s32 ammoType = weaponSetEntry->ammotype;

                                if ((ammoType > AMMO_NONE) && (ammoType <= AMMOTYPE_GLOBAL_MAX))
                                {
                                    ammoQuantity = weaponSetEntry->ammoamount;
                                    ammoCrate->slots[ammoType - 1].quantity = ammoQuantity;
                                }
                            }
                        }

                        if ((ammoQuantity > 0) && setupShouldLoadObject((ObjectRecord *) ammoCrate, exclusionFlags))
                        {
                            s32 slotIndex;

                            for (slotIndex = 0; slotIndex < AMMOTYPE_GLOBAL_MAX; slotIndex++)
                            {
                                if ((ammoCrate->slots[slotIndex].quantity > 0) && (ammoCrate->slots[slotIndex].modelnum != 0xFFFF))
                                {
                                    modelLoad(ammoCrate->slots[slotIndex].modelnum);
                                }
                            }

                            domakedefaultobj(stageId, (struct ObjectRecord *) ammoCrate, commandIndex);
                        }
                        break;
                    }
                    case PROPDEF_TANK:
                        if (setupShouldLoadObject((ObjectRecord *) command, exclusionFlags))
                        {
                            struct TankRecord *tank = (struct TankRecord *) command;
                            struct PropRecord *tankProp;
                            f32 groundY = 0.0f;

                            weaponLoadProjectileModels(ITEM_TANKSHELLS);
                            domakedefaultobj(stageId, (struct ObjectRecord *) tank, commandIndex);
                            tank->turret_vertical_angle = 0.0f;
                            tank->turret_orientation_angle = 0.0f;
                            tank->tank_orientation_angle = M_TAU_F - atan2f(tank->mtx.m[2][0], tank->mtx.m[2][2]);
                            tankProp = tank->prop;

                            if (tankProp)
                            {
                                groundY = stanGetPositionYValue(tankProp->stan, tankProp->pos.x, tankProp->pos.z);
                            }

                            tank->stan_y = groundY;
                            tank->unkD0 = groundY / 0.17000002f;
                        }
                        break;
                    case PROPDEF_VEHICLE:
                        if (setupShouldLoadObject((ObjectRecord *) command, exclusionFlags))
                        {
                            struct VehichleRecord *vehicle = (struct VehichleRecord *) command;

                            domakedefaultobj(stageId, (struct ObjectRecord *) vehicle, commandIndex);

                            if ((vehicle->model != NULL) && (vehicle->model->obj->Switches[5] != NULL))
                            {
                                modelGetNodeRwData(vehicle->model, vehicle->model->obj->Switches[5])->Raw.unk00 = (vehicle->flags & 0x10000000) == 0;
                            }

                            vehicle->speed        = 0.0f;
                            vehicle->wheelxrot    = 0.0f;
                            vehicle->wheelyrot    = 0.0f;
                            vehicle->speedaim     = 0.0f;
                            vehicle->turnrot60    = 0.0f;
                            vehicle->roty         = 0.0f;
                            vehicle->speedtime60  = -1.0f;
                            vehicle->ailist       = ailistFindById(vehicle->ailist);
                            vehicle->aioffset     = 0;
                            vehicle->aireturnlist = -1;
                            vehicle->path         = 0;
                            vehicle->nextstep     = 0;
                            vehicle->Sound        = 0;
                        }
                        break;
                    case PROPDEF_AIRCRAFT:
                        if (setupShouldLoadObject((ObjectRecord *) command, exclusionFlags))
                        {
                            struct AircraftRecord *aircraft = (struct AircraftRecord *) command;

                            domakedefaultobj(stageId, (struct ObjectRecord *) aircraft, commandIndex);
                            aircraft->speed           = 0.0f;
                            aircraft->speedaim        = 0.0f;
                            aircraft->rotoryrot       = 0.0f;
                            aircraft->rotaryspeed     = 0.0f;
                            aircraft->rotaryspeedaim  = 0.0f;
                            aircraft->yrot            = 0.0f;
                            aircraft->speedtime60     = -1.0f;
                            aircraft->rotaryspeedtime = -1.0f;
                            aircraft->ailist          = ailistFindById(aircraft->ailist);
                            aircraft->aioffset        = 0;
                            aircraft->aireturnlist    = -1;
                            aircraft->nextstep        = 0;
                            aircraft->path            = 0;
                            aircraft->Sound           = 0;
                        }
                        break;
                    case PROPDEF_TAG:
                    {
                        struct TagObjectRecord *tag = (struct TagObjectRecord *) command;
                        struct ObjectRecord *taggedObject;

                        taggedObject = setupCommandGetObject(stageId, commandIndex + (s32) tag->OffsetToObj);
                        tag->TaggedObject = taggedObject;

                        if (taggedObject)
                        {
                            taggedObject->runtime_bitflags |= RUNTIMEBITFLAG_TAGGED;
                        }

                        set_parent_cur_tag_entry(tag);
                        break;
                    }
                    case PROPDEF_RENAME:
                    {
                        struct RenameObjectRecord *rename = (struct RenameObjectRecord *) command;
                        struct ObjectRecord *targetObject;
                        s32 targetIndex;

                        targetIndex = rename->TagID + commandIndex;
                        targetObject = setupCommandGetObject(stageId, targetIndex);
                        rename->renobj = targetObject;

                        if (targetObject)
                        {
                            targetObject->runtime_bitflags |= RUNTIMEBITFLAG_DESTROYED;
                        }

                        bondinvAddTextOverride((struct textoverride *) rename);
                        break;
                    }
                    case PROPDEF_WATCH_MENU_OBJECTIVE_TEXT:
                        setup_briefing_text_entry_parent((struct setup_objective_text *) command);
                        break;
                    case PROPDEF_CAMERAPOS:
                    {
                        struct CutsceneRecord *camera = (struct CutsceneRecord *) command;

                        camera->pos.x = *(s32 *) &camera->pos.x / 100.0f;
                        camera->pos.y = *(s32 *) &camera->pos.y / 100.0f;
                        camera->pos.z = *(s32 *) &camera->pos.z / 100.0f;
                        camera->theta = *(s32 *) &camera->theta / M_U16_MAX_VALUE_F;
                        camera->verta = *(s32 *) &camera->verta / M_U16_MAX_VALUE_F;
                        break;
                    }
                    case PROPDEF_OBJECTIVE_START:
                        add_ptr_to_objective((struct objective_entry *) command);
                        break;
                    case PROPDEF_OBJECTIVE_ENTER_ROOM:
                        set_parent_cur_obj_enter_room((struct criteria_roomentered *) command);
                        break;
                    case PROPDEF_OBJECTIVE_DEPOSIT_OBJECT_IN_ROOM:
                        set_parent_cur_obj_deposited_in_room((struct criteria_deposit *) command);
                        break;
                    case PROPDEF_OBJECTIVE_PHOTOGRAPH:
                        set_parent_cur_obj_photograph((struct criteria_picture *) command);
                        break;
                }

                command = (PropDefHeaderRecord *) ((u32 *) command + sizepropdef(command));
                commandIndex++;
            }

            // Second pass: resolve relationships that require both objects to exist.
            command = g_CurrentSetup.propDefs;
            commandIndex = 0;

            while (command->type != PROPDEF_END)
            {
                switch (command->type)
                {
                    case PROPDEF_PROP:
                    case PROPDEF_KEY:
                    case PROPDEF_MAGAZINE:
                    case PROPDEF_COLLECTABLE:
                    case PROPDEF_MONITOR:
                    case PROPDEF_AMMO:
                    case PROPDEF_ARMOUR:
                    case PROPDEF_GAS_RELEASING:
                    case PROPDEF_UNK41:
                    case PROPDEF_GLASS:
                    case PROPDEF_SAFE:
                    case PROPDEF_TINTED_GLASS:
                    {
                        struct ObjectRecord *object = (struct ObjectRecord *) command;

                        if (object->prop && (object->flags & PROPFLAG_INSIDEANOTHEROBJ))
                        {
                            struct ObjectRecord *parent = setupCommandGetObject(stageId, object->pad + commandIndex);

                            if (parent && parent->prop)
                            {
                                object->runtime_bitflags |= RUNTIMEBITFLAG_HASOWNER;
                                modelSetScale(object->model, object->model->scale);
                                chrpropReparent(object->prop, parent->prop);
                            }
                        }
                        break;
                    }
                    case PROPDEF_LINK:
                    {
                        struct LinkRecord *link = (struct LinkRecord *) command;
                        struct WeaponObjRecord *firstWeapon = (struct WeaponObjRecord *) setupGetPtrToCommandByIndex(link->Index1 + commandIndex);
                        struct WeaponObjRecord *secondWeapon = (struct WeaponObjRecord *) setupGetPtrToCommandByIndex(link->Index2 + commandIndex);

                        if (firstWeapon && secondWeapon
                            && (firstWeapon->type == PROPDEF_COLLECTABLE)
                            && (secondWeapon->type == PROPDEF_COLLECTABLE))
                        {
                            propweaponSetDual(firstWeapon, secondWeapon);
                        }

                        break;
                    }
                    case PROPDEF_SWITCH:
                    {
                        struct LinkRecord *link = (struct LinkRecord *) command;
                        struct ObjectRecord *switchObject = setupCommandGetObject(stageId, commandIndex + link->Index1);
                        struct ObjectRecord *door = (struct ObjectRecord *) setupGetPtrToCommandByIndex(commandIndex + link->Index2);

                        if (switchObject && switchObject->prop && door && (door->type == PROPDEF_DOOR) && door->prop)
                        {
                            link->first = switchObject->prop;
                            link->second = door->prop;
                            initSetLevelLoadPropSwitch(link);
                            switchObject->runtime_bitflags |= RUNTIMEBITFLAG_00000001;
                        }

                        break;
                    }
                    case PROPDEF_SAFE_ITEM:
                    {
                        struct SafeObjectRecord *link = (struct SafeObjectRecord *) command;
                        struct ObjectRecord *item;
                        struct SafeRecord *safe;
                        struct DoorRecord *door;

                        item = setupCommandGetObject(stageId, commandIndex + link->Index1);
                        safe = (struct SafeRecord *) setupCommandGetObject(stageId, commandIndex + link->Index2);
                        door = (struct DoorRecord *) setupCommandGetObject(stageId, commandIndex + link->Index3);

                        if (item && item->prop
                            && safe && safe->prop && (safe->type == PROPDEF_SAFE)
                            && door && door->prop && (door->type == PROPDEF_DOOR))
                        {
                            link->item = item;
                            link->safe = safe;
                            link->door = door;
                            initSetLevelLoadPropSafeItem((struct ObjectRecord *) link);
                            item->flags2 |= PROPFLAG2_LINKEDTOSAFE;
                            door->flags2 |= PROPFLAG2_LINKEDTOSAFE;
                        }

                        break;
                    }
                    case PROPDEF_LOCK_DOOR:
                    {
                        struct LockDoorRecord *link = (struct LockDoorRecord *) command;
                        struct DoorRecord *door;
                        struct ObjectRecord *lock;

                        door = (struct DoorRecord *) setupCommandGetObject(stageId, commandIndex + link->Index1);
                        lock = setupCommandGetObject(stageId, commandIndex + link->Index2);

                        if (door && door->prop && (door->type == PROPDEF_DOOR) && lock && lock->prop)
                        {
                            link->door = door;
                            link->lock = lock;
                            initSetLevelLoadPropLockDoor(link);
                            door->runtime_bitflags |= RUNTIMEBITFLAG_PADLOCKEDDOOR;
                        }

                        break;
                    }
                }

                command = (PropDefHeaderRecord *) ((u32 *) command + sizepropdef(command));
                commandIndex++;
            }
        }
    }
    else
    {
        g_CurrentSetup.pathwaypoints = NULL;
        g_CurrentSetup.waypointgroups = NULL;
        g_CurrentSetup.intro = 0;
        g_CurrentSetup.propDefs = 0;
        g_CurrentSetup.patrolpaths = NULL;
        g_CurrentSetup.ailists = NULL;
        g_CurrentSetup.pads = NULL;
        g_CurrentSetup.boundpads = NULL;
        g_CurrentSetup.padnames = NULL;
        g_CurrentSetup.boundpadnames = NULL;
        alloc_init_GUARDdata_entries(0);
        modelmgrAllocateModelSlots(0);
        modelmgrAllocateAnimModelSlots(0);
    }

    allocBackgroundAiChrs();
}
