#include <ultra64.h>
#include <bondtypes.h>
#include "chrai.h"
#include "chrobjdata.h"
#include "propobj.h"
#include "loadobjectmodel.h"
#include "stan.h"
#include "model.h"


s32 getposstan(struct coord3d *pos, StandTile *stan, f32 radius, struct coord3d *posReturn, StandTile **stanReturn)
{
    posReturn->f[0] = pos->f[0];
    posReturn->f[1] = pos->f[1];
    posReturn->f[2] = pos->f[2];

    *stanReturn = stan;

    if (stan == 0)
    {
        return 0;
    }

    // Circle is not valid.
    if ((radius > 0.0f) && (stanTestVolume(stanReturn, posReturn->f[0], posReturn->f[2], radius, CDTYPE_OBJS | CDTYPE_DOORS | CDTYPE_PLAYERS | CDTYPE_CHRS | CDTYPE_PATHBLOCKER, 0.0f, 1.0f) >= 0))
    {
        return 0;
    }

    return 1;
}


/**
 * Get size of Prop Definition
 * @param pdef:  Prop Defenition to get size of
 * @return Size of prop in Words (32bit)
*/
s32 sizepropdef(PropDefHeaderRecord *pdef)
{
    switch (pdef->type)
    {
        case PROPDEF_GUARD:
            return sizeof(GuardRecord) / 4;
        case PROPDEF_DOOR:
            return sizeof(DoorRecord) / 4;
        case PROPDEF_DOOR_SCALE:
            return sizeof(GlobalDoorScaleRecord) / 4;
        case PROPDEF_PROP:
            return sizeof(ObjectRecord) / 4;
        case PROPDEF_GLASS:
            return sizeof(ObjectRecord) / 4;
        case PROPDEF_TINTED_GLASS:
            return sizeof(TintedGlassRecord) / 4;
        case PROPDEF_SAFE:
            return sizeof(ObjectRecord) / 4;
        case PROPDEF_GAS_RELEASING:
            return sizeof(ObjectRecord) / 4;
        case PROPDEF_KEY:
            return sizeof(KeyRecord) / 4;
        case PROPDEF_ALARM:
            return sizeof(ObjectRecord) / 4;
        case PROPDEF_CCTV:
            return 0x3b;
        case PROPDEF_MAGAZINE:
            return 0x21;
        case PROPDEF_COLLECTABLE:
            return 0x22;
        case PROPDEF_MONITOR:
            return 0x40;
        case PROPDEF_MULTI_MONITOR:
            return 0x95;
        case PROPDEF_RACK:
            return sizeof(ObjectRecord) / 4;
        case PROPDEF_AUTOGUN:
            return 0x36;
        case PROPDEF_LINK:
            return 3;
        case PROPDEF_HAT:
            return sizeof(ObjectRecord) / 4;
        case PROPDEF_GUARD_ATTRIBUTE:
            return 3;
        case PROPDEF_SWITCH:
            return 4;
        case PROPDEF_SAFE_ITEM:
            return 5;
        case PROPDEF_AMMO:
            return 0x2d;
        case PROPDEF_ARMOUR:
            return 0x22;
        case PROPDEF_TAG:
            return 4;
        case PROPDEF_RENAME:
            return 10;
        case PROPDEF_OBJECTIVE_START:
            return 4;
        case PROPDEF_OBJECTIVE_END:
            return 1;
        case PROPDEF_OBJECTIVE_DESTROY_OBJECT:
            return 2;
        case PROPDEF_OBJECTIVE_COMPLETE_CONDITION:
            return 2;
        case PROPDEF_OBJECTIVE_FAIL_CONDITION:
            return 2;
        case PROPDEF_OBJECTIVE_COLLECT_OBJECT:
            return 2;
        case PROPDEF_OBJECTIVE_DEPOSIT_OBJECT:
            return 2;
        case PROPDEF_OBJECTIVE_PHOTOGRAPH:
            return 4;
        case PROPDEF_OBJECTIVE_NULL:
            return 1;
        case PROPDEF_OBJECTIVE_ENTER_ROOM:
            return 4;
        case PROPDEF_OBJECTIVE_DEPOSIT_OBJECT_IN_ROOM:
            return 5;
        case PROPDEF_OBJECTIVE_COPY_ITEM:
            return 1;
        case PROPDEF_WATCH_MENU_OBJECTIVE_TEXT:
            return 4;
        case PROPDEF_LOCK_DOOR:
            return 4;
        case PROPDEF_VEHICLE:
            return 0x2c;
        case PROPDEF_AIRCRAFT:
            return 0x2d;
        case PROPDEF_TANK:
            return 0x38;
        case PROPDEF_CAMERAPOS:
            return 7;
        default:
            return sizeof(PropDefHeaderRecord) / 4;
    }
}


