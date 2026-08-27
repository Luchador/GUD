#include <ultra64.h>
#include <memp.h>
#include "game/mp_weapon.h"
#include "game/bondview_r.h"
#include "bg.h"
#include "bondview_r.h"
#include "chr.h"
#include "chrai.h"
#include "chraction.h"
#include "propobj.h"
#include "inititemslots.h"
#include "initobjects.h"
#include "initpathtablesomething.h"
#include "limits.h"
#include "loadobjectmodel.h"
#include "language.h"
#include "math_atan2f.h"
#include "matrixmath.h"
#include "mp_weapon.h"
#include "ob.h"
#include "objective.h"
#include "objective_status.h"
#include "objecthandler.h"
#include "player.h"
#include "prop.h"
#include "stan.h"
#include "model.h"
#include "token.h"


ITEM_IDS lastmpweaponnum;

extern f32 g_DoorScale;

// redeclare with the element count so ARRAYCOUNT works in proplvreset2
extern ItemModelFileRecord PitemZ_entries[341];

// Begin forward declarations.

s32 load_proptype(PROPDEF_TYPE type);
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


s32 load_proptype(PROPDEF_TYPE type)
{
    PropDefHeaderRecord *propdef = (PropDefHeaderRecord *) g_CurrentSetup.propDefs;
    s32 count = 0;

    if (propdef != NULL)
    {
        while (propdef->type != PROPDEF_END)
        {
            if (propdef->type == (type & 0xFF))
            {
                count ++;
            }
            propdef = &propdef[sizepropdef((PropDefHeaderRecord* ) propdef)];
        }
    }
    return count;
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
        #ifdef DEBUG
        else
        {
            osSyncPrintf("domakedefaultobj: no chr number %d for obj number %d!\n",objectRecord->pad,cmdindex + 1);
        }
        #endif
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

                    if (!(objectRecord->flags & PROPFLAG_ONSCREEN) && !(objectRecord->flags & PROPFLAG_00001000))
                    {
                        // removed
                        #ifdef DEBUG
                            osSyncPrintf("object number %d not positioned correctly!\n",cmdindex + 1);
                        #endif
                    }
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

                    if ((sp58 <= 0.000001f) || (sp54 <= 0.000001f) || (sp50 <= 0.000001f))
                    {
                        #ifdef DEBUG
                        osSyncPrintf("Scale warning: object number %d has a small scale: %f,%f,%f\n",cmdindex +1, sp58,sp54,sp50);
                        #endif
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
        #ifdef DEBUG
        else
        {
            osSyncPrintf("domakedefaultobj: prop obj number %d not reset!\n",cmdindex + 1);
        }
        #endif
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
        #ifdef DEBUG
        else
        {
            osSyncPrintf("domakeweaponobj: no chr number %d for obj number %d!\n",weapon->pad, cmdindex + 1);
        }
        #endif
    }
    else
    {
        hastoken = 1;
        giveweapon = 1;

        if (getPlayerCount() >= 2)
        {
            lastmpweaponnum = -1;

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
                    lastmpweaponnum = temp_a0;

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
    if (hat->flags & PROPFLAG_ASSIGNEDTOCHR) {
        ChrRecord* chr = chrFindByLiteralId(hat->pad);
        if (chr && chr->prop && chr->model) {
            hatAssignToChr(hat, chr);
        }
        #ifdef DEBUG
        else
        {
            osSyncPrintf("domakehatobj: no chr number %d for obj number %d!\n",hat->pad, cmdindex + 1);
        }
        #endif
    } else {
        domakedefaultobj(arg0, hat, cmdindex);
    }
}

//i should be object key
void setupKey(s32 arg0, ObjectRecord* key, s32 cmdindex)
{
    domakedefaultobj(arg0, key, cmdindex);
}


/**
 * NTSC address 0x7F002A3C.
*/
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
 * 
 * PD equivalent: setupCreateDoor.
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


// Perfect Dark void setupLoadFiles(s32 stagenum)
void proplvreset2(enum LEVELID stageId)
{
    ItemModelFileRecord *pitem;
    s32 withchrs;
    s32 withobjs;

    withchrs = (((void *) tokenFind(1, "-nochr")) == NULL) && (((void *) tokenFind(1, "-noprop")) == NULL);
    withobjs = (((void *) tokenFind(1, "-noobj")) == NULL) && (((void *) tokenFind(1, "-noprop")) == NULL);

    g_DoorScale = 1.0f;

    /**
     * Mark every prop model as "not resident" so the model loads later in this function actually fetch data. Essentially
     * RootNode is doubling as a loaded flag for function modelLoad().
     * 
     * The last entry in the PitemZ_entries table is a terminator which is why 1 is subtracted from the loop length.
     */
    for (pitem = PitemZ_entries; pitem < &PitemZ_entries[ARRAYCOUNT(PitemZ_entries) - 1]; pitem++)
    {
        pitem->header->RootNode = NULL;
    }

    if ((stageId <= (LEVELID_MAX + 1)) && setup_text_pointers[stageId])
    {
        char strResource[0x100] = ""; // Scratch buffer for synthesizing the setup file's name at runtime.
        s32 numAnimatedObjects = 0;
        s32 numObjects = 0;
        s32 i1 = 0;
        s32 i2 = 0;
        s32 i3 = 0;
        f32 roompos_1;
        s32 i5 = 0;
        s32 i8;
        f32 roompos_2;
        struct stagesetup *local_stage;

        strResource[0] = setup_text_pointers[stageId][0]; // 'U' -> "U"
        strResource[1] = 0; // Terminate so strcat has a valid string.

        /**
         * There are no slots for the mp stages in setup_text_pointers. The name is created
         * by adding "mp_" after the "U" e.g. "Ump_setuparchZ"
         */
        if (getPlayerCount() >= 2)
        {
            strcat(strResource, "mp_"); // -> "Ump_"
        }

        strcat(strResource, setup_text_pointers[stageId] + 1); // Add remaining text back U[mp_] + setupxxxZ

        g_ptrStageSetupFile = _fileNameLoadToBank(strResource, FILELOADMETHOD_DEFAULT, 256, MEMPOOL_STAGE);

        local_stage = g_ptrStageSetupFile;
        langLoadToAddr(langGetLangBankIndexFromStagenum(stageId));

        /**
         * The setup file stores every internal reference as a byte offset from the start of the file,
         * so rebase them all onto the RAM copy at local_stage.
         */
        g_CurrentSetup.pathwaypoints = (void *) (((u32) local_stage) + ((u32) local_stage->pathwaypoints));
        g_CurrentSetup.waypointgroups = (void *) (((u32) local_stage) + ((u32) local_stage->waypointgroups));
        g_CurrentSetup.intro = (void *) (((u32) local_stage) + ((u32) local_stage->intro));
        g_CurrentSetup.propDefs = (void *) (((u32) local_stage) + ((u32) local_stage->propDefs));
        g_CurrentSetup.patrolpaths = (void *) (((u32) local_stage) + ((u32) local_stage->patrolpaths));
        g_CurrentSetup.ailists = (void *) (((u32) local_stage) + ((u32) local_stage->ailists));
        g_CurrentSetup.pads = (void *) (((u32) local_stage) + ((u32) local_stage->pads));
        g_CurrentSetup.boundpads = (void *) (((u32) local_stage) + ((u32) local_stage->boundpads));

        // Pad names and bound names are optional. An offset of 0 means absent.
        if (local_stage->padnames != 0)
        {
            g_CurrentSetup.padnames = (void *) (((u32) local_stage) + ((u32) local_stage->padnames));
        }
        else
        {
            g_CurrentSetup.padnames = NULL;
        }

        if (local_stage->boundpadnames != 0)
        {
            g_CurrentSetup.boundpadnames = (void *) (((u32) local_stage) + ((u32) local_stage->boundpadnames));
        }
        else
        {
            g_CurrentSetup.boundpadnames = NULL;
        }

        if (g_CurrentSetup.pathwaypoints)
        {
            for (i1 = 0; g_CurrentSetup.pathwaypoints[i1].padID >= 0; i1++)
            {
                g_CurrentSetup.pathwaypoints[i1].neighbours = (void *) (((u32) g_CurrentSetup.pathwaypoints[i1].neighbours) + ((u32) local_stage));
            }
        }

        if (g_CurrentSetup.waypointgroups)
        {
            for (i2 = 0; g_CurrentSetup.waypointgroups[i2].neighbours; i2++)
            {
                g_CurrentSetup.waypointgroups[i2].neighbours = (void *) (((u32) g_CurrentSetup.waypointgroups[i2].neighbours) + ((u32) local_stage));
                g_CurrentSetup.waypointgroups[i2].waypoints = (void *) (((u32) g_CurrentSetup.waypointgroups[i2].waypoints) + ((u32) local_stage));
            }
        }

        // Convert ailist pointers a.k.a. Action Blocks from file-local to proper pointers
        {
            AIListRecord *ailists = g_CurrentSetup.ailists;
            if (ailists)
            {
                for (i3 = 0; g_CurrentSetup.ailists[i3].ailist != 0; i3++)
                {
                    g_CurrentSetup.ailists[i3].ailist = (void *) (((u32) g_CurrentSetup.ailists[i3].ailist) + ((u32) local_stage));
                }
            }
        }

        if (g_CurrentSetup.patrolpaths)
        {
            for (i3 = 0; g_CurrentSetup.patrolpaths[i3].waypoints != NULL; i3++)
            {
                g_CurrentSetup.patrolpaths[i3].waypoints = (void *) (((u32) g_CurrentSetup.patrolpaths[i3].waypoints) + ((u32) local_stage));
                g_CurrentSetup.patrolpaths[i3].len = i5;
            }
        }

        if (g_CurrentSetup.pads)
        {
            struct PadRecord *pad;
    
            roompos_1 = bgGetRoomInverseScale();
            pad = g_CurrentSetup.pads;

            for (; pad->plink != NULL; pad++)
            {
                pad->plink = (void *) (((u32) local_stage) + ((u32) pad->plink));
                pad->pos.f[0] *= roompos_1;
                pad->pos.f[1] *= roompos_1;
                pad->pos.f[2] *= roompos_1;
        
                padAssignStanTile(pad, pad->plink, &pad->stan);
            }
        }

        if (g_CurrentSetup.boundpads)
        {
            struct BoundPadRecord *vol;

            roompos_2 = bgGetRoomInverseScale();
            vol = g_CurrentSetup.boundpads;
            
            for (; vol->plink != NULL; vol++)
            {
                /** Ugly matching hack. 
                *   TODO: investigate if there's a way to get rid of this.
                */
                if ((((u32) local_stage) ^ 0) + ((u32)vol->plink));

                vol->plink = (void *) (((u32) local_stage) + ((u32)vol->plink));
                vol->pos.f[0] *= roompos_2;
                vol->pos.f[1] *= roompos_2;
                vol->pos.f[2] *= roompos_2;
                vol->bbox.xmin *= roompos_2;
                vol->bbox.xmax *= roompos_2;
                vol->bbox.ymin *= roompos_2;
                vol->bbox.ymax *= roompos_2;
                vol->bbox.zmin *= roompos_2;
                vol->bbox.zmax *= roompos_2;

                padAssignStanTile((struct PadRecord *) vol, vol->plink, &vol->stan);
            }
        }

        if (g_CurrentSetup.padnames)
        {
            for (i1 = 0; g_CurrentSetup.padnames[i1].p; i1++)
            {
                g_CurrentSetup.padnames[i1].p = (void *) (((u32) g_CurrentSetup.padnames[i1].p) + ((u32) local_stage));
            }
        }

        if (g_CurrentSetup.boundpadnames)
        {
            for (i1 = 0; g_CurrentSetup.boundpadnames[i1].p; i1++)
            {
                g_CurrentSetup.boundpadnames[i1].p = (void *) (((u32) g_CurrentSetup.boundpadnames[i1].p) + ((u32) local_stage));
            }
        }

        if (withchrs)
        {
            alloc_init_GUARDdata_entries(load_proptype(PROPDEF_GUARD));
            numAnimatedObjects += load_proptype(PROPDEF_GUARD);
            numObjects += load_proptype(PROPDEF_COLLECTABLE);
            numObjects += load_proptype(PROPDEF_KEY);
            numObjects += load_proptype(PROPDEF_HAT);
        }
        else
        {
            alloc_init_GUARDdata_entries(0); // chrmgrConfigure
        }

        if (withobjs)
        {
            // load std props for all stages
            numObjects += load_proptype(PROPDEF_DOOR);
            numObjects += load_proptype(PROPDEF_CCTV);
            numObjects += load_proptype(PROPDEF_AUTOGUN);
            numObjects += load_proptype(PROPDEF_RACK);
            numObjects += load_proptype(PROPDEF_MONITOR);
            numObjects += load_proptype(PROPDEF_MULTI_MONITOR);
            numObjects += load_proptype(PROPDEF_ARMOUR);
            numObjects += load_proptype(PROPDEF_PROP);
            numObjects += load_proptype(PROPDEF_GLASS);
            numObjects += load_proptype(PROPDEF_TINTED_GLASS);
            numObjects += load_proptype(PROPDEF_SAFE);
            numObjects += load_proptype(PROPDEF_UNK41);
            numObjects += load_proptype(PROPDEF_GAS_RELEASING);
            numObjects += load_proptype(PROPDEF_ALARM);
            numObjects += load_proptype(PROPDEF_MAGAZINE);
            numObjects += load_proptype(PROPDEF_AMMO);
            numObjects += load_proptype(PROPDEF_VEHICLE);
            numObjects += load_proptype(PROPDEF_TANK);
            numAnimatedObjects += load_proptype(PROPDEF_AIRCRAFT);
        }

        modelmgrAllocateModelSlots(numObjects);
        modelmgrAllocateAnimModelSlots(numAnimatedObjects);

        for (i8 = 0; i8 < getPlayerCount(); i8++)
        {
            set_cur_player(i8);
            alloc_additional_item_slots(load_proptype(PROPDEF_LINK));
        }

        if (g_CurrentSetup.propDefs)
        {
            PropDefHeaderRecord *phead;
            s32 flags;
            s32 pdefIndex;

            // per-difficulty "Don't Load" mask: PROPFLAG2_00000010/20/40 for Agent/Secret/00
            flags = 1 << (lvGetSelectedDifficulty() + 4);

            /**
             * Complete the skip loading mask started on the line above. Checks for:
             * - don't load on 2 players
             * - don't load on 3 players
             * - don't load on 4 players
             * - don't load in multiplayer
             */
            if (getPlayerCount() >= 2)
            {
                flags |= 1 << (getPlayerCount() + 20);
            }

            phead = g_CurrentSetup.propDefs;
            pdefIndex = 0;

            while ((i1 = phead->type) != PROPDEF_END)
            {
                switch (phead->type)
                {
                    case PROPDEF_GUARD_ATTRIBUTE:
                    {
                        GuardAttributeRecord *pdef_guarda;
                        u8 prob;
                        ChrRecord *chr;
                        pdef_guarda = (GuardAttributeRecord *) phead;
                        prob = (u8) pdef_guarda->GrenadeProb;
                        chr = chrFindByLiteralId(pdef_guarda->chrnum);
                        if ((chr && chr->prop) && chr->model)
                        {
                            chr->grenadeprob = prob;
                        }
#ifdef DEBUG
                        else
                        {
                            osSyncPrintf("grenade prob: no chr number %d for obj number %d! ", pdef_guarda->GrenadeProb, pdefIndex + 1);
                        }
#endif
                        break;
                    }
                    case PROPDEF_GUARD:
                        if (withchrs)
                        {
                            expand_09_characters(stageId, (struct GuardRecord *) phead, pdefIndex);
                        }
                        break;
                    case PROPDEF_DOOR:
                        if (withobjs && (!(((struct DoorRecord *) phead)->flags2 & flags)))
                        {
                            setupDoor((struct DoorRecord *) phead, pdefIndex);
                        }
                        break;
                    case PROPDEF_DOOR_SCALE:
                        g_DoorScale = ((struct GlobalDoorScaleRecord *) phead)->Scale / M_U16_MAX_VALUE_F;
                        break;
                    case PROPDEF_COLLECTABLE:
                        if (withchrs && (!(((struct WeaponObjRecord *) phead)->flags2 & flags)))
                        {
                            weaponAssignToHome(stageId, (struct WeaponObjRecord *) phead, pdefIndex);
                        }
                        break;
                    case PROPDEF_KEY:
                        if (withchrs && (!(((struct KeyRecord *) phead)->flags2 & flags)))
                        {
                            setupKey(stageId, (struct ObjectRecord *) phead, pdefIndex);
                        }
                        break;
                    case PROPDEF_HAT:
                        if (withchrs && (!(((struct ObjectRecord *) phead)->flags2 & flags)))
                        {
                            setupHat(stageId, (struct ObjectRecord *) phead, pdefIndex);
                        }
                        break;
                    case PROPDEF_CCTV:
                        if (withobjs && (!(((struct CCTVRecord *) phead)->flags2 & flags)))
                        {
                            setupCctv(stageId, (struct CCTVRecord *) phead, pdefIndex);
                        }
                        break;
                    case PROPDEF_AUTOGUN:
                        if (withobjs && (!(((struct AutogunRecord *) phead)->flags2 & flags)))
                        {
                            setupAutogun(stageId, (struct AutogunRecord *) phead, pdefIndex);
                        }
                        break;
                    case PROPDEF_RACK:
                        if (withobjs && (!(((struct ObjectRecord *) phead)->flags2 & flags)))
                        {
                            setupHangingMonitors(stageId, (struct ObjectRecord *) phead, pdefIndex);
                        }
                        break;
                    case PROPDEF_MONITOR:
                        if (withobjs && (!(((struct MonitorObjRecord *) phead)->flags2 & flags)))
                        {
                            setupSingleMonitor(stageId, (struct MonitorObjRecord *) phead, pdefIndex);
                        }
                        break;
                    case PROPDEF_MULTI_MONITOR:
                        if (withobjs && (!(((struct MultiMonitorObjRecord *) phead)->flags2 & flags)))
                        {
                            setupMultiMonitor(stageId, (struct MultiMonitorObjRecord *) phead, pdefIndex);
                        }
                        break;
                    case PROPDEF_ARMOUR:
                    {
                        struct BodyArmourRecord *pdef_ba = (struct BodyArmourRecord *) phead;
#ifndef VERSION_US
                        if (withobjs && (((pdef_ba->flags2 & flags) == 0) || j_text_trigger)) // JP: armour setup also proceeds when j_text_trigger is set
#else
                        if (withobjs && ((pdef_ba->flags2 & flags) == 0))
#endif
                        {
                            pdef_ba->initialamount = (*((s32 *) (&pdef_ba->initialamount))) / M_U16_MAX_VALUE_F;
                            pdef_ba->amount = pdef_ba->initialamount;
                            domakedefaultobj(stageId, (struct ObjectRecord *) phead, pdefIndex);
                        }
                        break;
                    }
                    case PROPDEF_TINTED_GLASS:
                    {
                        if (withobjs && (!(((struct TintedGlassRecord *) phead)->flags2 & flags)))
                        {
                            if (((struct TintedGlassRecord *) phead)->flags & PROPFLAG_GLASS_HASPORTAL)
                            {
                                if (!(((struct TintedGlassRecord *) phead)->pad < 10000))
                                {
                                    struct coord3d up;
                                    struct coord3d up2;
                                    BoundPadRecord *pad3d;

                                    pad3d = &g_CurrentSetup.boundpads[((struct TintedGlassRecord *) phead)->pad - 10000];
                                    padGetCenter(pad3d, &up);
                                    up2.x = (10.0f * pad3d->up.x) + up.x;
                                    up2.y = (10.0f * pad3d->up.y) + up.y;
                                    up2.z = (10.0f * pad3d->up.z) + up.z;
                                    up.x -= 10.0f * pad3d->up.x;
                                    up.y -= 10.0f * pad3d->up.y;
                                    up.z -= 10.0f * pad3d->up.z;

                                    ((struct TintedGlassRecord *) phead)->portalnum = bgFindPortalCrossedByLine(&up, &up2);
                                    ((struct TintedGlassRecord *) phead)->unk90 = (*((s32 *) (&((struct TintedGlassRecord *) phead)->unk90))) / M_U16_MAX_VALUE_F;
                                }
                            }
                            domakedefaultobj(stageId, (struct ObjectRecord *) phead, pdefIndex);
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
                        if (withobjs && (!(((ObjectRecord *) phead)->flags2 & flags)))
                        {
                            domakedefaultobj(stageId, (struct ObjectRecord *) phead, pdefIndex);
                        }
                        break;
                    case PROPDEF_AMMO:
                    {
                        struct MultiAmmoCrateRecord *pdef_macr = (struct MultiAmmoCrateRecord *) phead;
                        s32 ammoqty = 1;
                        s32 i9;

                        if (getPlayerCount() >= 2)
                        {
                            struct s_mp_weapon_set *mpweapon = &getPtrMPWeaponSetData()[lastmpweaponnum];
                            
                            ammoqty = mpweapon->ammoamount;
                            if (mpweapon->ammotype);
                            pdef_macr->slots[mpweapon->ammotype - 1].quantity = ammoqty;
                        }

                        if (((ammoqty > 0) && withobjs) && (!(pdef_macr->flags2 & flags)))
                        {
                            for (i9 = 0; i9 < AMMOTYPE_GLOBAL_MAX; i9++)
                            {
                                if ((pdef_macr->slots[i9].quantity > 0) && (pdef_macr->slots[i9].modelnum != 0xFFFF))
                                {
                                    modelLoad(pdef_macr->slots[i9].modelnum);
                                }
                            }

                            domakedefaultobj(stageId, (struct ObjectRecord *) pdef_macr, pdefIndex);
                        }
                        break;
                    }
                    case PROPDEF_TANK:
                        if (withobjs && (!(((struct TankRecord *) phead)->flags2 & flags)))
                        {
                            struct TankRecord *pdef_tank = (struct TankRecord *) phead;
                            struct PropRecord *tank_prop;

                            s32 padding;
                            f32 stan_y = 0.0f;
                            s32 paddinggg[4];

                            weaponLoadProjectileModels(ITEM_TANKSHELLS);
                            domakedefaultobj(stageId, (struct ObjectRecord *) pdef_tank, pdefIndex);
                            pdef_tank->turret_vertical_angle = 0.0f;
                            pdef_tank->turret_orientation_angle = 0.0f;
                            pdef_tank->tank_orientation_angle = M_TAU_F - atan2f(pdef_tank->mtx.m[2][0], pdef_tank->mtx.m[2][2]);
                            tank_prop = pdef_tank->prop;

                            if (tank_prop)
                            {
                                stan_y = stanGetPositionYValue(tank_prop->stan, tank_prop->pos.f[0], tank_prop->pos.f[2]);
                            }

                            pdef_tank->stan_y = stan_y;
#ifdef VERSION_EU
                            pdef_tank->unkD0 = stan_y / 0.2004f; // EU-tuned constant
#else
                            pdef_tank->unkD0 = stan_y / 0.17000002f;
#endif
                        }
                        break;
                    case PROPDEF_VEHICLE:
                        if (withobjs && (!(((struct VehichleRecord *) phead)->flags2 & flags)))
                        {
                            struct VehichleRecord *pdef_veh = (struct VehichleRecord *) phead;

                            domakedefaultobj(stageId, (struct ObjectRecord *) pdef_veh, pdefIndex);

                            if (pdef_veh->model != NULL)
                            {
                                if (pdef_veh->model->obj->Switches[5] != NULL)
                                {
                                    modelGetNodeRwData(pdef_veh->model, pdef_veh->model->obj->Switches[5])->Raw.unk00 = (pdef_veh->flags & 0x10000000) == 0;
                                }
                            }

                            pdef_veh->speed        = 0.0f;
                            pdef_veh->wheelxrot    = 0.0f;
                            pdef_veh->wheelyrot    = 0.0f;
                            pdef_veh->speedaim     = 0.0f;
                            pdef_veh->turnrot60    = 0.0f;
                            pdef_veh->roty         = 0.0f;
                            pdef_veh->speedtime60  = -1.0f;
                            pdef_veh->ailist       = ailistFindById(pdef_veh->ailist);
                            pdef_veh->aioffset     = 0;
                            pdef_veh->aireturnlist = -1;
                            pdef_veh->path         = 0;
                            pdef_veh->nextstep     = 0;
                            pdef_veh->Sound        = 0;
                        }
                        break;
                    case PROPDEF_AIRCRAFT:
                        if (withobjs && (!(((struct AircraftRecord *) phead)->flags2 & flags)))
                        {
                            struct AircraftRecord *pdef_air = (struct AircraftRecord *) phead;

                            domakedefaultobj(stageId, (struct ObjectRecord *) pdef_air, pdefIndex);
                            pdef_air->speed           = 0.0f;
                            pdef_air->speedaim        = 0.0f;
                            pdef_air->rotoryrot       = 0.0f;
                            pdef_air->rotaryspeed     = 0.0f;
                            pdef_air->rotaryspeedaim  = 0.0f;
                            pdef_air->yrot            = 0.0f;
                            pdef_air->speedtime60     = -1.0f;
                            pdef_air->rotaryspeedtime = -1.0f;
                            pdef_air->ailist          = ailistFindById(pdef_air->ailist);
                            pdef_air->aioffset        = 0;
                            pdef_air->aireturnlist    = -1;
                            pdef_air->nextstep        = 0;
                            pdef_air->path            = 0;
                            pdef_air->Sound           = 0;
                        }
                        break;
                    case PROPDEF_TAG:
                    {
                        struct TagObjectRecord *pdef_tag;
                        struct ObjectRecord *taggedobj;

                        pdef_tag = (struct TagObjectRecord *) phead;
                        taggedobj = setupCommandGetObject(stageId, pdefIndex + ((s32) pdef_tag->OffsetToObj));
                        pdef_tag->TaggedObject = taggedobj;

                        if (taggedobj)
                        {
                            taggedobj->runtime_bitflags |= RUNTIMEBITFLAG_TAGGED;
                        }

                        set_parent_cur_tag_entry(pdef_tag);
                        break;
                    }
                    case PROPDEF_RENAME:
                    {
                        struct RenameObjectRecord *pdef_ren;
                        struct ObjectRecord *targetobj;

                        pdef_ren = (struct RenameObjectRecord *) phead;
                        i3 = pdef_ren->TagID + pdefIndex;
                        targetobj = setupCommandGetObject(stageId, i3);
                        pdef_ren->renobj = targetobj;

                        if (targetobj)
                        {
                            targetobj->runtime_bitflags |= RUNTIMEBITFLAG_DESTROYED;
                        }

                        bondinvAddTextOverride((struct textoverride *) pdef_ren);
                        break;
                    }
                    case PROPDEF_WATCH_MENU_OBJECTIVE_TEXT:
                        setup_briefing_text_entry_parent((struct setup_objective_text *) phead);
                        break;
                    case PROPDEF_CAMERAPOS:
                    {
                        struct CutsceneRecord *pdef_cam = (struct CutsceneRecord *) phead;

                        pdef_cam->pos.f[0] = (*((s32 *) (&pdef_cam->pos.f[0]))) / 100.0f;
                        pdef_cam->pos.f[1] = (*((s32 *) (&pdef_cam->pos.f[1]))) / 100.0f;
                        pdef_cam->pos.f[2] = (*((s32 *) (&pdef_cam->pos.f[2]))) / 100.0f;
                        pdef_cam->theta = (*((s32 *) (&pdef_cam->theta))) / M_U16_MAX_VALUE_F;
                        pdef_cam->verta = (*((s32 *) (&pdef_cam->verta))) / M_U16_MAX_VALUE_F;
                        break;
                    }
                    case PROPDEF_OBJECTIVE_START:
                        add_ptr_to_objective((struct objective_entry *) phead);
                        break;
                    case PROPDEF_OBJECTIVE_ENTER_ROOM:
                        set_parent_cur_obj_enter_room((struct criteria_roomentered *) phead);
                        break;
                    case PROPDEF_OBJECTIVE_DEPOSIT_OBJECT_IN_ROOM:
                        set_parent_cur_obj_deposited_in_room((struct criteria_deposit *) phead);
                        break;
                    case PROPDEF_OBJECTIVE_PHOTOGRAPH:
                        set_parent_cur_obj_photograph((struct criteria_picture *) phead);
                        break;
                }

                phead = (PropDefHeaderRecord *) (((u32 *) phead) + sizepropdef(phead));
                pdefIndex++;
            }

            phead = g_CurrentSetup.propDefs;
            pdefIndex = 0;

            while (phead->type != PROPDEF_END)
            {
                switch (phead->type)
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
                        struct ObjectRecord *pdef_obj = (struct ObjectRecord *) phead;

                        if (pdef_obj->prop && (pdef_obj->flags & PROPFLAG_INSIDEANOTHEROBJ))
                        {
                            u32 offset = pdef_obj->pad;
                            struct ObjectRecord *inobj = setupCommandGetObject(stageId, offset + pdefIndex);

                            if (inobj && inobj->prop)
                            {
                                pdef_obj->runtime_bitflags |= RUNTIMEBITFLAG_HASOWNER;
                                modelSetScale(pdef_obj->model, pdef_obj->model->scale);
                                chrpropReparent(pdef_obj->prop, inobj->prop);
                            }

#ifdef DEBUG
                            //possibly wrong place
                            else
                            {
                                osSyncPrintf("inobj link not found for object number %d\n", pdefIndex + 1);
                            }
#endif

                        }
                        break;
                    }
                    case PROPDEF_LINK:
                    {
                        struct LinkRecord *pdef_link = (struct LinkRecord *) phead;
                        struct WeaponObjRecord *guna = (struct WeaponObjRecord *) setupGetPtrToCommandByIndex(pdef_link->Index1 + pdefIndex);
                        struct WeaponObjRecord *gunb = (struct WeaponObjRecord *) setupGetPtrToCommandByIndex(pdef_link->Index2 + pdefIndex);

                        if (guna && gunb)
                        {
                            if ((guna->type == PROPDEF_COLLECTABLE) && (gunb->type == PROPDEF_COLLECTABLE))
                            {
                                propweaponSetDual(guna, gunb);
                            }
#ifdef DEBUG
                            else
                            {
                                osSyncPrintf("link type wrong for doublegun object number %d\n", pdefIndex + 1);
                            }
                        }
                        else
                        {
                            osSyncPrintf("link not found for doublegun object number %d\n", pdefIndex + 1);
#endif

                        }

                        break;
                    }
                    case PROPDEF_SWITCH:
                    {
                        struct LinkRecord *pdef_switch;
                        struct ObjectRecord *doorA;
                        struct ObjectRecord *doorB;
                        s32 index1;
                        s32 index2;

                        pdef_switch = (struct LinkRecord *) phead;
                        index1 = pdef_switch->Index1;
                        index2 = pdef_switch->Index2;
                        doorA = (struct ObjectRecord *) setupCommandGetObject(stageId, pdefIndex + index1);
                        doorB = (struct ObjectRecord *) setupGetPtrToCommandByIndex(pdefIndex + index2);

                        if ((((doorA && doorA->prop) && doorB) && (doorB->type == PROPDEF_DOOR)) && doorB->prop)
                        {
                            pdef_switch->first = doorA->prop;
                            pdef_switch->second = doorB->prop;
                            initSetLevelLoadPropSwitch(pdef_switch);
                            doorA->runtime_bitflags |= RUNTIMEBITFLAG_00000001; // linked door
                        }

#ifdef DEBUG
                        else
                        {
                            osSyncPrintf("doorlink object number %d not initialised\n", pdefIndex + 1);
                        }
#endif

                        break;
                    }
                    case PROPDEF_SAFE_ITEM:
                    {
                        s32 index1;
                        struct SafeObjectRecord *pdef_safe;
                        s32 index2;
                        s32 index3;
                        struct ObjectRecord *safe_item;
                        struct SafeRecord *safe;
                        struct DoorRecord *door;

                        pdef_safe = (struct SafeObjectRecord *) phead;
                        index1 = pdef_safe->Index1;
                        index2 = pdef_safe->Index2;
                        index3 = pdef_safe->Index3;
                        safe_item = setupCommandGetObject(stageId, pdefIndex + index1);
                        safe = (struct SafeRecord *) setupCommandGetObject(stageId, pdefIndex + index2);
                        door = (struct DoorRecord *) setupCommandGetObject(stageId, pdefIndex + index3);

                        if (((((((safe_item && safe_item->prop) && safe) && safe->prop) && (safe->type == PROPDEF_SAFE)) && door) && door->prop) && (door->type == PROPDEF_DOOR))
                        {
                            pdef_safe->item = safe_item;
                            pdef_safe->safe = safe;
                            pdef_safe->door = door;
                            initSetLevelLoadPropSafeItem((struct ObjectRecord *) pdef_safe);
                            safe_item->flags2 |= PROPFLAG2_LINKEDTOSAFE;
                            door->flags2 |= PROPFLAG2_LINKEDTOSAFE;
                        }
#ifdef DEBUG
                        else
                        {
                            osSyncPrintf("safelink object number %d not initialised\n", pdefIndex + 1);
                        }
#endif
                        break;
                    }
                    case PROPDEF_LOCK_DOOR:
                    {
                        struct LockDoorRecord *pdef_lock_door;
                        struct DoorRecord *door;
                        struct ObjectRecord *lock;
                        s32 index1;
                        s32 index2;

                        pdef_lock_door = (struct LockDoorRecord *) phead;

                        index1 = pdef_lock_door->Index1;
                        index2 = pdef_lock_door->Index2;

                        door = (struct DoorRecord *) setupCommandGetObject(stageId, pdefIndex + index1);
                        lock = setupCommandGetObject(stageId, pdefIndex + index2);

                        if ((((door && door->prop) && lock) && lock->prop) && (door->type == PROPDEF_DOOR))
                        {
                            pdef_lock_door->door = door;
                            pdef_lock_door->lock = lock;
                            initSetLevelLoadPropLockDoor(pdef_lock_door);
                            door->runtime_bitflags |= RUNTIMEBITFLAG_PADLOCKEDDOOR;
                        }
#ifdef DEBUG
                        else
                        {
                            osSyncPrintf("doorlock object number %d not initialised\n", pdefIndex + 1);
                        }
#endif
                        break;
                    }
                }

                phead = (PropDefHeaderRecord *) (((u32 *) phead) + sizepropdef(phead));
                pdefIndex += 1;
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