/*
 *Return Item by Setup index
 *Setup Array is most likley PropDefHeaderRecord since size was 4
 */
ObjectRecord *setupGetPtrToCommandByIndex(s32 index) //#MATCH
{
    PropDefHeaderRecord *object = g_CurrentSetup.propDefs; //wow, first use of header, cool

    if (index >= 0 && object)
    {
        s32 i;
        for (i = 0; object->type != PROPDEF_END; i++)
        {
            if (i == index)
            {
                return object;
            }

            object = sizepropdef(object) + object; //This is correct order, using += swaps t7/s1
        }
    }

    return NULL;
}


s32 tagGetCommandIndex(struct ObjectRecord *tag)
{
    PropDefHeaderRecord *object;
    s32 i;

    object = g_CurrentSetup.propDefs;

    if (object != NULL)
    {
        for (i = 0; object->type != PROPDEF_END; i++)
        {
            if ((void*)object == (void*)tag)
            {
                return i;
            }

            object = sizepropdef(object) + object;
        }
    }

    return -1;
}


s32 setupGetCommandIndexByProp(struct PropRecord *prop)
{
    PropDefHeaderRecord *object;
    s32 i;

    object = g_CurrentSetup.propDefs;

    if (object != NULL)
    {
        for (i = 0; object->type != PROPDEF_END; i++)
        {
            if ((void*)((struct ObjectRecord *)object)->prop == (void*)prop)
            {
                return i;
            }

            object = sizepropdef(object) + object;
        }
    }

    return -1;
}


s32 modelLoad(s32 modelid)
{
    if (PitemZ_entries[modelid].header->RootNode == NULL)
    {
        fileLoad(PitemZ_entries[modelid].header,PitemZ_entries[modelid].filename);
        modelCalculateRwDataLen(PitemZ_entries[modelid].header);
        return TRUE;
    }

    return FALSE;
}


void setupUpdateObjectRoomPosition(ObjectRecord *obj)
{
    PropRecord *prop;
    struct coord3d bbmin;
    struct coord3d bbmax;
    struct ModelRoData_BoundingBoxRecord *bbox;
    f32 phi_f20;

    prop = obj->prop;
    phi_f20 = 0.0f;

    chrpropDeregisterRooms(prop);

    if (obj->flags2 & PROPFLAG2_USESTANROOM)
    {
        if (prop->stan != NULL)
        {
            prop->rooms[0] = prop->stan->room;
            prop->rooms[1] = (u8)-1;
        }
        else
        {
            prop->rooms[0] = (u8)-1;
        }
    }
    else
    {
        bbox = chrobjGetBboxFromObjectRecord(obj);

        if (bbox != NULL)
        {
            bbmin.f[0] = chrpropSumMatrixPosX(bbox, &obj->mtx) - 30.0f;
            bbmin.f[1] = chrpropSumMatrixPosY(bbox, &obj->mtx);
            bbmin.f[2] = chrpropSumMatrixPosZ(bbox, &obj->mtx) - 30.0f;

            bbmax.f[0] = chrpropSumMatrixNegX(bbox, &obj->mtx) + 30.0f;
            bbmax.f[1] = chrpropSumMatrixNegY(bbox, &obj->mtx);
            bbmax.f[2] = chrpropSumMatrixNegZ(bbox, &obj->mtx) + 30.0f;

            if (phi_f20 < -bbmin.f[0])
            {
                phi_f20 = -bbmin.f[0];
            }

            if (phi_f20 < -bbmin.f[2])
            {
                phi_f20 = -bbmin.f[2];
            }

            if (phi_f20 < bbmax.f[0])
            {
                phi_f20 = bbmax.f[0];
            }

            if (phi_f20 < bbmax.f[2])
            {
                phi_f20 = bbmax.f[2];
            }

            bbmin.f[0] += obj->position.f[0];
            bbmin.f[1] += obj->position.f[1];
            bbmin.f[2] += obj->position.f[2];

            bbmax.f[0] += obj->position.f[0];
            bbmax.f[1] += obj->position.f[1];
            bbmax.f[2] += obj->position.f[2];

            chrpropUpdateRoomList(prop, &bbmin, &bbmax, phi_f20);
        }
    }

    chrpropRegisterRooms(prop);
}


ObjectRecord *setupCommandGetObject(s32 stageID, s32 index)
{
    PropDefHeaderRecord *obj;

    obj = setupGetPtrToCommandByIndex(index);

    if (obj != NULL)
    {
        switch (obj->type)
        {
            case PROPDEF_DOOR:
            case PROPDEF_PROP:
            case PROPDEF_KEY:
            case PROPDEF_ALARM:
            case PROPDEF_CCTV:
            case PROPDEF_MAGAZINE:
            case PROPDEF_COLLECTABLE:
            case PROPDEF_MONITOR:
            case PROPDEF_MULTI_MONITOR:
            case PROPDEF_RACK:
            case PROPDEF_AUTOGUN:
            case PROPDEF_HAT:
            case PROPDEF_AMMO:
            case PROPDEF_ARMOUR:
            case PROPDEF_GAS_RELEASING:
            case PROPDEF_VEHICLE:
            case PROPDEF_AIRCRAFT:
            case PROPDEF_UNK41:
            case PROPDEF_GLASS:
            case PROPDEF_SAFE:
            case PROPDEF_TANK:
            case PROPDEF_TINTED_GLASS:
                return obj;
            break;

            case PROPDEF_DOOR_SCALE:
            case PROPDEF_GUARD:
            case PROPDEF_LINK:
            case PROPDEF_GUARD_ATTRIBUTE:
            case PROPDEF_SWITCH:
            case PROPDEF_TAG:
            case PROPDEF_OBJECTIVE_START:
            case PROPDEF_OBJECTIVE_END:
            case PROPDEF_OBJECTIVE_DESTROY_OBJECT:
            case PROPDEF_OBJECTIVE_COMPLETE_CONDITION:
            case PROPDEF_OBJECTIVE_FAIL_CONDITION:
            case PROPDEF_OBJECTIVE_COLLECT_OBJECT:
            case PROPDEF_OBJECTIVE_DEPOSIT_OBJECT:
            case PROPDEF_OBJECTIVE_PHOTOGRAPH:
            case PROPDEF_OBJECTIVE_NULL:
            case PROPDEF_OBJECTIVE_ENTER_ROOM:
            case PROPDEF_OBJECTIVE_DEPOSIT_OBJECT_IN_ROOM:
            case PROPDEF_OBJECTIVE_COPY_ITEM:
            case PROPDEF_WATCH_MENU_OBJECTIVE_TEXT:
            case PROPDEF_RENAME:
            case PROPDEF_LOCK_DOOR:
            case PROPDEF_SAFE_ITEM:
            case PROPDEF_CAMERAPOS:
                return NULL;
            break;

            // not included:
            //case PROPDEF_DEBRIS: //15:
            //case PROPDEF_UNK16: //16:
            // return ???
        }
    }

    return obj;
}


ObjectRecord *setupFindObjForReuse(s32 wanttype, ObjectRecord **offscreenobjptr, ObjectRecord **anyobjptr, bool musthaveprop, bool musthavemodel, ModelFileHeader *modeldef)
{
    ObjectRecord *offscreenobj = NULL;
    ObjectRecord *anyobj = NULL;

    u32 *cmd = g_CurrentSetup.propDefs;

    if (cmd)
    {
        while ((u8)cmd[0] != PROPDEF_END)
        {
            if ((wanttype & 0xff) == (u8)cmd[0])
            {
                ObjectRecord *obj = (ObjectRecord *)cmd;

                if (obj->prop == NULL)
                {
                    if (!musthaveprop && !musthavemodel)
                    {
                        return obj;
                    }
                }
                else if (wanttype != PROP_TYPE_SMOKE
                        && (obj->runtime_bitflags & RUNTIMEBITFLAG_HASPROJECTILE) == 0
                        && (obj->state & PROPSTATE_RESPAWN) == 0
                        && obj->prop->parent == NULL
                        && (!musthavemodel || modelmgrCanSlotFitRwdata(obj->model, modeldef)))
                {
                    if (offscreenobj == NULL && (obj->prop->flags & PROPFLAG_ONSCREEN) == 0)
                    {
                        offscreenobj = obj;
                    }

                    if (anyobj == NULL)
                    {
                        anyobj = obj;
                    }
                }
            }

            cmd = cmd + sizepropdef(cmd);
        }
    }

    *offscreenobjptr = offscreenobj;
    *anyobjptr = anyobj;

    return NULL;
}

