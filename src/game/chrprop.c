#include <ultra64.h>
#include <assert.h>
#include <bondgame.h>
#include <bondtypes.h>
#include <bondaicommands.h>
#include <boss.h>
#include <limits.h>
#include <music.h>
#include <random.h>
#include <snd.h>
#include "bg.h"
#include "bgfog.h"
#include "bondview.h"
#include "cheat.h"
#include "chr.h"
#include "chrai.h"
#include "chraidata.h"
#include "chraction.h"
#include "explosion.h"
#include "file.h"
#include "gun.h"
#include "initanitable.h"
#include "language.h"
#include "loadobjectmodel.h"
#include "lv.h"
#include "math.h"
#include "math_atan2f.h"
#include "math_ceil.h"
#include "math_floor.h"
#include "model.h"
#include "mp_music.h"
#include "player.h"
#include "propobj.h"
#include "objecthandler.h"
#include "objective_status.h"
#include "stan.h"


// bss


//CODE.bss:80069C30
s16 * ptr_list_object_lookup_indices;

//CODE.bss:80069C34 canonically roompropsnum
u32 num_obj_position_data_entries;

/**
 * Address 0x80069C38.
*/
PropRecord pos_data_entry[POS_DATA_ENTRY_LEN];

//CODE.bss:80071618
s16 *RoomPropListBlockIndices;

//CODE.bss:8007161C
struct roomproplistblock *RoomPropListBlocks;

/**
 * Array of pointers, containing onscreen props.
 *
 * Address 0x80071620.
*/
PropRecord *g_OnScreenPropList[ONSCREEN_PROP_LIST_LEN];

/**
 * Pointer to last onscreen prop.
 * Address 0x80071DF0.
*/
PropRecord **g_LastOnScreenProp;

/**
 * Count of onscreen props.
 * Address 0x80071DF4.
 * canonically propznum
*/
s32 g_OnScreenPropCount;

//CODE.bss:80071DF8
PropRecord *g_InteractProp;
//CODE.bss:80071DFC
u32 dword_CODE_bss_80071DFC;
//CODE.bss:80071E00
WeaponObjRecord* proxy_mine_table[30];

//CODE.bss:80071E78
f32 gasTimeToFullOpacity;
//CODE.bss:80071E7C
u32 gasDoesDamageFlag;

/**
 * Address 0x80071E80.
*/
WeaponObjRecord g_WeaponSlots[MAX_WEAPON_SLOTS];

/**
 * Address 0x80072E70.
*/
HatRecord g_HatSlots[MAX_HAT_SLOTS];

/**
 * Address 0x80073370.
*/
AmmoCrateRecord g_AmmoCrates[MAX_AMMO_CRATES];

/**
 * Address 0x80073DC0.
*/
Projectile g_Projectiles[PROJECTILES_ARR_MAX];

/**
 * Address 0x80075030.
*/
Embedment g_Embedments[EMBEDMENT_ARR_MAX];

//CODE.bss:80075B70
struct Model *g_CurrentProjectileModel;
//CODE.bss:80075B74
struct ModelNode * dword_CODE_bss_80075B74;
//CODE.bss:80075B78
coord3d flt_CODE_bss_80075B78;
//CODE.bss:80075B84
f32 flt_CODE_bss_80075B84;
//CODE.bss:80075B88
coord3d flt_CODE_bss_80075B88;
//CODE.bss:80075B94
f32 flt_CODE_bss_80075B94;

/**
 * Address 0x80075B98.
*/
MonitorRecord g_MonitorAnimController;

/**
 * Unused / unreferenced (from padding / align?)
 * Address 0x80075C0C.
*/
s32 bss_80075C0C;

/**
 * Address 0x80075C10.
*/
struct object_animation_controller g_UnknownAnimController;

/**
 * Unused / unreferenced (from padding / align?)
 * Address 0x80075C84.
*/
s32 bss_80075C84;

/**
 * Unused / unreferenced (from padding / align?)
 * Address 0x80075C88.
*/
struct object_animation_controller g_TaserAnimController;

/**
 * Address 0x80075CFC.
*/
s32 bss_80075CFC;



//CODE.bss:80075D00 - 80075D24
stagesetup g_CurrentSetup; //Public Working Setup

//CODE.bss:80075D28
stagesetup                        *g_ptrStageSetupFile;

PropRecord *ptr_obj_pos_list_current_entry = 0;
PropRecord *ptr_obj_pos_list_first_entry = 0;
PropRecord *ptr_obj_pos_list_final_entry = 0;
f32 difficulty = 1.0;
struct coord2d g_DefaultAutoAimCoord = { 0 };



// forward declarations

Gfx *chrpropRender(Gfx *arg0, PropRecord *arg1, s32 withalpha);
void chraiCheckUseHeldItem(s32 hand);
void chraiDefaultWeaponFireHandler(s32);
void chraiFistAttackHandler(s32 hand, s32 item_id);
void modelGetAxisExtents(Model* model, f32* max, f32* min, s32 axis);

// end forward declarations


/**
 * Counts onscreen props.
 *
 * Address 0x7F03A240.
*/
void chraiUpdateOnscreenPropCount(void)
{
    s32 i;
    s32 j;
    s32 count;
    PropRecord *prop;
    s32 phi_a0;
    f32 phi_f12;

    i = 0;
    count = 0;
    prop = get_ptr_obj_pos_list_current_entry();

    for (; prop != NULL; prop = prop->prev)
    {
        if ((prop->flags & (PROPFLAG_ENABLED | PROPFLAG_ONSCREEN)) == (PROPFLAG_ENABLED | PROPFLAG_ONSCREEN))
        {
            g_OnScreenPropList[count] = prop;
            count++;
        }
    }

    g_OnScreenPropCount = count;
    g_OnScreenPropList[count] = NULL;

    if(1)
    {
        // removed
        #ifdef DEBUG
        assert(propznum<MAXPROPSVISIBLE); //800 props
        #endif
    }

    g_LastOnScreenProp = (PropRecord *)&g_OnScreenPropList[count];

    for (i=0; i<count; i++)
    {
        phi_a0 = -1;
        phi_f12 = -4.2949673e9f;

        for (j = i; j < count; j++)
        {
            f32 f = g_OnScreenPropList[j]->zDepth;

            if (phi_f12 < f)
            {
                phi_f12 = f;
                phi_a0 = j;
            }
        }

        if (phi_a0 >= 0)
        {
            prop = g_OnScreenPropList[i];
            g_OnScreenPropList[i] = g_OnScreenPropList[phi_a0];
            g_OnScreenPropList[phi_a0] = prop;
        }
    }
}


void chrpropEnable(PropRecord *prop)
{
    prop->flags |= PROPFLAG_ENABLED;
}



void chrpropDisable(PropRecord *prop)
{
    prop->flags &= ~PROPFLAG_ENABLED;
}


PropRecord *get_ptr_obj_pos_list_current_entry(void)
{
    return ptr_obj_pos_list_current_entry;
}


PropRecord* chrpropAllocate(void)
{
    PropRecord* prop;

    if (ptr_obj_pos_list_final_entry)
    {
        prop = ptr_obj_pos_list_final_entry;
        ptr_obj_pos_list_final_entry = prop->prev;

        prop->prev = NULL;
        prop->next = NULL;
        prop->parent = NULL;
        prop->child = NULL;

        prop->flags = 0;
        prop->stan = NULL;
        prop->timetoregen = 0;
        prop->rooms[0] = 0xFF;
        return prop;
    }

    return NULL;
}


void chrpropFree(PropRecord *prop)
{
    prop->prev = ptr_obj_pos_list_final_entry;
    prop->next = 0x0;
    prop->stan = 0x0;
    ptr_obj_pos_list_final_entry = prop;
}


void chrpropActivate(PropRecord* prop)
{
    PropRecord* cur;

    cur = ptr_obj_pos_list_current_entry;
    if (cur != NULL)
    {
        cur->next = prop;

        prop->prev = ptr_obj_pos_list_current_entry;
        prop->next = NULL;

        ptr_obj_pos_list_current_entry = prop;
        return;
    }

    prop->prev = NULL;
    prop->next = NULL;
    ptr_obj_pos_list_current_entry = ptr_obj_pos_list_first_entry = prop;
}


void chrpropActivateThisFrame(PropRecord* prop) {
    PropRecord* first;

    first = ptr_obj_pos_list_first_entry;
    if (first != NULL) {
        first->prev = prop;
        prop->next = ptr_obj_pos_list_first_entry;
        prop->prev = NULL;
        ptr_obj_pos_list_first_entry = prop;
        return;
    }
    prop->prev = NULL;
    prop->next = NULL;
    ptr_obj_pos_list_first_entry = prop;
    ptr_obj_pos_list_current_entry = prop;
}


void chrpropDelist(PropRecord *prop)
{
    PropRecord *temp_v0;
    PropRecord *temp_v0_2;

    if (prop == ptr_obj_pos_list_current_entry)
    {
        ptr_obj_pos_list_current_entry = prop->prev;
    }
    if (prop == ptr_obj_pos_list_first_entry)
    {
        ptr_obj_pos_list_first_entry = prop->next;
    }
    temp_v0 = prop->prev;
    if (temp_v0 != 0)
    {
        temp_v0->next = prop->next;
    }
    temp_v0_2 = prop->next;
    if (temp_v0_2 != 0)
    {
        temp_v0_2->prev = prop->prev;
    }
    prop->prev = NULL;
    prop->next = NULL;
}


void chrpropReparent(PropRecord *newChild, PropRecord *host)
{
    newChild->parent = host;

    // Link the newChild into its siblings
    if (host->child)
    {
        host->child->next = newChild;
    }
    newChild->prev = host->child;
    newChild->next = NULL;
    newChild->stan = NULL;
    host->child    = newChild;

}


void chrpropDetach(PropRecord* prop) {
    PropRecord* parent;
    PropRecord* prev;
    PropRecord* next;

    parent = prop->parent;
    if (parent) {
        if (prop == parent->child) {
            parent->child = prop->prev;
        }
        prev = prop->prev;
        if (prev) {
            prev->next = prop->next;
        }
        next = prop->next;
        if (next) {
            next->prev = prop->prev;
        }
        prop->parent = NULL;
        prop->prev = NULL;
        prop->next = NULL;
    }
}


/**
 * Address: 7F03A62C
*/
Gfx *chrpropRender(Gfx * gdl, PropRecord *prop, s32 withalpha)
{
    u8 type;

    type = prop->type;

    if (type == PROP_TYPE_CHR)
    {
        gdl = chrRenderProp(prop, gdl, withalpha);
    }
    else if ((type == PROP_TYPE_OBJ) || (type == PROP_TYPE_WEAPON) || (type == PROP_TYPE_DOOR))
    {
        gdl = chrobjRenderProp(prop, gdl, withalpha);
    }
    else if (type == PROP_TYPE_EXPLOSION)
    {
        gdl = explosionRenderPropExplosion(prop, gdl, withalpha);
    }
    else if (type == PROP_TYPE_SMOKE)
    {
        gdl = explosionRenderPropSmoke(prop, gdl, withalpha);
    }
    else if (type == PROP_TYPE_VIEWER)
    {
        gdl = bondviewRenderProp(prop, gdl, withalpha);
    }

    return gdl;
}


/**
 * Address: 7F03A6F4
*/
Gfx *chrpropsRenderPass(Gfx *gdl, s32 roomid, s32 renderpass)
{
    s32 flag;
    PropRecord **pp;
    PropRecord *prop;
    s32 i;
    s32* rp;
    s32 unused2;
    s32 sp48[PROPRECORD_STAN_ROOM_LEN];
    s32 unused3;
    s32 unused4;

    if (bossGetStageNum() == LEVELID_CUBA)
    {
        if (renderpass == 0)
        {
            return gdl;
        }
        else if (renderpass == 2)
        {
            renderpass = 0;
        }
    }

    if ((renderpass == 0) || (renderpass == 2))
    {
        for (pp = g_LastOnScreenProp; --pp >= g_OnScreenPropList; )
        {
            prop = *pp;

            if (prop != NULL)
            {
                flag = 0;

                if ((renderpass == 0) && ((prop->flags & (PROPFLAG_00000020 | PROPFLAG_RENDERPOSTBG)) == 0))
                {
                    flag = 1;
                }
                else if ((renderpass == 2) && ((prop->flags & (PROPFLAG_00000020 | PROPFLAG_RENDERPOSTBG)) == PROPFLAG_RENDERPOSTBG))
                {
                    flag = 1;
                }

                if (flag != 0)
                {
                    flag = 0;
                    chraiGetPropRoomIds(prop, sp48);

                    for (rp = sp48; *rp >= 0; rp++)
                    {
                        if (getROOMID_isRendered(*rp))
                        {
                            if (roomid == *rp)
                            {
                                flag = 1;
                            }

                            break;
                        }
                    }

                    if (flag)
                    {
                        gdl = chrpropRender(gdl, prop, 0);
                    }
                }
            }
        }
    }
    else
    {
        for (pp = g_OnScreenPropList; pp < g_LastOnScreenProp; pp++)
        {
            prop = *pp;

            if (prop != NULL)
            {
                flag = 0;
                chraiGetPropRoomIds(prop, sp48);

                for (rp = sp48; *rp >= 0; rp++)
                {
                    if (getROOMID_isRendered(*rp))
                    {
                        if (roomid == *rp)
                        {
                            flag = 1;
                        }

                        break;
                    }
                }

                if (flag)
                {
                    if (prop->flags & PROPFLAG_00000020)
                    {
                        gdl = chrpropRender(gdl, prop, 0);
                    }

                    gdl = chrpropRender(gdl, prop, 1);
                }
            }
        }
    }

    return bgScissorCurrentPlayerViewDefault(gdl);
}


/**
 * Address: 7F03A97C
 *
 * Tests if a ray intersects the bounding box of the given room.
 * @return TRUE if the ray intersects, otherwise FALSE.
*/
s32 chrpropRayIntersectsRoomBbox(s32 room, coord3d* start, coord3d* dir)
{
    s32 max[3];
    s32 min[3];
    s_room_info* roominfo;

    roominfo = &g_BgRoomInfo[room];

    // Skip check if room has no collision data
    if (roominfo->vtx_batch_bounds != NULL)
    {
        min[0] = roominfo->minbounds.f[0];
        min[1] = roominfo->minbounds.f[1];
        min[2] = roominfo->minbounds.f[2];
        max[0] = roominfo->maxbounds.f[0];
        max[1] = roominfo->maxbounds.f[1];
        max[2] = roominfo->maxbounds.f[2];
        if (bgTestLineIntersectsBbox(start, dir, min, max)) {
            return TRUE;
        }
    }
    return FALSE;
}


/**
 * Address: 7F03AA44
 *
 * Unreferenced
 *
 * This takes a list of rooms and flags the ones that do *not* intersect a ray.
 */
void chrpropFlagRoomsFromRayTest(s32 arg0, coord3d *from, coord3d *to, u8 *rooms)
{
    coord3d start;
    coord3d dir;
    f32 scale;
    s32 i;

    scale = get_room_data_float1() * bgGetLevelVisibilityScale();

    dir.x = to->x - from->x;
    dir.y = to->y - from->y;
    dir.z = to->z - from->z;

    start.x = from->x * scale;
    start.y = from->y * scale;
    start.z = from->z * scale;

    for (i = 1; i < getMaxNumRooms(); i++) {
        if (!rooms[i] && chrpropRayIntersectsRoomBbox(i, &start, &dir) == 0) {
            rooms[i] = 1;
        }
    }
}


/**
 * Address: 7F03AB58
 *
 * Refines an existing background bullet hit by checking currently visible rooms
 * that have not already been tested.
 *
 * The function scans the visible room list, marks each tested room in visited,
 * performs a room bbox test first, then tests the room geometry. If no previous hit exists,
 * the first visible room hit is accepted.
 * Otherwise, a hit is accepted only if it lies between from and the current
 * best hit on all three axes, making it closer along the shot ray.
 *
 * @return Returns the room number of the accepted hit, or the bestroom if no
 * closer visible room hit is found.
 */
s32 chrpropFindCloserBgHitInVisibleRooms(coord3d *from, coord3d *to, coord3d *dir, coord3d *scaledDir, u8 *visited, struct HitThing *besthit, s32 bestroom)
{
    s32 rooms[100];
    s32 *roomptr;
    s32 *end;
    s32 numrooms;
    struct HitThing hit;
    f32 scale;
    s32 room;

    scale = get_room_data_float2();

    // Get up to 100 currently visible rooms.
    numrooms = bgCopyVisibleRoomsToList(&rooms[0], 100);

    if (numrooms > 0)
    {
        roomptr = rooms;
        // The bitwise AND is just a matching trick and effectively does nothing.
        end = roomptr + (numrooms & 0xFFFFFFFF);

        do
        {
            // Only check rooms that have not been visited.
            if (visited[*roomptr] == 0)
            {
                visited[*roomptr] = 1;

                if (chrpropRayIntersectsRoomBbox(*roomptr, scaledDir, dir))
                {
                    if (bgTestBulletHitBackground(from, to, *roomptr, &hit))
                    {
                        room = *roomptr;
                        hit.hitpos.x *= scale;
                        hit.hitpos.y *= scale;
                        hit.hitpos.z *= scale;

                        /**
                         * The (numrooms * 0) is weird but harmless and needed for matching.
                         */
                        if ((bestroom <= (numrooms * 0))
                                || (((((from->x <= besthit->hitpos.x)
                                            && (from->x <= hit.hitpos.x))
                                            && (hit.hitpos.x < besthit->hitpos.x))
                                        || (((besthit->hitpos.x <= from->x)
                                            && (hit.hitpos.x <= from->x))
                                            && (besthit->hitpos.x < hit.hitpos.x)))
                                    && ((((from->y <= besthit->hitpos.y)
                                            && (from->y <= hit.hitpos.y))
                                            && (hit.hitpos.y < besthit->hitpos.y))
                                        || (((besthit->hitpos.y <= from->y)
                                            && (hit.hitpos.y <= from->y))
                                            && (besthit->hitpos.y < hit.hitpos.y)))
                                    && ((((from->z <= besthit->hitpos.z)
                                            && (from->z <= hit.hitpos.z))
                                            && (hit.hitpos.z < besthit->hitpos.z))
                                        || (((besthit->hitpos.z <= from->z)
                                            && (hit.hitpos.z <= from->z))
                                            && (besthit->hitpos.z < hit.hitpos.z)))))
                        {
                            bestroom = room;
                            *besthit = hit;
                        }
                    }
                }
            }

            roomptr++;
        }
        while (roomptr < end);

        if (rooms);
    }

    return bestroom;
}


/**
 * Address: 7F03ADF4
 *
 * Beginning at startroom, walk connected rooms looking for a background
 * bullet hit.
 *
 * Rooms are skipped if already marked in visited, and newly processed rooms are
 * marked visited.
 * @return Return 0 if no hit is found, otherwise the room number of the first room whose bbox and
 * background geometry intersect the bullet ray.
 */
s32 chrpropFindFirstBgHitInConnectedRooms(s32 startroom, coord3d *from, coord3d *to, coord3d *dir, coord3d *scaledDir, u8 *visited, struct HitThing *hit)
{
    u8 rooms[256];
    s32 pad;
    s32 neighbours[100];
    s32 numneighbours;
    s32 i;
    s32 j;
    s32 count;
    s32 curindex;
    s32 room;

    rooms[0] = startroom;
    count = 1;

    for (curindex = 0; curindex < count; curindex++) {
        room = rooms[curindex];

        if (visited[room] == 0) {
            visited[room] = 1;

            if (chrpropRayIntersectsRoomBbox(room, scaledDir, dir)) {
                if (bgTestBulletHitBackground(from, to, room, hit)) {
                    return room;
                }
            }
        }

        numneighbours = bgGetConnectedRooms(room, neighbours, 100);

        for (i = 0; i < numneighbours; i++) {
            for (j = 0; j < count; j++) {
                if (rooms[j] == neighbours[i]) {
                    break;
                }
            }

            if (j == count) {
                rooms[count] = neighbours[i];
                count++;
            }
        }
    }

    return 0;
}


/**
 * Address: 7F03AF5C
 *
 * Finds the closest bg bullet collision among rooms not already visited by the shot traversal.
 * It first does a cheap bounding box test, then a precise test for rooms whose bounding boxes are intersected.
 * This seems to be a brute force/fallback version of the function above, chrpropFindFirstBgHitInConnectedRooms.
 * @return 0 if no bg hit in any unvisited room, otherwise the room number containing the closest bg hit.
 */
s32 chrpropFindClosestBgHitRoom(s32 unused, coord3d *from, coord3d *to, coord3d *dir, coord3d *scaledDir, u8 *visited, struct HitThing *besthit)
{
    f32 dx;
    f32 dy;
    struct HitThing hit;
    f32 scale;
    f32 dist;
    f32 adjusteddist;
    f32 bestdist;
    s32 bestroom;
    f32 tmp;
    s32 room;

    bestdist = M_U32_MAX_VALUE_F;
    bestroom = 0;

    scale = get_room_data_float2();

    room = 1;

    if (getMaxNumRooms() >= 2)
    {
        do
        {
            if (visited[room] == 0)
            {
                visited[room] = 1;

                if (chrpropRayIntersectsRoomBbox(room, scaledDir, dir))
                {
                    if (bgTestBulletHitBackground(from, to, room, &hit))
                    {
                        dx = (hit.hitpos.x * scale) - from->x;
                        dy = ((hit.hitpos.y * scale) - from->y) * 1.0f;
                        dist = (hit.hitpos.z * scale) - from->z;
                        dist = (tmp = ((dx * dx) + (dy * dy)) + (dist * dist));
                        adjusteddist = tmp;

                        if (check_if_imageID_is_light(hit.texturenum))
                        {
                            adjusteddist = tmp - 4.0f;
                        }

                        if (adjusteddist < bestdist)
                        {
                            besthit->hitpos.x = hit.hitpos.x;
                            besthit->hitpos.y = hit.hitpos.y;
                            besthit->hitpos.z = hit.hitpos.z;

                            bestdist = adjusteddist;
                            bestroom = room;

                            besthit->normal.x = hit.normal.x;
                            besthit->normal.y = hit.normal.y;
                            besthit->normal.z = hit.normal.z;

                            besthit->vtx0 = hit.vtx0;
                            besthit->vtx1 = hit.vtx1;
                            besthit->vtx2 = hit.vtx2;

                            besthit->texturenum = hit.texturenum;
                            besthit->tricmd = hit.tricmd;
                            besthit->unk28 = hit.unk28;
                        }
                    }
                }
            }

            room++;
        }
        while (room < getMaxNumRooms());
    }

    return bestroom;
}


#ifdef NONMATCHING
void chraiDefaultWeaponFireHandler(void) {
    assert(!IsBadVec3d((vec3d *)phit.hits[i].hitinfo.hitpos)}
#else
GLOBAL_ASM(
.text
glabel chraiDefaultWeaponFireHandler
/* 06FC8C 7F03B15C 27BDFA88 */  addiu $sp, $sp, -0x578
/* 06FC90 7F03B160 AFBF0034 */  sw    $ra, 0x34($sp)
/* 06FC94 7F03B164 AFB00028 */  sw    $s0, 0x28($sp)
/* 06FC98 7F03B168 00808025 */  move  $s0, $a0
/* 06FC9C 7F03B16C AFB20030 */  sw    $s2, 0x30($sp)
/* 06FCA0 7F03B170 AFB1002C */  sw    $s1, 0x2c($sp)
/* 06FCA4 7F03B174 AFA0056C */  sw    $zero, 0x56c($sp)
/* 06FCA8 7F03B178 AFA0055C */  sw    $zero, 0x55c($sp)
/* 06FCAC 7F03B17C AFA00554 */  sw    $zero, 0x554($sp)
/* 06FCB0 7F03B180 0FC225E6 */  jal   getCurrentPlayerProp
/* 06FCB4 7F03B184 AFA00544 */   sw    $zero, 0x544($sp)
/* 06FCB8 7F03B188 8C4E0014 */  lw    $t6, 0x14($v0)
/* 06FCBC 7F03B18C 27B20194 */  addiu $s2, $sp, 0x194
/* 06FCC0 7F03B190 00408825 */  move  $s1, $v0
/* 06FCC4 7F03B194 AFA0018C */  sw    $zero, 0x18c($sp)
/* 06FCC8 7F03B198 02402025 */  move  $a0, $s2
/* 06FCCC 7F03B19C 27A501A0 */  addiu $a1, $sp, 0x1a0
/* 06FCD0 7F03B1A0 02003025 */  move  $a2, $s0
/* 06FCD4 7F03B1A4 0FC1A073 */  jal   bullet_path_from_screen_center
/* 06FCD8 7F03B1A8 AFAE04F8 */   sw    $t6, 0x4f8($sp)
/* 06FCDC 7F03B1AC 0FC17674 */  jal   getCurrentPlayerWeaponId
/* 06FCE0 7F03B1B0 02002025 */   move  $a0, $s0
/* 06FCE4 7F03B1B4 3C014F80 */  li    $at, 0x4F800000 # 4294967296.000000
/* 06FCE8 7F03B1B8 44812000 */  mtc1  $at, $f4
/* 06FCEC 7F03B1BC AFA201AC */  sw    $v0, 0x1ac($sp)
/* 06FCF0 7F03B1C0 27A204B4 */  addiu $v0, $sp, 0x4b4
/* 06FCF4 7F03B1C4 AFA001D0 */  sw    $zero, 0x1d0($sp)
/* 06FCF8 7F03B1C8 AFA001D4 */  sw    $zero, 0x1d4($sp)
/* 06FCFC 7F03B1CC AFA001D8 */  sw    $zero, 0x1d8($sp)
/* 06FD00 7F03B1D0 AFA00220 */  sw    $zero, 0x220($sp)
/* 06FD04 7F03B1D4 AFA00224 */  sw    $zero, 0x224($sp)
/* 06FD08 7F03B1D8 AFA00228 */  sw    $zero, 0x228($sp)
/* 06FD0C 7F03B1DC 27B00234 */  addiu $s0, $sp, 0x234
/* 06FD10 7F03B1E0 E7A401C8 */  swc1  $f4, 0x1c8($sp)
.L7F03B1E4:
/* 06FD14 7F03B1E4 26100140 */  addiu $s0, $s0, 0x140
/* 06FD18 7F03B1E8 AE00FF4C */  sw    $zero, -0xb4($s0)
/* 06FD1C 7F03B1EC AE00FF50 */  sw    $zero, -0xb0($s0)
/* 06FD20 7F03B1F0 AE00FF54 */  sw    $zero, -0xac($s0)
/* 06FD24 7F03B1F4 AE00FF9C */  sw    $zero, -0x64($s0)
/* 06FD28 7F03B1F8 AE00FFA0 */  sw    $zero, -0x60($s0)
/* 06FD2C 7F03B1FC AE00FFA4 */  sw    $zero, -0x5c($s0)
/* 06FD30 7F03B200 AE00FFEC */  sw    $zero, -0x14($s0)
/* 06FD34 7F03B204 AE00FFF0 */  sw    $zero, -0x10($s0)
/* 06FD38 7F03B208 AE00FFF4 */  sw    $zero, -0xc($s0)
/* 06FD3C 7F03B20C AE00FEFC */  sw    $zero, -0x104($s0)
/* 06FD40 7F03B210 AE00FF00 */  sw    $zero, -0x100($s0)
/* 06FD44 7F03B214 1602FFF3 */  bne   $s0, $v0, .L7F03B1E4
/* 06FD48 7F03B218 AE00FF04 */   sw    $zero, -0xfc($s0)
/* 06FD4C 7F03B21C C7A60194 */  lwc1  $f6, 0x194($sp)
/* 06FD50 7F03B220 C7A80198 */  lwc1  $f8, 0x198($sp)
/* 06FD54 7F03B224 C7AA019C */  lwc1  $f10, 0x19c($sp)
/* 06FD58 7F03B228 E7A601B0 */  swc1  $f6, 0x1b0($sp)
/* 06FD5C 7F03B22C E7A801B4 */  swc1  $f8, 0x1b4($sp)
/* 06FD60 7F03B230 0FC1E111 */  jal   currentPlayerGetViewToWorldMtxf
/* 06FD64 7F03B234 E7AA01B8 */   swc1  $f10, 0x1b8($sp)
/* 06FD68 7F03B238 00402025 */  move  $a0, $v0
/* 06FD6C 7F03B23C 0FC1611D */  jal   mtx4TransformVecInPlace
/* 06FD70 7F03B240 27A501B0 */   addiu $a1, $sp, 0x1b0
/* 06FD74 7F03B244 C7B001A0 */  lwc1  $f16, 0x1a0($sp)
/* 06FD78 7F03B248 C7B201A4 */  lwc1  $f18, 0x1a4($sp)
/* 06FD7C 7F03B24C C7A401A8 */  lwc1  $f4, 0x1a8($sp)
/* 06FD80 7F03B250 E7B001BC */  swc1  $f16, 0x1bc($sp)
/* 06FD84 7F03B254 E7B201C0 */  swc1  $f18, 0x1c0($sp)
/* 06FD88 7F03B258 0FC1E111 */  jal   currentPlayerGetViewToWorldMtxf
/* 06FD8C 7F03B25C E7A401C4 */   swc1  $f4, 0x1c4($sp)
/* 06FD90 7F03B260 00402025 */  move  $a0, $v0
/* 06FD94 7F03B264 0FC160F6 */  jal   mtx4RotateVecInPlace
/* 06FD98 7F03B268 27A501BC */   addiu $a1, $sp, 0x1bc
/* 06FD9C 7F03B26C 3C014780 */  li    $at, 0x47800000 # 65536.000000
/* 06FDA0 7F03B270 44810000 */  mtc1  $at, $f0
/* 06FDA4 7F03B274 C7A601BC */  lwc1  $f6, 0x1bc($sp)
/* 06FDA8 7F03B278 C7AA01B0 */  lwc1  $f10, 0x1b0($sp)
/* 06FDAC 7F03B27C C7B201C0 */  lwc1  $f18, 0x1c0($sp)
/* 06FDB0 7F03B280 46003202 */  mul.s $f8, $f6, $f0
/* 06FDB4 7F03B284 C7A601B4 */  lwc1  $f6, 0x1b4($sp)
/* 06FDB8 7F03B288 44075000 */  mfc1  $a3, $f10
/* 06FDBC 7F03B28C 46009102 */  mul.s $f4, $f18, $f0
/* 06FDC0 7F03B290 27A404F8 */  addiu $a0, $sp, 0x4f8
/* 06FDC4 7F03B294 460A4400 */  add.s $f16, $f8, $f10
/* 06FDC8 7F03B298 46062200 */  add.s $f8, $f4, $f6
/* 06FDCC 7F03B29C E7B004EC */  swc1  $f16, 0x4ec($sp)
/* 06FDD0 7F03B2A0 C7B001C4 */  lwc1  $f16, 0x1c4($sp)
/* 06FDD4 7F03B2A4 C7A401B8 */  lwc1  $f4, 0x1b8($sp)
/* 06FDD8 7F03B2A8 E7A804F0 */  swc1  $f8, 0x4f0($sp)
/* 06FDDC 7F03B2AC 46008482 */  mul.s $f18, $f16, $f0
/* 06FDE0 7F03B2B0 46049180 */  add.s $f6, $f18, $f4
/* 06FDE4 7F03B2B4 E7A604F4 */  swc1  $f6, 0x4f4($sp)
/* 06FDE8 7F03B2B8 8E260010 */  lw    $a2, 0x10($s1)
/* 06FDEC 7F03B2BC 8E250008 */  lw    $a1, 8($s1)
/* 06FDF0 7F03B2C0 0FC2C2F9 */  jal   walkTilesBetweenPoints_NoCallback
/* 06FDF4 7F03B2C4 E7A40010 */   swc1  $f4, 0x10($sp)
/* 06FDF8 7F03B2C8 504000B9 */  beql  $v0, $zero, .L7F03B5B0
/* 06FDFC 7F03B2CC 8FB901AC */   lw    $t9, 0x1ac($sp)
/* 06FE00 7F03B2D0 0FC2D20F */  jal   get_room_data_float1
/* 06FE04 7F03B2D4 00000000 */   nop
/* 06FE08 7F03B2D8 0FC2D21E */  jal   bgGetLevelVisibilityScale
/* 06FE0C 7F03B2DC E7A00048 */   swc1  $f0, 0x48($sp)
/* 06FE10 7F03B2E0 C7A20048 */  lwc1  $f2, 0x48($sp)
/* 06FE14 7F03B2E4 46020202 */  mul.s $f8, $f0, $f2
/* 06FE18 7F03B2E8 0FC227F5 */  jal   bondviewGetCurrentPlayersPosition
/* 06FE1C 7F03B2EC E7A8006C */   swc1  $f8, 0x6c($sp)
/* 06FE20 7F03B2F0 0FC2C731 */  jal   stanResetHits
/* 06FE24 7F03B2F4 00408025 */   move  $s0, $v0
/* 06FE28 7F03B2F8 C7B004F4 */  lwc1  $f16, 0x4f4($sp)
/* 06FE2C 7F03B2FC 27A404F8 */  addiu $a0, $sp, 0x4f8
/* 06FE30 7F03B300 8FA501B0 */  lw    $a1, 0x1b0($sp)
/* 06FE34 7F03B304 8FA601B8 */  lw    $a2, 0x1b8($sp)
/* 06FE38 7F03B308 8FA704EC */  lw    $a3, 0x4ec($sp)
/* 06FE3C 7F03B30C 0FC2C2F9 */  jal   walkTilesBetweenPoints_NoCallback
/* 06FE40 7F03B310 E7B00010 */   swc1  $f16, 0x10($sp)
/* 06FE44 7F03B314 14400008 */  bnez  $v0, .L7F03B338
/* 06FE48 7F03B318 C7B204EC */   lwc1  $f18, 0x4ec($sp)
/* 06FE4C 7F03B31C 27A401B0 */  addiu $a0, $sp, 0x1b0
/* 06FE50 7F03B320 27A501BC */  addiu $a1, $sp, 0x1bc
/* 06FE54 7F03B324 0FC09893 */  jal   chrlvStanLineDirIntersection
/* 06FE58 7F03B328 27A60560 */   addiu $a2, $sp, 0x560
/* 06FE5C 7F03B32C 240F0001 */  li    $t7, 1
/* 06FE60 7F03B330 10000006 */  b     .L7F03B34C
/* 06FE64 7F03B334 AFAF056C */   sw    $t7, 0x56c($sp)
.L7F03B338:
/* 06FE68 7F03B338 C7A604F0 */  lwc1  $f6, 0x4f0($sp)
/* 06FE6C 7F03B33C C7AA04F4 */  lwc1  $f10, 0x4f4($sp)
/* 06FE70 7F03B340 E7B20560 */  swc1  $f18, 0x560($sp)
/* 06FE74 7F03B344 E7A60564 */  swc1  $f6, 0x564($sp)
/* 06FE78 7F03B348 E7AA0568 */  swc1  $f10, 0x568($sp)
.L7F03B34C:
/* 06FE7C 7F03B34C C7A40560 */  lwc1  $f4, 0x560($sp)
/* 06FE80 7F03B350 C6080000 */  lwc1  $f8, ($s0)
/* 06FE84 7F03B354 C7B20564 */  lwc1  $f18, 0x564($sp)
/* 06FE88 7F03B358 C7A0006C */  lwc1  $f0, 0x6c($sp)
/* 06FE8C 7F03B35C 46082401 */  sub.s $f16, $f4, $f8
/* 06FE90 7F03B360 C7A40568 */  lwc1  $f4, 0x568($sp)
/* 06FE94 7F03B364 8FA404F8 */  lw    $a0, 0x4f8($sp)
/* 06FE98 7F03B368 E7B00070 */  swc1  $f16, 0x70($sp)
/* 06FE9C 7F03B36C C6060004 */  lwc1  $f6, 4($s0)
/* 06FEA0 7F03B370 46069281 */  sub.s $f10, $f18, $f6
/* 06FEA4 7F03B374 E7AA0074 */  swc1  $f10, 0x74($sp)
/* 06FEA8 7F03B378 C6080008 */  lwc1  $f8, 8($s0)
/* 06FEAC 7F03B37C 46082401 */  sub.s $f16, $f4, $f8
/* 06FEB0 7F03B380 E7B00078 */  swc1  $f16, 0x78($sp)
/* 06FEB4 7F03B384 C6120000 */  lwc1  $f18, ($s0)
/* 06FEB8 7F03B388 46009182 */  mul.s $f6, $f18, $f0
/* 06FEBC 7F03B38C E7A6007C */  swc1  $f6, 0x7c($sp)
/* 06FEC0 7F03B390 C60A0004 */  lwc1  $f10, 4($s0)
/* 06FEC4 7F03B394 46005102 */  mul.s $f4, $f10, $f0
/* 06FEC8 7F03B398 E7A40080 */  swc1  $f4, 0x80($sp)
/* 06FECC 7F03B39C C6080008 */  lwc1  $f8, 8($s0)
/* 06FED0 7F03B3A0 AFA4055C */  sw    $a0, 0x55c($sp)
/* 06FED4 7F03B3A4 46004402 */  mul.s $f16, $f8, $f0
/* 06FED8 7F03B3A8 0FC2CBF6 */  jal   getTileRoom
/* 06FEDC 7F03B3AC E7B00084 */   swc1  $f16, 0x84($sp)
/* 06FEE0 7F03B3B0 00408825 */  move  $s1, $v0
/* 06FEE4 7F03B3B4 27A2018C */  addiu $v0, $sp, 0x18c
/* 06FEE8 7F03B3B8 27A3008C */  addiu $v1, $sp, 0x8c
.L7F03B3BC:
/* 06FEEC 7F03B3BC 24630004 */  addiu $v1, $v1, 4
/* 06FEF0 7F03B3C0 A060FFFD */  sb    $zero, -3($v1)
/* 06FEF4 7F03B3C4 A060FFFE */  sb    $zero, -2($v1)
/* 06FEF8 7F03B3C8 A060FFFF */  sb    $zero, -1($v1)
/* 06FEFC 7F03B3CC 1462FFFB */  bne   $v1, $v0, .L7F03B3BC
/* 06FF00 7F03B3D0 A060FFFC */   sb    $zero, -4($v1)
/* 06FF04 7F03B3D4 02002025 */  move  $a0, $s0
/* 06FF08 7F03B3D8 27A50560 */  addiu $a1, $sp, 0x560
/* 06FF0C 7F03B3DC 02203025 */  move  $a2, $s1
/* 06FF10 7F03B3E0 0FC2DE9E */  jal   bgTestBulletHitBackground
/* 06FF14 7F03B3E4 27A70510 */   addiu $a3, $sp, 0x510
/* 06FF18 7F03B3E8 10400002 */  beqz  $v0, .L7F03B3F4
/* 06FF1C 7F03B3EC 24190001 */   li    $t9, 1
/* 06FF20 7F03B3F0 AFB10544 */  sw    $s1, 0x544($sp)
.L7F03B3F4:
/* 06FF24 7F03B3F4 03B14021 */  addu  $t0, $sp, $s1
/* 06FF28 7F03B3F8 A119008C */  sb    $t9, 0x8c($t0)
/* 06FF2C 7F03B3FC 8FA90544 */  lw    $t1, 0x544($sp)
/* 06FF30 7F03B400 3C0A8008 */  lui   $t2, %hi(g_BgPortals)
/* 06FF34 7F03B404 5D200027 */  bgtzl $t1, .L7F03B4A4
/* 06FF38 7F03B408 8FA80544 */   lw    $t0, 0x544($sp)
/* 06FF3C 7F03B40C 8D4AFF80 */  lw    $t2, %lo(g_BgPortals)($t2)
/* 06FF40 7F03B410 8D4B0000 */  lw    $t3, ($t2)
/* 06FF44 7F03B414 11600012 */  beqz  $t3, .L7F03B460
/* 06FF48 7F03B418 00000000 */   nop
/* 06FF4C 7F03B41C 0FC225E6 */  jal   getCurrentPlayerProp
/* 06FF50 7F03B420 00000000 */   nop
/* 06FF54 7F03B424 0FC2CBF6 */  jal   getTileRoom
/* 06FF58 7F03B428 8C440014 */   lw    $a0, 0x14($v0)
/* 06FF5C 7F03B42C 27AC007C */  addiu $t4, $sp, 0x7c
/* 06FF60 7F03B430 27AD008C */  addiu $t5, $sp, 0x8c
/* 06FF64 7F03B434 27AE0510 */  addiu $t6, $sp, 0x510
/* 06FF68 7F03B438 AFAE0018 */  sw    $t6, 0x18($sp)
/* 06FF6C 7F03B43C AFAD0014 */  sw    $t5, 0x14($sp)
/* 06FF70 7F03B440 AFAC0010 */  sw    $t4, 0x10($sp)
/* 06FF74 7F03B444 00402025 */  move  $a0, $v0
/* 06FF78 7F03B448 02002825 */  move  $a1, $s0
/* 06FF7C 7F03B44C 27A60560 */  addiu $a2, $sp, 0x560
/* 06FF80 7F03B450 0FC0EB7D */  jal   chrpropFindFirstBgHitInConnectedRooms
/* 06FF84 7F03B454 27A70070 */   addiu $a3, $sp, 0x70
/* 06FF88 7F03B458 10000011 */  b     .L7F03B4A0
/* 06FF8C 7F03B45C AFA20544 */   sw    $v0, 0x544($sp)
.L7F03B460:
/* 06FF90 7F03B460 0FC225E6 */  jal   getCurrentPlayerProp
/* 06FF94 7F03B464 00000000 */   nop
/* 06FF98 7F03B468 0FC2CBF6 */  jal   getTileRoom
/* 06FF9C 7F03B46C 8C440014 */   lw    $a0, 0x14($v0)
/* 06FFA0 7F03B470 27AF007C */  addiu $t7, $sp, 0x7c
/* 06FFA4 7F03B474 27B8008C */  addiu $t8, $sp, 0x8c
/* 06FFA8 7F03B478 27B90510 */  addiu $t9, $sp, 0x510
/* 06FFAC 7F03B47C AFB90018 */  sw    $t9, 0x18($sp)
/* 06FFB0 7F03B480 AFB80014 */  sw    $t8, 0x14($sp)
/* 06FFB4 7F03B484 AFAF0010 */  sw    $t7, 0x10($sp)
/* 06FFB8 7F03B488 00402025 */  move  $a0, $v0
/* 06FFBC 7F03B48C 02002825 */  move  $a1, $s0
/* 06FFC0 7F03B490 27A60560 */  addiu $a2, $sp, 0x560
/* 06FFC4 7F03B494 0FC0EBD7 */  jal   chrpropFindClosestBgHitRoom
/* 06FFC8 7F03B498 27A70070 */   addiu $a3, $sp, 0x70
/* 06FFCC 7F03B49C AFA20544 */  sw    $v0, 0x544($sp)
.L7F03B4A0:
/* 06FFD0 7F03B4A0 8FA80544 */  lw    $t0, 0x544($sp)
.L7F03B4A4:
/* 06FFD4 7F03B4A4 5900000F */  blezl $t0, .L7F03B4E4
/* 06FFD8 7F03B4A8 8FAB0544 */   lw    $t3, 0x544($sp)
/* 06FFDC 7F03B4AC 0FC2D20C */  jal   get_room_data_float2
/* 06FFE0 7F03B4B0 00000000 */   nop
/* 06FFE4 7F03B4B4 C7B20510 */  lwc1  $f18, 0x510($sp)
/* 06FFE8 7F03B4B8 C7AA0514 */  lwc1  $f10, 0x514($sp)
/* 06FFEC 7F03B4BC C7A80518 */  lwc1  $f8, 0x518($sp)
/* 06FFF0 7F03B4C0 46009182 */  mul.s $f6, $f18, $f0
/* 06FFF4 7F03B4C4 00000000 */  nop
/* 06FFF8 7F03B4C8 46005102 */  mul.s $f4, $f10, $f0
/* 06FFFC 7F03B4CC 00000000 */  nop
/* 070000 7F03B4D0 46004402 */  mul.s $f16, $f8, $f0
/* 070004 7F03B4D4 E7A60510 */  swc1  $f6, 0x510($sp)
/* 070008 7F03B4D8 E7A40514 */  swc1  $f4, 0x514($sp)
/* 07000C 7F03B4DC E7B00518 */  swc1  $f16, 0x518($sp)
/* 070010 7F03B4E0 8FAB0544 */  lw    $t3, 0x544($sp)
.L7F03B4E4:
/* 070014 7F03B4E4 27A9008C */  addiu $t1, $sp, 0x8c
/* 070018 7F03B4E8 27AA0510 */  addiu $t2, $sp, 0x510
/* 07001C 7F03B4EC AFAA0014 */  sw    $t2, 0x14($sp)
/* 070020 7F03B4F0 AFA90010 */  sw    $t1, 0x10($sp)
/* 070024 7F03B4F4 02002025 */  move  $a0, $s0
/* 070028 7F03B4F8 27A50560 */  addiu $a1, $sp, 0x560
/* 07002C 7F03B4FC 27A60070 */  addiu $a2, $sp, 0x70
/* 070030 7F03B500 27A7007C */  addiu $a3, $sp, 0x7c
/* 070034 7F03B504 0FC0EAD6 */  jal   chrpropFindCloserBgHitInVisibleRooms
/* 070038 7F03B508 AFAB0018 */   sw    $t3, 0x18($sp)
/* 07003C 7F03B50C 1840000F */  blez  $v0, .L7F03B54C
/* 070040 7F03B510 AFA20544 */   sw    $v0, 0x544($sp)
/* 070044 7F03B514 C7B20510 */  lwc1  $f18, 0x510($sp)
/* 070048 7F03B518 87AD053A */  lh    $t5, 0x53a($sp)
/* 07004C 7F03B51C C7B00514 */  lwc1  $f16, 0x514($sp)
/* 070050 7F03B520 C7AE0518 */  lwc1  $f14, 0x518($sp)
/* 070054 7F03B524 240C0001 */  li    $t4, 1
/* 070058 7F03B528 AFAC0554 */  sw    $t4, 0x554($sp)
/* 07005C 7F03B52C AFAD0540 */  sw    $t5, 0x540($sp)
/* 070060 7F03B530 E7B20548 */  swc1  $f18, 0x548($sp)
/* 070064 7F03B534 E7B20500 */  swc1  $f18, 0x500($sp)
/* 070068 7F03B538 E7B0054C */  swc1  $f16, 0x54c($sp)
/* 07006C 7F03B53C E7B00504 */  swc1  $f16, 0x504($sp)
/* 070070 7F03B540 E7AE0550 */  swc1  $f14, 0x550($sp)
/* 070074 7F03B544 1000000A */  b     .L7F03B570
/* 070078 7F03B548 E7AE0508 */   swc1  $f14, 0x508($sp)
.L7F03B54C:
/* 07007C 7F03B54C C7B204EC */  lwc1  $f18, 0x4ec($sp)
/* 070080 7F03B550 C7A604F0 */  lwc1  $f6, 0x4f0($sp)
/* 070084 7F03B554 C7AA04F4 */  lwc1  $f10, 0x4f4($sp)
/* 070088 7F03B558 240EFFFF */  li    $t6, -1
/* 07008C 7F03B55C AFB10544 */  sw    $s1, 0x544($sp)
/* 070090 7F03B560 AFAE0540 */  sw    $t6, 0x540($sp)
/* 070094 7F03B564 E7B20500 */  swc1  $f18, 0x500($sp)
/* 070098 7F03B568 E7A60504 */  swc1  $f6, 0x504($sp)
/* 07009C 7F03B56C E7AA0508 */  swc1  $f10, 0x508($sp)
.L7F03B570:
/* 0700A0 7F03B570 8FAF056C */  lw    $t7, 0x56c($sp)
/* 0700A4 7F03B574 8FB80554 */  lw    $t8, 0x554($sp)
/* 0700A8 7F03B578 15E00003 */  bnez  $t7, .L7F03B588
/* 0700AC 7F03B57C 00000000 */   nop
/* 0700B0 7F03B580 5300000B */  beql  $t8, $zero, .L7F03B5B0
/* 0700B4 7F03B584 8FB901AC */   lw    $t9, 0x1ac($sp)
.L7F03B588:
/* 0700B8 7F03B588 0FC1E0F1 */  jal   camGetWorldToScreenMtxf
/* 0700BC 7F03B58C 00000000 */   nop
/* 0700C0 7F03B590 00402025 */  move  $a0, $v0
/* 0700C4 7F03B594 0FC1611D */  jal   mtx4TransformVecInPlace
/* 0700C8 7F03B598 27A50500 */   addiu $a1, $sp, 0x500
/* 0700CC 7F03B59C C7A40508 */  lwc1  $f4, 0x508($sp)
/* 0700D0 7F03B5A0 46002207 */  neg.s $f8, $f4
/* 0700D4 7F03B5A4 E7A8050C */  swc1  $f8, 0x50c($sp)
/* 0700D8 7F03B5A8 E7A801C8 */  swc1  $f8, 0x1c8($sp)
/* 0700DC 7F03B5AC 8FB901AC */  lw    $t9, 0x1ac($sp)
.L7F03B5B0:
/* 0700E0 7F03B5B0 24010017 */  li    $at, 23
/* 0700E4 7F03B5B4 C7B201C8 */  lwc1  $f18, 0x1c8($sp)
/* 0700E8 7F03B5B8 1721000B */  bne   $t9, $at, .L7F03B5E8
/* 0700EC 7F03B5BC 3C118007 */   lui   $s1, %hi(g_LastOnScreenProp)
/* 0700F0 7F03B5C0 3C014396 */  li    $at, 0x43960000 # 300.000000
/* 0700F4 7F03B5C4 44818000 */  mtc1  $at, $f16
/* 0700F8 7F03B5C8 3C014396 */  li    $at, 0x43960000 # 300.000000
/* 0700FC 7F03B5CC 4612803C */  c.lt.s $f16, $f18
/* 070100 7F03B5D0 00000000 */  nop
/* 070104 7F03B5D4 45000004 */  bc1f  .L7F03B5E8
/* 070108 7F03B5D8 00000000 */   nop
/* 07010C 7F03B5DC 44813000 */  mtc1  $at, $f6
/* 070110 7F03B5E0 00000000 */  nop
/* 070114 7F03B5E4 E7A601C8 */  swc1  $f6, 0x1c8($sp)
.L7F03B5E8:
/* 070118 7F03B5E8 8E311DF0 */  lw    $s1, %lo(g_LastOnScreenProp)($s1)
/* 07011C 7F03B5EC 3C088007 */  lui   $t0, %hi(g_OnScreenPropList)
/* 070120 7F03B5F0 25081620 */  addiu $t0, %lo(g_OnScreenPropList) # addiu $t0, $t0, 0x1620
/* 070124 7F03B5F4 2631FFFC */  addiu $s1, $s1, -4
/* 070128 7F03B5F8 0228082B */  sltu  $at, $s1, $t0
/* 07012C 7F03B5FC 5420002A */  bnezl $at, .L7F03B6A8
/* 070130 7F03B600 27B00194 */   addiu $s0, $sp, 0x194
/* 070134 7F03B604 8E300000 */  lw    $s0, ($s1)
.L7F03B608:
/* 070138 7F03B608 12000020 */  beqz  $s0, .L7F03B68C
/* 07013C 7F03B60C 00000000 */   nop
/* 070140 7F03B610 92020000 */  lbu   $v0, ($s0)
/* 070144 7F03B614 24010003 */  li    $at, 3
/* 070148 7F03B618 1041000C */  beq   $v0, $at, .L7F03B64C
/* 07014C 7F03B61C 24010006 */   li    $at, 6
/* 070150 7F03B620 54410010 */  bnel  $v0, $at, .L7F03B664
/* 070154 7F03B624 92020000 */   lbu   $v0, ($s0)
/* 070158 7F03B628 8E090004 */  lw    $t1, 4($s0)
/* 07015C 7F03B62C 5120000D */  beql  $t1, $zero, .L7F03B664
/* 070160 7F03B630 92020000 */   lbu   $v0, ($s0)
/* 070164 7F03B634 0FC26C57 */  jal   getPlayerPointerIndex
/* 070168 7F03B638 02002025 */   move  $a0, $s0
/* 07016C 7F03B63C 0FC26C54 */  jal   get_cur_playernum
/* 070170 7F03B640 AFA2004C */   sw    $v0, 0x4c($sp)
/* 070174 7F03B644 8FAA004C */  lw    $t2, 0x4c($sp)
/* 070178 7F03B648 104A0005 */  beq   $v0, $t2, .L7F03B660
.L7F03B64C:
/* 07017C 7F03B64C 02002025 */   move  $a0, $s0
/* 070180 7F03B650 0FC08992 */  jal   chrTestHit
/* 070184 7F03B654 02402825 */   move  $a1, $s2
/* 070188 7F03B658 1000000C */  b     .L7F03B68C
/* 07018C 7F03B65C 00000000 */   nop
.L7F03B660:
/* 070190 7F03B660 92020000 */  lbu   $v0, ($s0)
.L7F03B664:
/* 070194 7F03B664 24010001 */  li    $at, 1
/* 070198 7F03B668 02002025 */  move  $a0, $s0
/* 07019C 7F03B66C 10410005 */  beq   $v0, $at, .L7F03B684
/* 0701A0 7F03B670 24010004 */   li    $at, 4
/* 0701A4 7F03B674 10410003 */  beq   $v0, $at, .L7F03B684
/* 0701A8 7F03B678 24010002 */   li    $at, 2
/* 0701AC 7F03B67C 14410003 */  bne   $v0, $at, .L7F03B68C
/* 0701B0 7F03B680 00000000 */   nop
.L7F03B684:
/* 0701B4 7F03B684 0FC13A6F */  jal   sub_GAME_7F04E9BC
/* 0701B8 7F03B688 02402825 */   move  $a1, $s2
.L7F03B68C:
/* 0701BC 7F03B68C 3C0B8007 */  lui   $t3, %hi(g_OnScreenPropList)
/* 0701C0 7F03B690 256B1620 */  addiu $t3, %lo(g_OnScreenPropList) # addiu $t3, $t3, 0x1620
/* 0701C4 7F03B694 2631FFFC */  addiu $s1, $s1, -4
/* 0701C8 7F03B698 022B082B */  sltu  $at, $s1, $t3
/* 0701CC 7F03B69C 5020FFDA */  beql  $at, $zero, .L7F03B608
/* 0701D0 7F03B6A0 8E300000 */   lw    $s0, ($s1)
/* 0701D4 7F03B6A4 27B00194 */  addiu $s0, $sp, 0x194
.L7F03B6A8:
/* 0701D8 7F03B6A8 27B104B4 */  addiu $s1, $sp, 0x4b4
/* 0701DC 7F03B6AC 8E03003C */  lw    $v1, 0x3c($s0)
.L7F03B6B0:
/* 0701E0 7F03B6B0 50600024 */  beql  $v1, $zero, .L7F03B744
/* 0701E4 7F03B6B4 26100050 */   addiu $s0, $s0, 0x50
/* 0701E8 7F03B6B8 90620000 */  lbu   $v0, ($v1)
/* 0701EC 7F03B6BC 24010003 */  li    $at, 3
/* 0701F0 7F03B6C0 02402025 */  move  $a0, $s2
/* 0701F4 7F03B6C4 10410003 */  beq   $v0, $at, .L7F03B6D4
/* 0701F8 7F03B6C8 24010006 */   li    $at, 6
/* 0701FC 7F03B6CC 54410006 */  bnel  $v0, $at, .L7F03B6E8
/* 070200 7F03B6D0 24010001 */   li    $at, 1
.L7F03B6D4:
/* 070204 7F03B6D4 0FC08A60 */  jal   chrHandleBulletHit
/* 070208 7F03B6D8 26050038 */   addiu $a1, $s0, 0x38
/* 07020C 7F03B6DC 1000000C */  b     .L7F03B710
/* 070210 7F03B6E0 8E0C0084 */   lw    $t4, 0x84($s0)
/* 070214 7F03B6E4 24010001 */  li    $at, 1
.L7F03B6E8:
/* 070218 7F03B6E8 10410006 */  beq   $v0, $at, .L7F03B704
/* 07021C 7F03B6EC 02402025 */   move  $a0, $s2
/* 070220 7F03B6F0 24010004 */  li    $at, 4
/* 070224 7F03B6F4 10410003 */  beq   $v0, $at, .L7F03B704
/* 070228 7F03B6F8 24010002 */   li    $at, 2
/* 07022C 7F03B6FC 54410004 */  bnel  $v0, $at, .L7F03B710
/* 070230 7F03B700 8E0C0084 */   lw    $t4, 0x84($s0)
.L7F03B704:
/* 070234 7F03B704 0FC13A9A */  jal   objHit
/* 070238 7F03B708 26050038 */   addiu $a1, $s0, 0x38
/* 07023C 7F03B70C 8E0C0084 */  lw    $t4, 0x84($s0)
.L7F03B710:
/* 070240 7F03B710 8FAD018C */  lw    $t5, 0x18c($sp)
/* 070244 7F03B714 1180000A */  beqz  $t4, .L7F03B740
/* 070248 7F03B718 25AE0001 */   addiu $t6, $t5, 1
/* 07024C 7F03B71C AFAE018C */  sw    $t6, 0x18c($sp)
/* 070250 7F03B720 0FC1780E */  jal   bondwalkItemGetObjectsShootThrough
/* 070254 7F03B724 8FA401AC */   lw    $a0, 0x1ac($sp)
/* 070258 7F03B728 8FAF018C */  lw    $t7, 0x18c($sp)
/* 07025C 7F03B72C 01E2082A */  slt   $at, $t7, $v0
/* 070260 7F03B730 54200004 */  bnezl $at, .L7F03B744
/* 070264 7F03B734 26100050 */   addiu $s0, $s0, 0x50
/* 070268 7F03B738 AFA00554 */  sw    $zero, 0x554($sp)
/* 07026C 7F03B73C AFA0056C */  sw    $zero, 0x56c($sp)
.L7F03B740:
/* 070270 7F03B740 26100050 */  addiu $s0, $s0, 0x50
.L7F03B744:
/* 070274 7F03B744 5611FFDA */  bnel  $s0, $s1, .L7F03B6B0
/* 070278 7F03B748 8E03003C */   lw    $v1, 0x3c($s0)
/* 07027C 7F03B74C 8FB80554 */  lw    $t8, 0x554($sp)
/* 070280 7F03B750 8FB9056C */  lw    $t9, 0x56c($sp)
/* 070284 7F03B754 8FA801AC */  lw    $t0, 0x1ac($sp)
/* 070288 7F03B758 17000002 */  bnez  $t8, .L7F03B764
/* 07028C 7F03B75C 24010017 */   li    $at, 23
/* 070290 7F03B760 13200091 */  beqz  $t9, .L7F03B9A8
.L7F03B764:
/* 070294 7F03B764 00008025 */   move  $s0, $zero
/* 070298 7F03B768 15010009 */  bne   $t0, $at, .L7F03B790
/* 07029C 7F03B76C 24110001 */   li    $s1, 1
/* 0702A0 7F03B770 3C014396 */  li    $at, 0x43960000 # 300.000000
/* 0702A4 7F03B774 44815000 */  mtc1  $at, $f10
/* 0702A8 7F03B778 C7A4050C */  lwc1  $f4, 0x50c($sp)
/* 0702AC 7F03B77C 4604503C */  c.lt.s $f10, $f4
/* 0702B0 7F03B780 00000000 */  nop
/* 0702B4 7F03B784 45020003 */  bc1fl .L7F03B794
/* 0702B8 7F03B788 8FA90554 */   lw    $t1, 0x554($sp)
/* 0702BC 7F03B78C 00008825 */  move  $s1, $zero
.L7F03B790:
/* 0702C0 7F03B790 8FA90554 */  lw    $t1, 0x554($sp)
.L7F03B794:
/* 0702C4 7F03B794 87AA053A */  lh    $t2, 0x53a($sp)
/* 0702C8 7F03B798 8FAC056C */  lw    $t4, 0x56c($sp)
/* 0702CC 7F03B79C 11200034 */  beqz  $t1, .L7F03B870
/* 0702D0 7F03B7A0 00000000 */   nop
/* 0702D4 7F03B7A4 05410004 */  bgez  $t2, .L7F03B7B8
/* 0702D8 7F03B7A8 87AB053A */   lh    $t3, 0x53a($sp)
/* 0702DC 7F03B7AC 3C108005 */  lui   $s0, %hi(g_HitTypeSounds)
/* 0702E0 7F03B7B0 1000000A */  b     .L7F03B7DC
/* 0702E4 7F03B7B4 8E10E86C */   lw    $s0, %lo(g_HitTypeSounds)($s0)
.L7F03B7B8:
/* 0702E8 7F03B7B8 000B60C0 */  sll   $t4, $t3, 3
/* 0702EC 7F03B7BC 3C0D8005 */  lui   $t5, %hi(g_Textures)
/* 0702F0 7F03B7C0 01AC6821 */  addu  $t5, $t5, $t4
/* 0702F4 7F03B7C4 91AD9300 */  lbu   $t5, %lo(g_Textures)($t5)
/* 0702F8 7F03B7C8 3C108005 */  lui   $s0, %hi(g_HitTypeSounds)
/* 0702FC 7F03B7CC 31AE000F */  andi  $t6, $t5, 0xf
/* 070300 7F03B7D0 000E7880 */  sll   $t7, $t6, 2
/* 070304 7F03B7D4 020F8021 */  addu  $s0, $s0, $t7
/* 070308 7F03B7D8 8E10E86C */  lw    $s0, %lo(g_HitTypeSounds)($s0)
.L7F03B7DC:
/* 07030C 7F03B7DC 12200022 */  beqz  $s1, .L7F03B868
/* 070310 7F03B7E0 00000000 */   nop
/* 070314 7F03B7E4 8618000A */  lh    $t8, 0xa($s0)
/* 070318 7F03B7E8 8FB901AC */  lw    $t9, 0x1ac($sp)
/* 07031C 7F03B7EC 24010017 */  li    $at, 23
/* 070320 7F03B7F0 1B000016 */  blez  $t8, .L7F03B84C
/* 070324 7F03B7F4 00000000 */   nop
/* 070328 7F03B7F8 13210014 */  beq   $t9, $at, .L7F03B84C
/* 07032C 7F03B7FC 00000000 */   nop
/* 070330 7F03B800 0C002914 */  jal   randomGetNext
/* 070334 7F03B804 00000000 */   nop
/* 070338 7F03B808 8608000A */  lh    $t0, 0xa($s0)
/* 07033C 7F03B80C 8E090004 */  lw    $t1, 4($s0)
/* 070340 7F03B810 240BFFFF */  li    $t3, -1
/* 070344 7F03B814 0048001B */  divu  $zero, $v0, $t0
/* 070348 7F03B818 00001810 */  mfhi  $v1
/* 07034C 7F03B81C 01235021 */  addu  $t2, $t1, $v1
/* 070350 7F03B820 91460000 */  lbu   $a2, ($t2)
/* 070354 7F03B824 15000002 */  bnez  $t0, .L7F03B830
/* 070358 7F03B828 00000000 */   nop
/* 07035C 7F03B82C 0007000D */  break 7
.L7F03B830:
/* 070360 7F03B830 AFA00018 */  sw    $zero, 0x18($sp)
/* 070364 7F03B834 AFAB0014 */  sw    $t3, 0x14($sp)
/* 070368 7F03B838 AFA00010 */  sw    $zero, 0x10($sp)
/* 07036C 7F03B83C 27A40548 */  addiu $a0, $sp, 0x548
/* 070370 7F03B840 27A5051C */  addiu $a1, $sp, 0x51c
/* 070374 7F03B844 0FC28423 */  jal   explosionCreateBulletImpact
/* 070378 7F03B848 87A70546 */   lh    $a3, 0x546($sp)
.L7F03B84C:
/* 07037C 7F03B84C 0FC2ED8C */  jal   check_if_imageID_is_light
/* 070380 7F03B850 87A4053A */   lh    $a0, 0x53a($sp)
/* 070384 7F03B854 10400004 */  beqz  $v0, .L7F03B868
/* 070388 7F03B858 8FA40534 */   lw    $a0, 0x534($sp)
/* 07038C 7F03B85C 87A50538 */  lh    $a1, 0x538($sp)
/* 070390 7F03B860 0FC2EF83 */  jal   lightFixtureBreak
/* 070394 7F03B864 8FA60544 */   lw    $a2, 0x544($sp)
.L7F03B868:
/* 070398 7F03B868 10000014 */  b     .L7F03B8BC
/* 07039C 7F03B86C 27B00548 */   addiu $s0, $sp, 0x548
.L7F03B870:
/* 0703A0 7F03B870 11800012 */  beqz  $t4, .L7F03B8BC
/* 0703A4 7F03B874 3C014780 */   li    $at, 0x47800000 # 65536.000000
/* 0703A8 7F03B878 44810000 */  mtc1  $at, $f0
/* 0703AC 7F03B87C C7A801BC */  lwc1  $f8, 0x1bc($sp)
/* 0703B0 7F03B880 C7B201B0 */  lwc1  $f18, 0x1b0($sp)
/* 0703B4 7F03B884 C7AA01C0 */  lwc1  $f10, 0x1c0($sp)
/* 0703B8 7F03B888 46004402 */  mul.s $f16, $f8, $f0
/* 0703BC 7F03B88C C7A801B4 */  lwc1  $f8, 0x1b4($sp)
/* 0703C0 7F03B890 27B00560 */  addiu $s0, $sp, 0x560
/* 0703C4 7F03B894 46005102 */  mul.s $f4, $f10, $f0
/* 0703C8 7F03B898 C7AA01B8 */  lwc1  $f10, 0x1b8($sp)
/* 0703CC 7F03B89C 46128180 */  add.s $f6, $f16, $f18
/* 0703D0 7F03B8A0 C7B201C4 */  lwc1  $f18, 0x1c4($sp)
/* 0703D4 7F03B8A4 46082400 */  add.s $f16, $f4, $f8
/* 0703D8 7F03B8A8 E7A60560 */  swc1  $f6, 0x560($sp)
/* 0703DC 7F03B8AC 46009182 */  mul.s $f6, $f18, $f0
/* 0703E0 7F03B8B0 E7B00564 */  swc1  $f16, 0x564($sp)
/* 0703E4 7F03B8B4 460A3100 */  add.s $f4, $f6, $f10
/* 0703E8 7F03B8B8 E7A40568 */  swc1  $f4, 0x568($sp)
.L7F03B8BC:
/* 0703EC 7F03B8BC 5200003B */  beql  $s0, $zero, .L7F03B9AC
/* 0703F0 7F03B8C0 8FBF0034 */   lw    $ra, 0x34($sp)
/* 0703F4 7F03B8C4 1220001D */  beqz  $s1, .L7F03B93C
/* 0703F8 7F03B8C8 8FA401AC */   lw    $a0, 0x1ac($sp)
/* 0703FC 7F03B8CC 02002825 */  move  $a1, $s0
/* 070400 7F03B8D0 0FC191DD */  jal   recall_joy2_hits_edit_flag
/* 070404 7F03B8D4 8FA60540 */   lw    $a2, 0x540($sp)
/* 070408 7F03B8D8 8FAD0540 */  lw    $t5, 0x540($sp)
/* 07040C 7F03B8DC 3C028005 */  lui   $v0, %hi(g_Textures)
/* 070410 7F03B8E0 24010005 */  li    $at, 5
/* 070414 7F03B8E4 000D70C0 */  sll   $t6, $t5, 3
/* 070418 7F03B8E8 004E1021 */  addu  $v0, $v0, $t6
/* 07041C 7F03B8EC 90429300 */  lbu   $v0, %lo(g_Textures)($v0)
/* 070420 7F03B8F0 304F000F */  andi  $t7, $v0, 0xf
/* 070424 7F03B8F4 11E10011 */  beq   $t7, $at, .L7F03B93C
/* 070428 7F03B8F8 24010006 */   li    $at, 6
/* 07042C 7F03B8FC 11E1000F */  beq   $t7, $at, .L7F03B93C
/* 070430 7F03B900 8FB80544 */   lw    $t8, 0x544($sp)
/* 070434 7F03B904 241900FF */  li    $t9, 255
/* 070438 7F03B908 A3B80050 */  sb    $t8, 0x50($sp)
/* 07043C 7F03B90C 0FC26C54 */  jal   get_cur_playernum
/* 070440 7F03B910 A3B90051 */   sb    $t9, 0x51($sp)
/* 070444 7F03B914 27A80050 */  addiu $t0, $sp, 0x50
/* 070448 7F03B918 AFA80018 */  sw    $t0, 0x18($sp)
/* 07044C 7F03B91C 00002025 */  move  $a0, $zero
/* 070450 7F03B920 02002825 */  move  $a1, $s0
/* 070454 7F03B924 8FA6055C */  lw    $a2, 0x55c($sp)
/* 070458 7F03B928 24070001 */  li    $a3, 1
/* 07045C 7F03B92C AFA00010 */  sw    $zero, 0x10($sp)
/* 070460 7F03B930 AFA20014 */  sw    $v0, 0x14($sp)
/* 070464 7F03B934 0FC27094 */  jal   explosionCreate
/* 070468 7F03B938 AFA0001C */   sw    $zero, 0x1c($sp)
.L7F03B93C:
/* 07046C 7F03B93C 3C0141D0 */  li    $at, 0x41D00000 # 26.000000
/* 070470 7F03B940 44810000 */  mtc1  $at, $f0
/* 070474 7F03B944 C7B001BC */  lwc1  $f16, 0x1bc($sp)
/* 070478 7F03B948 C6080000 */  lwc1  $f8, ($s0)
/* 07047C 7F03B94C C60A0004 */  lwc1  $f10, 4($s0)
/* 070480 7F03B950 46100482 */  mul.s $f18, $f0, $f16
/* 070484 7F03B954 02002025 */  move  $a0, $s0
/* 070488 7F03B958 46124181 */  sub.s $f6, $f8, $f18
/* 07048C 7F03B95C C6120008 */  lwc1  $f18, 8($s0)
/* 070490 7F03B960 E6060000 */  swc1  $f6, ($s0)
/* 070494 7F03B964 C7A401C0 */  lwc1  $f4, 0x1c0($sp)
/* 070498 7F03B968 46040402 */  mul.s $f16, $f0, $f4
/* 07049C 7F03B96C 46105201 */  sub.s $f8, $f10, $f16
/* 0704A0 7F03B970 E6080004 */  swc1  $f8, 4($s0)
/* 0704A4 7F03B974 C7A601C4 */  lwc1  $f6, 0x1c4($sp)
/* 0704A8 7F03B978 46060102 */  mul.s $f4, $f0, $f6
/* 0704AC 7F03B97C 46049281 */  sub.s $f10, $f18, $f4
/* 0704B0 7F03B980 0FC19ED3 */  jal   gunSetTracerTarget
/* 0704B4 7F03B984 E60A0008 */   swc1  $f10, 8($s0)
/* 0704B8 7F03B988 3C0141D0 */  li    $at, 0x41D00000 # 26.000000
/* 0704BC 7F03B98C 44810000 */  mtc1  $at, $f0
/* 0704C0 7F03B990 12200005 */  beqz  $s1, .L7F03B9A8
/* 0704C4 7F03B994 02002025 */   move  $a0, $s0
/* 0704C8 7F03B998 44060000 */  mfc1  $a2, $f0
/* 0704CC 7F03B99C 24050001 */  li    $a1, 1
/* 0704D0 7F03B9A0 0FC28F87 */  jal   bullet_spark_create
/* 0704D4 7F03B9A4 87A70546 */   lh    $a3, 0x546($sp)
.L7F03B9A8:
/* 0704D8 7F03B9A8 8FBF0034 */  lw    $ra, 0x34($sp)
.L7F03B9AC:
/* 0704DC 7F03B9AC 8FB00028 */  lw    $s0, 0x28($sp)
/* 0704E0 7F03B9B0 8FB1002C */  lw    $s1, 0x2c($sp)
/* 0704E4 7F03B9B4 8FB20030 */  lw    $s2, 0x30($sp)
/* 0704E8 7F03B9B8 03E00008 */  jr    $ra
/* 0704EC 7F03B9BC 27BD0578 */   addiu $sp, $sp, 0x578
)
#endif


/**
 * Address: 7F03B9C0
 *
 * Hitscans gather candidate hits along the bullet path. This function records each candidate hit into shotdata
 * and enforces pentration limits and removes hits that should be blocked by closer objects.
 */
void chrpropAddBulletHit(struct ShotData *shotdata, PropRecord *prop, f32 dist, s32 hitpart, ModelNode *node, struct HitThing *hitthing, s32 room, s32 unk44, Model *model, bool countsAsPenetration, s32 blocksFurtherHits)
{
    s32 pad;
    s32 i;
    s32 furthestindex;
    f32 prevfurthest;
    s32 numPenetratedObjects;
    f32 furthest;
    struct ShotData *localshot; // Assigned but never used, required for matching.

    /**
     * If countsAsPenetration is true, then this hit is on an object that bullets may pass through,
     * and it counts against the weapon's shoot-through object limit.
     */
    if (countsAsPenetration)
    {
        furthest = 0.0f;
        prevfurthest = furthest;
        furthestindex = 0;
        numPenetratedObjects = 0;
        localshot = shotdata;

        for (i = 0; i < ARRAYCOUNT(shotdata->hits); i++)
        {
            if (shotdata->hits[i].prop != NULL && shotdata->hits[i].countsAsPenetration)
            {
                numPenetratedObjects++;

                if (furthest < shotdata->hits[i].dist)
                {
                    prevfurthest = furthest;
                    furthest = shotdata->hits[i].dist;
                    furthestindex = i;
                }
            }
        }

        /**
         * The bullet has reached the max number of objects it can penetrate.
         */
        if (numPenetratedObjects >= bondwalkItemGetObjectsShootThrough(shotdata->weapon))
        {
            // Make room for this new hit.
            shotdata->hits[furthestindex].prop = NULL;
            shotdata->maxdist = prevfurthest;

            // Update the shot's useful distance.
            if (prevfurthest < dist)
            {
                shotdata->maxdist = dist;
            }

            // Remove hits that are beyond the penetration limit.
            for (i = 0; i < ARRAYCOUNT(shotdata->hits); i++)
            {
                if (shotdata->hits[i].prop != NULL && (!shotdata->hits[i].countsAsPenetration) && prevfurthest < shotdata->hits[i].dist)
                {
                    shotdata->hits[i].prop = NULL;
                }
            }
        }
        else
        {
            /**
             * This hit is the final allowed penetrable object.
             */
            if (numPenetratedObjects + 1 == bondwalkItemGetObjectsShootThrough(shotdata->weapon))
            {
                if (dist < shotdata->maxdist)
                {
                    shotdata->maxdist = dist;
                }
            }
        }
    }

    /**
     * If true, this stops the bullets for all weapons except the Cougar Magnum and Silver PP7.
     * Any already recorded hits farther than this one are removed,
     * and the shot's max distance is clamped to this hit distance.
     * Used by bulletproof glass.
    */
    if (blocksFurtherHits)
    {
        if (shotdata->weapon != ITEM_RUGER && shotdata->weapon != ITEM_SILVERWPPK)
        {
            for (i = 0; i < ARRAYCOUNT(shotdata->hits); i++)
            {
                if (shotdata->hits[i].prop != NULL && dist < shotdata->hits[i].dist)
                {
                    shotdata->hits[i].prop = NULL;
                }
            }

            shotdata->maxdist = dist;
        }
    }

    for (i = 0; i < ARRAYCOUNT(shotdata->hits); i++)
    {
        if (shotdata->hits[i].prop == NULL)
        {
            shotdata->hits[i].dist = dist;
            shotdata->hits[i].prop = prop;
            shotdata->hits[i].hitpart = hitpart;
            shotdata->hits[i].node = node;
            shotdata->hits[i].hit = *hitthing;
            shotdata->hits[i].room = room;
            shotdata->hits[i].unk44 = unk44;
            shotdata->hits[i].model = model;
            shotdata->hits[i].countsAsPenetration = countsAsPenetration;
            break;
        }
    }
}


void chraiFistAttackHandler(s32 hand, s32 item_id)
{
    PropRecord *playerprop;
    f32 ducking;
    s32 hit;
    PropRecord *prop;
    PropRecord **propptr;
    ChrRecord *chr;
    f32 max0;
    f32 min0;
    f32 max1;
    f32 min1;
    f32 max2;
    f32 min2;
    s32 hitpart;
    StandTile *tile;
    coord3d from;
    coord3d vector;
    f32 reach;

    hit = 0;
    playerprop = getCurrentPlayerProp();
    ducking = bondviewGetPlayerDuckingHeightRelated(g_CurrentPlayer);

    for (propptr = g_LastOnScreenProp - 1; propptr >= g_OnScreenPropList; propptr--)
    {
        prop = *propptr;

        if (prop == NULL)
        {
            continue;
        }

        if (!(prop->zDepth < 500.0f))
        {
            continue;
        }

        if (prop->type != PROP_TYPE_CHR)
        {
            if (prop->type != PROP_TYPE_VIEWER)
            {
                continue;
            }

            if (prop->chr == NULL)
            {
                continue;
            }

            if (getPlayerPointerIndex(prop) == get_cur_playernum())
            {
                continue;
            }
        }

        reach = 50.0f;
        chr = prop->chr;

        if (getCurrentWeaponOrItem() == ITEM_SNIPERRIFLE)
        {
            reach = 100.0f;
        }

        modelGetAxisExtents(chr->model, &max0, &min0, 0);

        if (!(0.0f <= max0))
        {
            continue;
        }

        if (!(min0 <= 0.0f))
        {
            continue;
        }

        modelGetAxisExtents(chr->model, &max1, &min1, 1);

        if (!(0.0f <= max1))
        {
            continue;
        }

        if (!(min1 <= 0.0f))
        {
            continue;
        }

        modelGetAxisExtents(chr->model, &max2, &min2, 2);

        if (!(min2 <= 0.0f))
        {
            continue;
        }

        if (!((-reach) <= max2))
        {
            continue;
        }

        tile = playerprop->stan;

        if (!stanTestLineUnobstructed(&tile, playerprop->pos.x, playerprop->pos.z, prop->pos.x, prop->pos.z, CDTYPE_OBJS | CDTYPE_DOORS | CDTYPE_PATHBLOCKER, ducking, ducking, 0.0f, 1.0f))
        {
            continue;
        }

        if (tile != prop->stan) {
            continue;
        }

        hitpart = HIT_CHEST;

        if (currentPlayerGetCrouchPos() == CROUCH_HALF) {
            hitpart = HIT_GENERAL;
        }
        else if (currentPlayerGetCrouchPos() == CROUCH_SQUAT)
        {
            hitpart = HIT_GENERALHALF;
        }

        if (g_musicSfxBufferPtr && g_musicSfxBufferPtr);

        bullet_path_from_screen_center(&from, &vector, hand);
        mtx4RotateVecInPlace(currentPlayerGetViewToWorldMtxf(), &vector);

        if (handles_shot_actors(chr, hitpart, &vector, item_id, 1))
        {
            recall_joy2_hits_edit_detail_edit_flag(item_id, prop, -1);
            hit = 1;
        }
    }

    if ((!hit) && (item_id == ITEM_FIST))
    {
        sndPlaySfx(g_musicSfxBufferPtr, PUNCHING_AIR_SFX, 0);
    }
}


void chraiDefaultWeaponFireHandler(s32);

/**
 * Address 0x7F03C0F0.
*/
void chraiCheckUseHeldItem(s32 hand)
{
    s32 item_id;
    s32 i;

    if (get_hands_firing_status(hand) != 0)
    {
        item_id = getCurrentPlayerWeaponId(hand);

        if (item_id == ITEM_TRIGGER)
        {
            trigger_remote_mine_detonation();
        }
        else if (item_id == ITEM_GRENADELAUNCH
            || item_id == ITEM_ROCKETLAUNCH
            || item_id == ITEM_GRENADE
            || item_id == ITEM_THROWKNIFE
            || item_id == ITEM_REMOTEMINE
            || item_id == ITEM_PROXIMITYMINE
            || item_id == ITEM_TIMEDMINE
            || item_id == ITEM_FLAREPISTOL
            || item_id == ITEM_PITONGUN
            || item_id == ITEM_BOMBCASE
            || item_id == ITEM_BUG
            || item_id == ITEM_MICROCAMERA
            || item_id == ITEM_GOLDENEYEKEY
            || item_id == ITEM_TOKEN
            || item_id == ITEM_PLASTIQUE
        )
        {
            // nothing to do
        }
        else if (item_id == ITEM_TANKSHELLS)
        {
            gunFireTankShell(hand);
        }
        else if (item_id == ITEM_FIST || item_id == ITEM_KNIFE)
        {
            chraiFistAttackHandler(hand, item_id);
        }
        else if (item_id == ITEM_SHOTGUN || item_id == ITEM_AUTOSHOT)
        {
            inc_curplayer_hitcount_with_weapon(item_id, SHOT_REGISTER_TOTAL);

            for (i=0; i<NUMBER_SHOTGUN_BULLETS; i++)
            {
                chraiDefaultWeaponFireHandler(hand);
            }
        }
        else if (item_id == ITEM_CAMERA)
        {
            objectiveTakePictureHandler();
        }
        else if (item_id == ITEM_WATCHMAGNETATTRACT)
        {
            g_CurrentPlayer->magnetattracttime = 0;
        }
        else
        {
            inc_curplayer_hitcount_with_weapon(item_id, SHOT_REGISTER_TOTAL);
            chraiDefaultWeaponFireHandler(hand);
        }
    }
}


/**
 * Address 0x7F03C294.
*/
void chraiCheckUseHeldItems(void)
{
    chraiCheckUseHeldItem(GUNRIGHT);
    chraiCheckUseHeldItem(GUNLEFT);
}


void propExecuteTickOperation(PropRecord *prop, INV_ITEM_TYPE type)
{
    ObjectRecord *propobj;

    if (type == INV_ITEM_WEAPON)
    {
        if ((prop->type == PROP_TYPE_WEAPON) || (prop->type == PROP_TYPE_OBJ))
        {
            propobj = prop->obj;
            if (prop->obj->state & PROPSTATE_RESPAWN) //matches only if called directly (not propobj)
            {
                #ifndef VERSION_EU
                prop->timetoregen = 0x4B0;
                #else
                prop->timetoregen = 0x3E8;
                #endif
                propobj->runtime_bitflags |= RUNTIMEBITFLAG_00000800;
                propobj->runtime_bitflags &= ~RUNTIMEBITFLAG_REMOVE;
                propobj->state &= ~0x80;
                propobj->maxdamage = 0.0f;
                chrpropDeregisterRooms(prop);
                chrpropDisable(prop);
                return;
            }
        }
        chrpropDeregisterRooms(prop);
        chrpropDelist(prop);
        chrpropDisable(prop);
        chrpropFree(prop);
    }
    else if (type == INV_ITEM_PROP)
    {
        chrpropDeregisterRooms(prop);
        chrpropDelist(prop);
        chrpropDisable(prop);
    }
    else if (type == INV_ITEM_PICKUP)
    {
        chrpropDeregisterRooms(prop);
        chrpropDelist(prop);
        chrpropDisable(prop);
        objDetach(prop);
        objFreeEmbedmentOrProjectile(prop);
        chrpropReparent(prop, getCurrentPlayerProp());
    }
}


PropRecord *propFindForInteract(void)
{
    PropRecord **ptr;
    s32 i;
    bool checkmore = TRUE;

    g_InteractProp = NULL;

    // Iterate onscreen list near to far
    for (ptr = g_LastOnScreenProp - 1; ptr >= g_OnScreenPropList; ptr--)
    {
        PropRecord *prop = *ptr;

        if (prop)
        {
            if (prop->type == PROP_TYPE_CHR)
            {
                // empty
            }
            else if (prop->type == PROP_TYPE_OBJ || prop->type == PROP_TYPE_WEAPON)
            {
                checkmore = objTestForInteract(prop);
            }
            else if (prop->type == PROP_TYPE_DOOR)
            {
                checkmore = doorTestForInteract(prop);
            }
            else if (prop->type == PROP_TYPE_EXPLOSION)
            {
                // empty
            }
            else if (prop->type == PROP_TYPE_SMOKE)
            {
                // empty
            }

            if (!checkmore)
            {
                break;
            }
        }
    }

    return g_InteractProp;
}


bool bond_interact_object(void)
{
    PropRecord *prop;
    bool op;

    prop = propFindForInteract();
    op = INV_ITEM_NONE;

    if (prop)
    {
        switch (prop->type)
        {
            case PROP_TYPE_OBJ:
            case PROP_TYPE_WEAPON:
                op = propobjInteract(prop);
                break;
            case PROP_TYPE_DOOR:
                op = propdoorInteract(prop);
                break;
            case PROP_TYPE_CHR:
            case PROP_TYPE_PLAYER:
            case PROP_TYPE_EXPLOSION:
            case PROP_TYPE_SMOKE:
                break;
        }

        propExecuteTickOperation(prop, op);

        return FALSE;
    }

    return TRUE;
}


/**
* Returns true when the given prop isn't within 400 units of any player prop.
*/
s32 chrpropIsFarFromPlayers(PropRecord* prop)
{
    PropRecord* player_prop;
    coord3d pos_diff;
    s32 uninitialized; // needed for match
    s32 rc;
    s32 i;
    s32 player_count;

    player_count = getPlayerCount();
    rc = 1;

    for (i = 0; i < player_count; i++)
    {
        player_prop = g_playerPointers[i]->prop;
        pos_diff.x = player_prop->pos.x - prop->pos.x;
        pos_diff.y = player_prop->pos.y - prop->pos.y;
        pos_diff.z = player_prop->pos.z - prop->pos.z;
        if (sqrtf((pos_diff.x * pos_diff.x) + (pos_diff.y * pos_diff.y) + (pos_diff.z * pos_diff.z)) < 400.0f)
        {
            rc = 0;
            break;
        }
    }

    return rc;
}


/**
 * Per-frame tick for chrprop-managed props.
 *
 * 1) Advance all AI act states.
 * 2) Update NPC bullet tracers.
 * 3) Handle MP pickup respawns.
 * 4) Update autogun bullet tracers.
 * 5) Tick explosions and smoke.
 * 6) Update MP character bullet tracers.
 * 7) Handle prop delisting or activation.
 */
void chrpropTick(void)
{
    PropRecord *prop;
    ObjectRecord *obj;
    PropRecord *prev;
    PropRecord *next;
    ChrRecord *chr;
    bool skip_regen_sfx;
    s32 tickop;
    bool is_under_60;
    struct ObjectRecord *autogun;
    s32 cmdindex;
    s32 pad;
    ObjectRecord *setupobj;

    // Advance AI states e.g. attacking, walking, dying, etc...
    chrlvAllChrTick();

    prop = get_ptr_obj_pos_list_current_entry();

    while (prop != NULL)
    {
        prev = prop->prev;
        tickop = 0;
        if (prop->type == PROP_TYPE_CHR)
        {
            chr = prop->chr;
            // Update NPC bullet tracers.
            gunAdvanceBeamTimer(&chr->beams[0]);
            gunAdvanceBeamTimer(&chr->beams[1]);
        }
        else if (((prop->type == PROP_TYPE_OBJ) || (prop->type == PROP_TYPE_WEAPON)) || (prop->type == PROP_TYPE_DOOR))
        {
            obj = prop->obj;
            skip_regen_sfx = FALSE;

            if (prop->timetoregen > 0)
            {
                is_under_60 = TRUE;

                if (prop->timetoregen >= CHROBJ_TIMETOREGEN)
                {
                    is_under_60 = FALSE;
                }

                prop->timetoregen -= g_ClockTimer;

                if (prop->timetoregen < CHROBJ_TIMETOREGEN)
                {
                    if (!is_under_60)
                    {
                        if (!chrpropIsFarFromPlayers(prop))
                        {
                            prop->timetoregen += CHROBJ_TIMETOREGEN;
                        }
                    }
                }

                if (prop->timetoregen <= 0)
                {
                    prop->timetoregen = 0;
                    if (obj->state & PROPSTATE_10)
                    {
                        obj->runtime_bitflags |= RUNTIMEBITFLAG_00001000;
                    }
                    else
                    {
                        obj->runtime_bitflags &= ~RUNTIMEBITFLAG_00001000;
                    }
                }
                else if ((prop->timetoregen < CHROBJ_TIMETOREGEN) && (!is_under_60))
                {
                    if ((obj->maxdamage == 0.0f) && (!(obj->state & PROPSTATE_DESTROYED)))
                    {
                        if (obj->flags & PROPFLAG_INSIDEANOTHEROBJ)
                        {
                            chrpropDeregisterRooms(prop);
                            chrpropDelist(prop);

                            obj->runtime_bitflags &= ~RUNTIMEBITFLAG_00000800;
                            cmdindex = setupGetCommandIndexByProp(prop);
                            pad = obj->pad;
                            setupobj = setupCommandGetObject(lvlGetCurrentStageToLoad(), cmdindex + pad);

                            if ((setupobj != NULL) && (setupobj->prop != NULL))
                            {
                                modelSetScale(obj->model, obj->model->scale);
                                chrpropReparent(obj->prop, setupobj->prop);
                                skip_regen_sfx = TRUE;
                            }
#ifdef DEBUG
                            else
                            {
                                osSyncPrintf("inobj link not found for object number %d\n", cmdindex + 1);
                            }
#endif
                        }
                        else
                        {
                            chrpropEnable(prop);
                            sub_GAME_7F03E134(prop);
                            obj->runtime_bitflags &= ~RUNTIMEBITFLAG_00000800;
                        }
                    }
                    else
                    {
                        if (obj->state & PROPSTATE_EXT_COLISION_BLOCK)
                        {
                            obj->flags |= PROPFLAG_00000100;
                        }
                        else
                        {
                            obj->flags &= ~PROPFLAG_00000100;
                        }

                        obj->maxdamage = 0.0f;
                        obj->state &= ~PROPSTATE_DESTROYED;
                        sub_GAME_7F050DE8(obj->model);
                    }

                    if (obj->type == PROPDEF_ARMOUR)
                    {
                        ((BodyArmourRecord *) obj)->amount = ((BodyArmourRecord *) obj)->initialamount;
                    }

                    if (!skip_regen_sfx)
                    {
                        chrobjSndCreatePostEventDefault(sndPlaySfx(g_musicSfxBufferPtr, OBJ_REGEN_SFX, NULL), &prop->pos);
                    }

                }
            }

            // Update autogun bullet tracers.
            if (obj->type == PROPDEF_AUTOGUN)
            {
                autogun = prop->obj;
                gunAdvanceBeamTimer((BeamRecord *) ((AutogunRecord *) autogun)->beam);
            }
        }
        else if (prop->type == PROP_TYPE_EXPLOSION)
        {
            tickop = explosionTick(prop);
        }
        else if (prop->type == PROP_TYPE_SMOKE)
        {
            tickop = explosionSmokeTick(prop);
        }
        else
        {
            if (prop->type == PROP_TYPE_VIEWER)
            {
                s32 playernum;
                playernum = getPlayerPointerIndex(prop);
                gunAdvanceBeamTimer(&g_playerPointers[playernum]->hands[0].weapon_beam);
                playernum = getPlayerPointerIndex(prop);
                gunAdvanceBeamTimer(&g_playerPointers[playernum]->hands[1].weapon_beam);

                // Update MP character bullet tracers.
                if (prop->chr != NULL)
                {
                    if (getPlayerCount() >= 2)
                    {
                        chr = prop->chr;
                        gunAdvanceBeamTimer(&chr->beams[0]);
                        gunAdvanceBeamTimer(&chr->beams[1]);
                    }
                }
            }
        }

        if (tickop == 5)
        {
          next = prev;
        }
        else
        {
            next = prop->prev;
            if (tickop == 3)
            {
                chrpropDelist(prop);
                chrpropActivateThisFrame(prop);

                if (next == NULL)
                {
                    next = prop;
                }
            }
            else
            {
                propExecuteTickOperation(prop, tickop);
            }
        }

        prop = next;
    }
}


/*
* Address: 0x7F03CA30
* PD: propsTick (src/game/proptick.c)
*/
void determing_type_of_object_and_detection(void)
{
    s32 tickop;
    PropRecord *prop;
    PropRecord *prev;
    PropRecord *propprev;

    prop = get_ptr_obj_pos_list_current_entry();

    while (prop != NULL)
    {
        tickop = 0;
        prev = prop->prev;

        if (prop->type == PROP_TYPE_CHR)
        {
            tickop = chrTickBeams(prop);
        }
        else if ((prop->type == PROP_TYPE_OBJ) || (prop->type == PROP_TYPE_WEAPON) || (prop->type == PROP_TYPE_DOOR))
        {
            tickop = object_interaction(prop);
        }
        else if (prop->type == PROP_TYPE_EXPLOSION)
        {
            tickop = explosionChrpropExplosionTick(prop);
        }
        else if (prop->type == PROP_TYPE_SMOKE)
        {
            tickop = explosionChrpropSmokeTick(prop);
        }
        else if (prop->type == PROP_TYPE_VIEWER)
        {
            tickop = playerTickBeams(prop);
        }

		if (tickop == 5)
        {
			propprev = prev;
		}
        else
        {
			propprev = prop->prev;

			if (tickop == 3)
            {
				chrpropDelist(prop);
				chrpropActivateThisFrame(prop);

				if (propprev == NULL)
                {
					propprev = prop;
				}
			}
            else
            {
				propExecuteTickOperation(prop, tickop);
			}
		}

		prop = propprev;
    }

    if (get_player_position_in_shuffled(get_cur_playernum()) == 0)
    {
        handle_alarm_gas_timer_calldamage();
        loop_set_sound_effect_all_slots();
        propsDefragRoomProps();
    }
}






/**
 * Copies stan roomids from prop to array. The list is terminated
 * with an entry of -1.
 *
 * @param self: prop
 * @param roomids: out parameter. Must contain enough space to store room ids.
 *
 * Address 0x7F03CB8C.
*/
void chraiGetPropRoomIds(PropRecord *self, s32 *roomids)
{
    StandTile *stan;
    s32 i;

    stan = self->stan;

    if (stan == NULL)
    {
        roomids[0] = -1;
    }
    else if ((self->type == PROP_TYPE_VIEWER) && (self->obj == NULL))
    {
        roomids[0] = stan->room;
        roomids[1] = -1;
    }
    else
    {
        for (i=0; self->rooms[i] != 0xff; i++)
        {
            roomids[i] = self->rooms[i];
        }

        roomids[i] = -1;
    }
}




/**
 * @param arg0:
 * @param arg1: out parameter. Bounding coords (x,z) by (x,z).
 * @param arg2: out parameter.
 * @param arg3: out parameter. Maybe ymin. (ground)
 * @param arg4: out parameter. Maybe ymax. (ground + chr/object height)
 *
 * Address 0x7F03CC20.
*/
void chraiGetCollisionBounds(PropRecord *prop, struct rect4f **polygon, s32 *edges, f32 *top, f32 *bottom)
{
    *polygon = NULL;
    *edges = 0;

    if (prop->type == PROP_TYPE_CHR)
    {
        chrUpdateCollisionBounds(prop, polygon, edges, top, bottom);
    }
    else if (prop->type == PROP_TYPE_VIEWER)
    {
        bondviewGetPropHeightRelatedValues(prop, polygon, edges, top, bottom);
    }
    else if (prop->type == PROP_TYPE_WEAPON)
    {
        // nothing to do
    }
    else if ((prop->type == PROP_TYPE_OBJ) || (prop->type == PROP_TYPE_DOOR))
    {
        sub_GAME_7F04F244(prop, polygon, edges, top, bottom);
    }
    else if (prop->type == PROP_TYPE_PLAYER)
    {
        // nothing to do
    }
    else if (prop->type == PROP_TYPE_NUL)
    {
        // nothing to do
    }

    return;
}





/**
 * Same as @see chraiGetCollisionBounds, but throws away arg3 and arg4.
 *
 * @param arg0:
 * @param arg1: out parameter. Bounding coords (x,z) by (x,z).
 * @param arg2: out parameter.
 *
 * Address 0x7F03CCB0.
*/
void chraiGetCollisionBoundsWithoutY(PropRecord *prop, struct rect4f **polygon, s32 *edges)
{
    f32 sp24;
    f32 sp20;

    chraiGetCollisionBounds(prop, polygon, edges, &sp24, &sp20);
}





/**
 * @param point: 3d point to test if inside polygon. Only uses (x,z).
 * @param polygon: Convex polygon. Iterates edges and checks that
 * point is oriented correctly inside all of them.
 * @param edges: Number of edges to iterate in polygon.
 * Address 0x7F03CCD8.
*/
s32 chrpropTestPointInPolygon(coord3d *point, struct rect4f *polygon, s32 edges)
{
    /**
     * Stack overflow:
     *
     * In any case, for any convex polygon (including rectangle) the test is
     * very simple: check each edge of the polygon, assuming each edge is
     * oriented in counterclockwise direction, and test whether the point lies
     * to the left of the edge (in the left-hand half-plane). If all edges pass
     * the test - the point is inside. If at least one fails - the point is outside.
     *
     * In order to test whether the point (xp, yp) lies on the left-hand
     * side of the edge (x1, y1) - (x2, y2), you just need to calculate
     *
     * D = (x2 - x1) * (yp - y1) - (xp - x1) * (y2 - y1)
     *
     * https://stackoverflow.com/a/2752753/1462295
    */

    /**
     * Assuming the above is correct, I think that means rectangles (polygons)
     * are clockwise oriented.
    */

    f32 diff;
    s32 i;
    s32 ret = -1;

    if (edges <= 0)
    {
        return 0;
    }

    for (i=0; i<edges; i++)
    {
        // curse you compiler loop unroller
        diff = (    (polygon->points[(i+1) % edges].f[1] - polygon->points[i].f[1]) * (point->f[0] - polygon->points[i].f[0]))
                 - ((polygon->points[(i+1) % edges].f[0] - polygon->points[i].f[0]) * (point->f[2] - polygon->points[i].f[1]));

        if (diff != 0.0f)
        {
            if (i == 0 || ret < 0)
            {
                ret = (diff > 0.0f);

                continue;
            }

            if ((ret != 0) && (diff < 0.0f))
            {
                return 0;
            }

            if ((ret == 0) && (diff > 0.0f))
            {
                return 0;
            }
        }
    }

    return 1;
}





/**
 * @param arg0: prop
 * @param collision_radius: out parameter, will be set to character width or player collision radius.
 * @param height: out parameter, will be set to height
 * @param always_20: out parameter, will be set to either 20 or 30.
 *
 * Address 0x7F03CF88.
*/
void chrpropGetCollisionBounds(PropRecord *arg0, f32 *collision_radius, f32 *height, f32 *arg3)
{
    if (arg0->type == PROP_TYPE_CHR)
    {
        chrGetChrWidthHeight(arg0, collision_radius, height, arg3);
        return;
    }

    if (arg0->type == PROP_TYPE_VIEWER)
    {
        bondviewGetCollisionRadius(arg0, collision_radius, height, arg3);
        return;
    }

    *collision_radius = 0.0f;
}




/**
 * Address 0x7F03CFE8.
*/
f32 sub_GAME_7F03CFE8(PropRecord *arg0)
{
    if (arg0->type == PROP_TYPE_CHR)
    {
        return chrGetChrGround(arg0);
    }

    if (arg0->type == PROP_TYPE_VIEWER)
    {
        return bondviewGetPlayerStanHeight(g_playerPointers[getPlayerPointerIndex(arg0)]);
    }

    return 0.0f;
}






void sub_GAME_7F03D058(PropRecord *prop, bool unset) //#MATCH
{
    if (prop->type == PROP_TYPE_CHR)
    {
        chrSetMoving(prop->chr, unset);
    }
    else if (prop->type == PROP_TYPE_VIEWER)
    {
        bondviewUpdateGuardTankFlagsRelated(prop, unset);
    }
    else if ((prop->type == PROP_TYPE_OBJ) || (prop->type == PROP_TYPE_DOOR) || (prop->type == PROP_TYPE_WEAPON))
    {
        sub_GAME_7F04F218(prop, unset);
    }
}



/**
 * NTSC address: 0x7F03D0D4.
*/
void sub_GAME_7F03D0D4(void)
{
    PropRecord *prop;
    PropRecord *propprev;
    bool isCollected = FALSE;

    if (!isBondInTank() && !g_PlayerInvincible)
    {
        //for each prop in setup

        for (prop = get_ptr_obj_pos_list_current_entry(); prop != NULL; prop = propprev)
        {
            isCollected = 0;

            if (prop->timetoregen <= 0)
            {
                switch (prop->type)
                {
                    case PROP_TYPE_DOOR:
                    case PROP_TYPE_CHR:
                    case PROP_TYPE_PLAYER:
                    case PROP_TYPE_VIEWER:
                    case PROP_TYPE_EXPLOSION:
                    case PROP_TYPE_SMOKE:
                         break;

                    case PROP_TYPE_OBJ:
                        isCollected = object_collectability_routines(prop);
                        break;

                    case PROP_TYPE_WEAPON:
                        isCollected = redirect_object_collectability_routines(prop);
                        break;
                }
            }
            propprev = prop->prev; //not sure why rare put this here and not in the for statement

            propExecuteTickOperation(prop, isCollected);
        }
    }
}


/**
 * Address: 7F03D188
 *
 * Calculates an auto-aim score for a given prop based roughly on how close it is to the center of the screen.
 * Higher scores are better, with 1.0 being the best possible score.
 */
f32 chrpropScoreAutoAimTarget(PropRecord *targetprop, coord3d *aimpos, f32 *world_xbounds, f32 *world_ybounds, coord2d *out_screen)
{
    f32 aim_screen[2];
    coord3d testpos;
    f32 screen_left_edge[2];
    f32 screen_right_edge[2];
    f32 screen_top_edge[2];
    f32 screen_bottom_edge[2];
    f32 crosshair_x;
    f32 crosshair_y;
    f32 autoaim_top;
    f32 autoaim_bottom;
    f32 autoaim_left;
    f32 autoaim_right;
    f32 score;
    bool passes_horizontal_check;
    f32 horizontal_tolerance;
    PropRecord *playerprop;
    StandTile* line_stan;
    f32 player_los_height;

    /**
     * Define a central auto-aim acceptance region.
     * The sweet spot is 65% vertically in favor of the top of the screen and 50% horizontally.
     */
    autoaim_top = getPlayer_c_screentop() + getPlayer_c_screenheight() * 0.175f;
    autoaim_bottom = getPlayer_c_screentop() + getPlayer_c_screenheight() * 0.825f;
    autoaim_left = getPlayer_c_screenleft() + getPlayer_c_screenwidth() * 0.25f;
    autoaim_right = getPlayer_c_screenleft() + getPlayer_c_screenwidth() * 0.75f;

    score = -2.0f;

    transform3Dto2DCoords(aimpos, (coord3d*)aim_screen);
    testpos.x = world_xbounds[0];
    testpos.y = aimpos->y;
    testpos.z = aimpos->z;
    transform3Dto2DCoords(&testpos, (coord3d*)screen_left_edge);
    testpos.x = world_xbounds[1];
    testpos.y = aimpos->y;
    testpos.z = aimpos->z;
    transform3Dto2DCoords(&testpos, (coord3d*)screen_right_edge);
    testpos.x = aimpos->x;
    testpos.y = world_ybounds[1];
    testpos.z = aimpos->z;
    transform3Dto2DCoords(&testpos, (coord3d*)screen_top_edge);
    testpos.x = aimpos->x;
    testpos.y = world_ybounds[0];
    testpos.z = aimpos->z;
    transform3Dto2DCoords(&testpos, (coord3d*)screen_bottom_edge);

    if (screen_bottom_edge[1] >= autoaim_top && autoaim_bottom >= screen_top_edge[1])
    {
        passes_horizontal_check = FALSE;
        get_bullet_angle(&crosshair_x, &crosshair_y);
        screen_left_edge[0] = floorFloat(screen_left_edge[0]);
        screen_right_edge[0] = ceilFloat(screen_right_edge[0]);

        if (currentPlayerGetXAutoAimEnabledRedirect())
        {
            if (screen_left_edge[0] <= autoaim_right && autoaim_left <= screen_right_edge[0])
            {
                horizontal_tolerance = (screen_right_edge[0] - screen_left_edge[0]) * 1.5f;

                if (getPlayerCount() == 1)
                {
                    horizontal_tolerance = horizontal_tolerance * difficulty;
                }

                passes_horizontal_check = getPlayer_c_screenleft() + 0.5f * getPlayer_c_screenwidth() >= (screen_left_edge[0] + screen_right_edge[0]) * 0.5f - horizontal_tolerance
                    && getPlayer_c_screenleft() + 0.5f * getPlayer_c_screenwidth() <= (screen_left_edge[0] + screen_right_edge[0]) * 0.5f + horizontal_tolerance
                    && autoaim_left <= aim_screen[0]
                    && autoaim_right >= aim_screen[0];
            }
        }
        else
        {
            passes_horizontal_check = screen_left_edge[0] <= crosshair_x && crosshair_x <= screen_right_edge[0];
        }

        if (passes_horizontal_check)
        {
            playerprop = getCurrentPlayerProp();
            line_stan = playerprop->stan;
            player_los_height = bondviewGetPlayerDuckingHeightRelated(g_CurrentPlayer);
            bondviewUpdateGuardTankFlagsRelated(playerprop, FALSE);

            // Can auto-aim see the target?
            if ((stanTestLineUnobstructed(&line_stan, playerprop->pos.f[0], playerprop->pos.f[2], targetprop->pos.f[0], targetprop->pos.f[2], CDTYPE_OBJS | CDTYPE_DOORS | CDTYPE_PATHBLOCKER, player_los_height, player_los_height, 0.0f, 1.0f) != 0))
            {
                if (line_stan == targetprop->stan)
                {
                    f32 clamped_screen_y = aim_screen[1];

                    if (clamped_screen_y < autoaim_top)
                    {
                        clamped_screen_y = autoaim_top;
                    }
                    else if (clamped_screen_y > autoaim_bottom)
                    {
                        clamped_screen_y = autoaim_bottom;
                    }

                    out_screen->y = clamped_screen_y;

                    if (currentPlayerGetXAutoAimEnabledRedirect())
                    {
                        f32 clamped_screen_x = aim_screen[0];

                        if (clamped_screen_x < autoaim_left)
                        {
                            clamped_screen_x = autoaim_left;
                        }
                        else if (clamped_screen_x > autoaim_right)
                        {
                            clamped_screen_x = autoaim_right;
                        }

                        out_screen->x = clamped_screen_x;
                    }

                    /** If the screen's center x-coord overlaps the target's horizontal span, give it the best possible score of 1.0.
                     *  If this happens, this function's caller, chrpropUpdateAutoaimTarget, treats this as the winning prop and stops searching.
                     */
                    if (getPlayer_c_screenleft() + 0.5f * getPlayer_c_screenwidth() >= screen_left_edge[0] && getPlayer_c_screenleft() + 0.5f * getPlayer_c_screenwidth() <= screen_right_edge[0])
                    {
                        score = 1.0f;
                    }
                    // If the target is towards the left side of the screen, penalize it based on how far towards the left.
                    else if (getPlayer_c_screenleft() + 0.5f * getPlayer_c_screenwidth() >= screen_left_edge[0])
                    {
                        score = 1.0f - ((getPlayer_c_screenleft() + 0.5f * getPlayer_c_screenwidth()) - screen_right_edge[0]) / horizontal_tolerance;
                    }
                    // If the target is towards the right side of the screen, penalize it based on how far towards the right.
                    else
                    {
                        score = 1.0f - (screen_left_edge[0] - (getPlayer_c_screenleft() + 0.5f * getPlayer_c_screenwidth())) / horizontal_tolerance;
                    }
                }
            }

            bondviewUpdateGuardTankFlagsRelated(playerprop, TRUE);
        }
    }

    return score;
}


/**
 * Iterates on screen props to find autoaim target.
 *
 * US address 7F03D78C.
*/
void chrpropUpdateAutoaimTarget(void)
{
    f32 best_score;
    struct coord2d best_screen_aim; // Winning target's screen space aim point.
    f32 candidate_score;
    struct PropRecord **onscreen_prop_iter;
    struct coord3d target_aimpos;
    struct coord2d target_world_xbounds;
    struct coord2d target_world_ybounds;
    struct PropRecord *candidate_prop;
    struct coord2d candidate_screen_aim;
    struct PropRecord *best_prop;
    struct ChrRecord *candidate_chr;

    best_prop = NULL;
    best_screen_aim = g_DefaultAutoAimCoord;

    if (currentPlayerGetYAutoAimEnabledRedirect() != FALSE)
    {
        best_score = -1.0f;

        // Search all on screen props and record the best target.
        for (onscreen_prop_iter = g_LastOnScreenProp - 1; onscreen_prop_iter >= &g_OnScreenPropList[0]; onscreen_prop_iter--)
        {
            candidate_prop = *onscreen_prop_iter;

            if (candidate_prop == NULL)
            {
                continue;
            }

            if ((candidate_prop->type != PROP_TYPE_CHR)
                    && ((candidate_prop->type != PROP_TYPE_VIEWER)
                        || (candidate_prop->obj == NULL)
                        || (getPlayerPointerIndex(candidate_prop) == get_cur_playernum())))
            {
                continue;
            }

            candidate_chr = candidate_prop->chr;

            // Characters not holding a weapon are exempt from being a target.
            if (((chrGetEquippedWeaponProp(candidate_chr, GUNRIGHT) == 0) && (chrGetEquippedWeaponProp(candidate_chr, GUNLEFT) == 0)))
            {
                continue;
            }

            if ((chrGetOnscreenRenderBounds(candidate_prop, &target_aimpos, &target_world_xbounds, &target_world_ybounds) == 0))
            {
                continue;
            }

            // Score the candidate based on how close it is to the center of the screen.
            candidate_score = chrpropScoreAutoAimTarget(candidate_prop, &target_aimpos, &target_world_xbounds.x, &target_world_ybounds.x, &candidate_screen_aim.x);

            if (best_score < candidate_score)
            {
                best_score = candidate_score;

                best_prop = candidate_prop;
                best_screen_aim.x = candidate_screen_aim.x;
                best_screen_aim.y = candidate_screen_aim.y;

                // If we find a score of 1.0, we can't do any better, so break out of this search.
                if (1.0f <= candidate_score)
                {
                    break;
                }
            }
        }
    }

    if (best_prop != NULL)
    {
        // Fake but needed for matching.
        if (best_screen_aim.x > 1.0f);

        bondviewUpdateYAutoAimTime(best_prop, ((best_screen_aim.y - getPlayer_c_screentop()) / (getPlayer_c_screenheight() * 0.5f)) - 1.0f);

        if (currentPlayerGetXAutoAimEnabledRedirect() != FALSE)
        {
            bondviewUpdateXAutoAimTime(best_prop, ((best_screen_aim.x - getPlayer_c_screenleft()) / (getPlayer_c_screenwidth() * 0.5f)) - 1.0f);
        }
    }
    else
    {
        bondviewUpdateYAutoAimTime(NULL, 0.0f);
        bondviewUpdateXAutoAimTime(NULL, 0.0f);
    }
}


/*
* Address: 7F03D9EC
*/
s32 propDoorGetCdTypes(PropRecord* prop)
{
    s32 var_v1;

    if (prop->door->openPosition <= 0.0f)
    {
        var_v1 = CDTYPE_CLOSEDDOORS;
    }
    else
    {
        var_v1 = (prop->door->maxFrac <= prop->door->openPosition)
            ? CDTYPE_OPENDOORS
            : CDTYPE_AJARDOORS;
    }

    if (((s32)prop->door->flags2 * 4) < 0)
    {
        var_v1 |= CDTYPE_DOORSLOCKEDTOAI;
    }

    return var_v1;
}


/*
* Address: 7F03DA50
* PD: prop_is_of_cd_type
*/
s32 propIsOfCdType(PropRecord* prop, s32 cdtypes) {
    s32 ret;
    ObjectRecord *obj;
    ret = 1;

    if (prop->type == PROP_TYPE_DOOR) {
        if ((cdtypes & CDTYPE_AIOPAQUE)) {
            obj = prop->obj;

            if (obj->flags & PROPFLAG_04000000) {
                ret = 0;
            }
        }

        if (!(cdtypes & CDTYPE_DOORS)) {
            if (!(propDoorGetCdTypes(prop) & cdtypes)) {
                ret = 0;
            }
        }
    } else if (prop->type == PROP_TYPE_VIEWER) {
        if (!(cdtypes & CDTYPE_PLAYERS)) {
            ret = 0;
        }
    } else if (prop->type == PROP_TYPE_CHR) {
        if (!(cdtypes & CDTYPE_CHRS)) {
            ret = 0;
        }
    } else {
        obj = prop->obj;

        if ((cdtypes & CDTYPE_AIOPAQUE) && (obj->flags & PROPFLAG_04000000)) {
            ret = 0;
        }

        if ((cdtypes & CDTYPE_OBJSIMMUNETOEXPLOSIONS) && !(obj->flags & PROPFLAG_INVINCIBLE)) {
            ret = 0;
        }

        if (obj->flags & PROPFLAG_00000800) {
            if (!(cdtypes & CDTYPE_PATHBLOCKER)) {
                ret = 0;
            }
        } else if (!(cdtypes & CDTYPE_OBJS)) {
            ret = 0;
        }
    }

    return ret;
}



/* I think the arguments are lists of roomids but I'm not certain. This function checks if any item in the two lists match */
s32 sub_GAME_7F03DB70(s32* roomids1, s32* roomids2)
{
    s32* itr1;
    s32* itr2;
    s32 itr1_val;
    s32 itr2_val;

    itr1 = roomids1;
    itr1_val = *itr1;
    while (itr1_val >= 0)
    {
        itr2 = roomids2;
        itr2_val = *itr2;
        while(itr2_val >= 0)
        {
            if (itr1_val == itr2_val) { return 1; }
            itr2++;
            itr2_val = *itr2;
        }
        itr1++;
        itr1_val = *itr1;
    }

    return 0;
}


#define MAXBLOCKS 256
#define ROOMLISTMAX 256
/*
* Address: 0x7F03DBCC
* PD: prop_try_add_to_chunk
* block: canonical name
*/
s32 chrpropInsertPropnum(s16 propnum, s32 block)
{
    s32 i;
    #ifdef DEBUG
    assert(block<MAXBLOCKS); //prop.c line 2136
    #endif
    // Note: The size of the propnums array is 16, but we're only iterating over the first 15 elements.
    //       Is this because the last element is always -1? Seems like a waste.
    for (i = 0; i < 15; i++)
    {
        if (RoomPropListBlocks[block].propnums[i] < 0)
        {
            RoomPropListBlocks[block].propnums[i] = propnum;
            return 1;
        }
    }

    return 0;
}



/*
* Address: 0x7F03DCB8
* canonical name newblockforroom
* Description: Find an emtpy chunk that can be assigned to a room
* PD: room_allocate_prop_list_chunk
* room: canonical name
* prevblock: canonical name
*/
s32 chrpropInitializeNewChunkForRoom(s32 room, s32 prevblock)
{
    s32 i;
#ifdef DEBUG
    assert(room < g_MaxNumRooms); // roomnumber
    assert(prevblock<MAXBLOCKS);
#endif
    for (i = 0; i < MAXBLOCKS; i++)
    {
        if (RoomPropListBlocks[i].propnums[0] == -2)
        {
            // This chunk is allowed to be erased
            s32 j;
            for (j = 0; j < 16; j++)
            {
                RoomPropListBlocks[i].propnums[j] = -1;
            }

            if (prevblock >= 0)
            {
                RoomPropListBlocks[prevblock].propnums[0xF] = i;
            }
            else
            {
                RoomPropListBlockIndices[room] = i;
            }

            return i;
        }
    }
#ifdef DEBUG
    osSyncPrintf("newblockforroom: no free blocks!\n");
#endif
    return -1;
}




/*
* Address: 0x7F03DD9C
* PD: prop_register_room
* PD adds an upper bound check to make sure room is not above the max number of rooms
*/
void chrpropRegisterRoom(PropRecord *prop, s16 room)
{
   	s32 prevchunk = -1;
#ifdef DEBUG
    assert(room < g_MaxNumRooms); // roomnumber
#endif
    if (room < 0)
    {
        return;
    }
    else
    {
        // Find which chunk to start at
        s32 block = RoomPropListBlockIndices[room];
        s16 propnum = (prop - pos_data_entry);
#ifdef DEBUG
        assert(block<MAXBLOCKS);
#endif

        while (block >= 0)
        {
            if (chrpropInsertPropnum(propnum, block))
            {
                return;
            }

            prevchunk = block;
            block     = RoomPropListBlocks[block].propnums[0xF];
#ifdef DEBUG
            assert(block<MAXBLOCKS);
#endif
        }

        // Allocate a new chunk
        block = chrpropInitializeNewChunkForRoom(room, prevchunk);

        if (block >= 0)
        {
            chrpropInsertPropnum(propnum, block);
        }
    }
}




/*
* Address: 0x7F03DE94
* PD: prop_deregister_room
* PD adds an upper bound check to make sure room is not above the max number of rooms
*/
void chrpropDeregisterRoom(PropRecord* prop, s16 room) {
    bool removed = 0;
    s32 prev = -1;
#ifdef DEBUG
        assert(room < g_MaxNumRooms); // roomnumber
#endif

    if (room >= 0)
    {
        s16 block = RoomPropListBlockIndices[room];
        s16 propIndex = (prop - pos_data_entry);
#ifdef DEBUG
        assert(block<MAXBLOCKS);
#endif

        while (block >= 0)
        {
            bool populated = 0;
            s32 var_s0_2;

            // Check each prop entry in the chunk
            for (var_s0_2 = 0; var_s0_2 < 15; var_s0_2++)
            {
                if (propIndex == RoomPropListBlocks[block].propnums[var_s0_2])
                {
                    RoomPropListBlocks[block].propnums[var_s0_2] = -1; // Mark entry as empty
                    removed = 1;
                }
                else if (!populated && RoomPropListBlocks[block].propnums[var_s0_2] >= 0)
                {
                    populated = 1;
                }
            }

            if (!populated) // not matching
            {
                // This chunk is empty, so it can be marked as available
                RoomPropListBlocks[block].propnums[0] = -2;

                if (prev >= 0)
                {
                    RoomPropListBlocks[prev].propnums[0xF] = RoomPropListBlocks[block].propnums[0xF];
                }
                else
                {
                    RoomPropListBlockIndices[room] = RoomPropListBlocks[block].propnums[0xF];
                }
            }
            else
            {
                prev = block; // not matching
            }

            if (removed)
            {
                return;
            }

            block = RoomPropListBlocks[block].propnums[0xF];
#ifdef DEBUG
            assert(block<MAXBLOCKS);
#endif

        }
    }
}



void sub_GAME_7F03E134(PropRecord* p)
{
    if (p->type == PROP_TYPE_CHR)
    {
        chrPositionRelated7F020D94(p->chr);
    } else if ((p->type == PROP_TYPE_OBJ) || (p->type == PROP_TYPE_WEAPON))
    {
        setupUpdateObjectRoomPosition((ObjectRecord* ) p->obj);
    }
}



// Duplicate of the below function with a small extension.
void chrpropDeregisterRooms(PropRecord *prop)
{
    u8  room;
    u8 *roomIter;

    roomIter = prop->rooms;
    room     = roomIter[0];

    while (room != (u8)-1)
    {
        chrpropDeregisterRoom(prop, room);
        roomIter += 1;
        room = *roomIter;
    }
    if (!(prop->flags & PROPFLAG_00000010))
    {
        prop->rooms[0] = -1; //hide room
    }
}




void chrpropRegisterRooms(PropRecord *prop)
{
    u8  room;
    u8 *roomIter;

    roomIter = prop->rooms;
    room     = roomIter[0];

    while (room != (u8)-1)
    {
        chrpropRegisterRoom(prop, room);
        roomIter += 1;
        room = *roomIter;
    }
}


/*
* Address: 0x7F03E27C
*
* Recalculate the prop's room list with rooms it is found to be overlapping.
*/
void chrpropUpdateRoomList(PropRecord *prop, coord3d *bbmin, coord3d *bbmax, f32 radius)
{
    ObjectRecord *obj;
    s32 rooms[7]; // Room payload only, no terminator.
    StandTile *tile;
    s32 count;
    s32 i;
    u8 *src;

    count = 0;
    obj = NULL;

    if (prop->flags & PROPFLAG_00000008) {
        // Seed from the prop's existing room list.
        if (prop->type == PROP_TYPE_OBJ || prop->type == PROP_TYPE_WEAPON || prop->type == PROP_TYPE_DOOR) {
            obj = prop->obj;
        }

        if (obj != NULL
            && obj->runtime_bitflags & 0x80
            && obj->projectile->flags & PROJECTILEFLAG_00000008) {
            src = obj->projectile->unkCC;
        } else {
            src = prop->rooms;
        }

        for (i = 0; src[i] != 0xff; i++) {
            rooms[i] = src[i];
        }

        count = i;
    } else {
        // Seed from the stan tile locus around the prop's X/Z position.
        tile = prop->stan;
        count = 0;
        sub_GAME_7F0B21B0(&tile, prop->pos.x, prop->pos.z, radius, rooms, &count, 7);
    }

    // Update the room list with neighboring rooms reachable through portals and overlapped by the bounding box.
    sub_GAME_7F0BA2D4(bbmin, bbmax, rooms, &count, 7);

    for (i = 0; i < count; i++) {
        prop->rooms[i] = rooms[i];
    }

    // Commit the rebuilt room list to the prop, terminated by -1.
    prop->rooms[i] = -1;
}


/**
 * Given a list of rooms (terminated by -1), populate the propnums
 * list based on which props are in any of those rooms.
 * PD: roomGetProps
 */
void roomGetProps(s32 *rooms)
{
    s16 *writeptr = ptr_list_object_lookup_indices;
    s32 room;
    s32 i;
    s32 j;

    room = *rooms;

    // Iterate rooms
    while (room >= 0)
    {
        // Find the chunk to start at
        s32 chunkindex = RoomPropListBlockIndices[room];

        // Iterate the chunks
        while (chunkindex >= 0)
        {
            // Iterate the propnums within each chunk
            for (i = 0; i < 15; i++)
            {
                s32 propnum = RoomPropListBlocks[chunkindex].propnums[i];

                if (propnum >= 0)
                {
                    // Check if it's in the list already
                    s16 *ptr = ptr_list_object_lookup_indices;

                    while (ptr < writeptr)
                    {
                        if (*ptr == propnum) { break; }
                        ptr++;
                    }

                    if (ptr == writeptr)
                    {
                        // Prop is not in the list, so insert it
                        *writeptr = propnum;
                        writeptr++;
                    }
                }
            }

            chunkindex = RoomPropListBlocks[chunkindex].propnums[15];
        }

        rooms++;
        room = *rooms;
    }

    *writeptr = -1;
    writeptr++;
    num_obj_position_data_entries = writeptr - ptr_list_object_lookup_indices;
    #ifdef DEBUG
    assert(roomspropnum<ROOMLISTMAX-1); //num_obj_position_data_entries
    #endif
}


void propsDefragRoomProps(void)
{
	s32 i;
	s32 j;
	s32 k;

	// Iterate rooms
	for (i = 0; i < g_MaxNumRooms; i++)
    {
		s32 previndex = RoomPropListBlockIndices[i];

		if (previndex >= 0)
        {
			s32 nextindex = RoomPropListBlocks[previndex].propnums[0xF];

			// Iterate this room's chunks but skip the first
			while (nextindex >= 0)
            {
				// Iterate propnums within this chunk
				for (j = 0; j < 15; j++)
                {
					// If this propnum is unallocated
					if (RoomPropListBlocks[previndex].propnums[j] < 0)
                    {
						// Iterate forward through the chunk list and find a
						// propnum to move back to the prev chunk
						for (k = 0; k < 15; k++)
                        {
							if (RoomPropListBlocks[nextindex].propnums[k] >= 0)
                            {
								RoomPropListBlocks[previndex].propnums[j] = RoomPropListBlocks[nextindex].propnums[k];
								RoomPropListBlocks[nextindex].propnums[k] = -1;
								break;
							}
						}

						// Check if there are more propnums in the future chunk
						for (; k < 15; k++)
                        {
							if (RoomPropListBlocks[nextindex].propnums[k] >= 0)
                            {
								break;
							}
						}

						if (k == 15)
                        {
							// There's no more propnums, so this chunk can be removed
							RoomPropListBlocks[nextindex].propnums[0] = -2;
							RoomPropListBlocks[previndex].propnums[15] = RoomPropListBlocks[nextindex].propnums[15];

							nextindex = RoomPropListBlocks[previndex].propnums[15];

							if (nextindex < 0)
                            {
								break;
							}
						}
					}
				}

				if (nextindex >= 0)
                {
					previndex = nextindex;
					nextindex = RoomPropListBlocks[nextindex].propnums[15];
				}
			}
		}
	}
}


void removed_debug_roomblocks_feature(void)
{

}
//end of prop.c, now chrprop.c

/**
 * NTSC address 0x7F03E6A0.
*/
void sub_GAME_7F03E6A0(PropRecord *prop)
{
    struct LinkRecord *link;
    struct ObjectRecord *obj;

    obj = prop->obj;

    if (obj->runtime_bitflags & RUNTIMEBITFLAG_00000001)
    {
        for (link = g_LevelLoadPropSwitch; link != NULL; link = link->next)
        {
            if (prop == link->first)
            {
                if (link->second != NULL)
                {
                    doorActivateWrapper(link->second);
                }
            }
        }
    }
}



bool doorIsPadlockFree(DoorRecord* door)
{
    if (door->runtime_bitflags & RUNTIMEBITFLAG_PADLOCKEDDOOR)
    {
        LockDoorRecord *padlockeddoor = g_LevelLoadPropLockDoor;

        while (padlockeddoor)
        {
            if (door == padlockeddoor->door
                    && padlockeddoor->lock
                    && padlockeddoor->lock->prop
                    && objIsHealthy(padlockeddoor->lock)) {
                return FALSE;
            }

            padlockeddoor = padlockeddoor->next;
        }
    }

    return TRUE;
}


bool objCanPickupFromSafe(ObjectRecord *obj)
{
    if (obj->flags2 & PROPFLAG2_LINKEDTOSAFE)
    {
        SafeObjectRecord *link = g_LevelLoadPropSafeItem;

        while (link)
        {
            ObjectRecord *loopobj = link->item;

            if (obj == link->item && link->door && link->door->prop)
            {
                if (link->door->openPosition <= 0.5f)
                {
                    return FALSE;
                }
            }

            link = link->next;
        }
    }

    return TRUE;
}


void sub_GAME_7F03E830(ObjectRecord* arg0)
{
    PropRecord* prop = arg0->prop;
    stanGetPositionYValue(prop->stan, prop->pos.x, prop->pos.z);
}

f32 chrpropBBOXGetXmin(ModelRoData_BoundingBoxRecord *modelBoundingBox)
{
    return modelBoundingBox->Bounds.xmin;
}

f32 chrpropBBOXGetYmin(ModelRoData_BoundingBoxRecord *modelBoundingBox)
{
    return modelBoundingBox->Bounds.ymin;
}

f32 chrpropBBOXGetYmax(ModelRoData_BoundingBoxRecord *modelBoundingBox)
{
    return modelBoundingBox->Bounds.ymax;
}
f32 chrpropBBOXGetZmin(ModelRoData_BoundingBoxRecord *modelBoundingBox)
{
    return modelBoundingBox->Bounds.zmin;
}







/**
 * Address 0x7F03E87C.
*/
f32 chrpropSumMatrixPosX(struct ModelRoData_BoundingBoxRecord *bbox, Mtxf *arg1)
{
    f32 phi_f2;

    phi_f2 = 0.0f;

    if (arg1->m[0][0] >= 0.0f)
    {
        phi_f2 += (bbox->Bounds.xmin * arg1->m[0][0]);
    }
    else
    {
        phi_f2 += (bbox->Bounds.xmax * arg1->m[0][0]);
    }

    if (arg1->m[1][0] >= 0.0f)
    {
        phi_f2 += (bbox->Bounds.ymin * arg1->m[1][0]);
    }
    else
    {
        phi_f2 += (bbox->Bounds.ymax * arg1->m[1][0]);
    }

    if (arg1->m[2][0] >= 0.0f)
    {
        phi_f2 += (bbox->Bounds.zmin * arg1->m[2][0]);
    }
    else
    {
        phi_f2 += (bbox->Bounds.zmax * arg1->m[2][0]);
    }

    return phi_f2;
}




/**
 * Address 0x7F03E91C.
*/
f32 chrpropSumMatrixNegX(struct ModelRoData_BoundingBoxRecord *bbox, Mtxf *arg1)
{
    f32 phi_f2;

    phi_f2 = 0.0f;

    if (arg1->m[0][0] <= 0.0f)
    {
        phi_f2 += (bbox->Bounds.xmin * arg1->m[0][0]);
    }
    else
    {
        phi_f2 += (bbox->Bounds.xmax * arg1->m[0][0]);
    }

    if (arg1->m[1][0] <= 0.0f)
    {
        phi_f2 += (bbox->Bounds.ymin * arg1->m[1][0]);
    }
    else
    {
        phi_f2 += (bbox->Bounds.ymax * arg1->m[1][0]);
    }

    if (arg1->m[2][0] <= 0.0f)
    {
        phi_f2 += (bbox->Bounds.zmin * arg1->m[2][0]);
    }
    else
    {
        phi_f2 += (bbox->Bounds.zmax * arg1->m[2][0]);
    }

    return phi_f2;
}




/**
 * Address 0x7F03E9BC.
*/
f32 chrpropSumMatrixPosY(struct ModelRoData_BoundingBoxRecord *bbox, Mtxf *arg1)
{
    f32 phi_f2;

    phi_f2 = 0.0f;

    if (arg1->m[0][1] >= 0.0f)
    {
        phi_f2 += (bbox->Bounds.xmin * arg1->m[0][1]);
    }
    else
    {
        phi_f2 += (bbox->Bounds.xmax * arg1->m[0][1]);
    }

    if (arg1->m[1][1] >= 0.0f)
    {
        phi_f2 += (bbox->Bounds.ymin * arg1->m[1][1]);
    }
    else
    {
        phi_f2 += (bbox->Bounds.ymax * arg1->m[1][1]);
    }

    if (arg1->m[2][1] >= 0.0f)
    {
        phi_f2 += (bbox->Bounds.zmin * arg1->m[2][1]);
    }
    else
    {
        phi_f2 += (bbox->Bounds.zmax * arg1->m[2][1]);
    }

    return phi_f2;
}



/**
 * Address 0x7F03EA5C.
*/
f32 chrpropSumMatrixNegY(struct ModelRoData_BoundingBoxRecord *bbox, Mtxf *arg1)
{
    f32 phi_f2;

    phi_f2 = 0.0f;

    if (arg1->m[0][1] <= 0.0f)
    {
        phi_f2 += (bbox->Bounds.xmin * arg1->m[0][1]);
    }
    else
    {
        phi_f2 += (bbox->Bounds.xmax * arg1->m[0][1]);
    }

    if (arg1->m[1][1] <= 0.0f)
    {
        phi_f2 += (bbox->Bounds.ymin * arg1->m[1][1]);
    }
    else
    {
        phi_f2 += (bbox->Bounds.ymax * arg1->m[1][1]);
    }

    if (arg1->m[2][1] <= 0.0f)
    {
        phi_f2 += (bbox->Bounds.zmin * arg1->m[2][1]);
    }
    else
    {
        phi_f2 += (bbox->Bounds.zmax * arg1->m[2][1]);
    }

    return phi_f2;
}



/**
 * Address 0x7F03EAFC.
*/
f32 chrpropSumMatrixPosZ(struct ModelRoData_BoundingBoxRecord *bbox, Mtxf *arg1)
{
    f32 phi_f2;

    phi_f2 = 0.0f;

    if (arg1->m[0][2] >= 0.0f)
    {
        phi_f2 += (bbox->Bounds.xmin * arg1->m[0][2]);
    }
    else
    {
        phi_f2 += (bbox->Bounds.xmax * arg1->m[0][2]);
    }

    if (arg1->m[1][2] >= 0.0f)
    {
        phi_f2 += (bbox->Bounds.ymin * arg1->m[1][2]);
    }
    else
    {
        phi_f2 += (bbox->Bounds.ymax * arg1->m[1][2]);
    }

    if (arg1->m[2][2] >= 0.0f)
    {
        phi_f2 += (bbox->Bounds.zmin * arg1->m[2][2]);
    }
    else
    {
        phi_f2 += (bbox->Bounds.zmax * arg1->m[2][2]);
    }

    return phi_f2;
}



/**
 * Address 0x7F03EB9C.
*/
f32 chrpropSumMatrixNegZ(struct ModelRoData_BoundingBoxRecord *bbox, Mtxf *arg1)
{
    f32 phi_f2;

    phi_f2 = 0.0f;

    if (arg1->m[0][2] <= 0.0f)
    {
        phi_f2 += (bbox->Bounds.xmin * arg1->m[0][2]);
    }
    else
    {
        phi_f2 += (bbox->Bounds.xmax * arg1->m[0][2]);
    }

    if (arg1->m[1][2] <= 0.0f)
    {
        phi_f2 += (bbox->Bounds.ymin * arg1->m[1][2]);
    }
    else
    {
        phi_f2 += (bbox->Bounds.ymax * arg1->m[1][2]);
    }

    if (arg1->m[2][2] <= 0.0f)
    {
        phi_f2 += (bbox->Bounds.zmin * arg1->m[2][2]);
    }
    else
    {
        phi_f2 += (bbox->Bounds.zmax * arg1->m[2][2]);
    }

    return phi_f2;
}




/**
 * Unreferenced.
 * 0x7F03EC3C.
*/
void sub_GAME_7F03EC3C(struct ModelRoData_BoundingBoxRecord *bbox, Mtxf *arg1, struct coord3d *arg2)
{
    if (arg1->m[0][2] <= 0.0f)
    {
        arg2->f[0] = bbox->Bounds.xmin;
    }
    else
    {
        arg2->f[0] = bbox->Bounds.xmax;
    }

    if (arg1->m[1][2] <= 0.0f)
    {
        arg2->f[1] = bbox->Bounds.ymin;
    }
    else
    {
        arg2->f[1] = bbox->Bounds.ymax;
    }

    if (arg1->m[2][2] <= 0.0f)
    {
        arg2->f[2] = bbox->Bounds.zmin;
    }
    else
    {
        arg2->f[2] = bbox->Bounds.zmax;
    }
}


void sub_GAME_7F03ECC0(f32 x1, f32 x2, f32 y1, f32 y2, f32 z1, f32 z2, Mtxf *m, struct rect4f *poly, struct collision_data *collision)
{
    f64 pts[8][2];
    f64 pad[1];
    s32 i;
    s32 lim;
    s32 minxi = 0;
    s32 maxxi = 0;
    s32 minzi;
    s32 maxzi = 0;
    s32 rem[4];
    s32 cnt;
    f64 x1d = x1;
    f64 x2d = x2;
    f64 y1d = y1;
    f64 y2d = y2;
    f64 z1d = z1;
    f64 z2d = z2;
    f64 m00 = m->m[0][0];
    f64 m02 = m->m[0][2];
    f64 m10 = m->m[1][0];
    f64 m12 = m->m[1][2];
    f64 m20 = m->m[2][0];
    f64 m22 = m->m[2][2];
    minzi = 0;
    pts[0][0] = ((m00 * x1d) + (m10 * y1d)) + (m20 * z1d);
    pts[0][1] = ((m02 * x1d) + (m12 * y1d)) + (m22 * z1d);
    pts[1][0] = ((m00 * x1d) + (m10 * y1d)) + (m20 * z2d);
    pts[1][1] = ((m02 * x1d) + (m12 * y1d)) + (m22 * z2d);
    pts[2][0] = ((m00 * x1d) + (m10 * y2d)) + (m20 * z1d);
    pts[2][1] = ((m02 * x1d) + (m12 * y2d)) + (m22 * z1d);
    pts[3][0] = ((m00 * x1d) + (m10 * y2d)) + (m20 * z2d);
    pts[3][1] = ((m02 * x1d) + (m12 * y2d)) + (m22 * z2d);
    pts[4][0] = ((m00 * x2d) + (m10 * y1d)) + (m20 * z1d);
    pts[4][1] = ((m02 * x2d) + (m12 * y1d)) + (m22 * z1d);
    pts[5][0] = ((m00 * x2d) + (m10 * y1d)) + (m20 * z2d);
    pts[5][1] = ((m02 * x2d) + (m12 * y1d)) + (m22 * z2d);
    pts[6][0] = ((m00 * x2d) + (m10 * y2d)) + (m20 * z1d);
    pts[6][1] = ((m02 * x2d) + (m12 * y2d)) + (m22 * z1d);
    pts[7][0] = ((m00 * x2d) + (m10 * y2d)) + (m20 * z2d);
    pts[7][1] = ((m02 * x2d) + (m12 * y2d)) + (m22 * z2d);

    for (i = 1; i < 8; i++)
    {
        if ((pts[i][0] < pts[minxi][0]) || ((pts[i][0] == pts[minxi][0]) && (pts[i][1] < pts[minxi][1])))
        {
            minxi = i;
        }
    }

    for (i = 1; i < 8; i++)
    {
        if ((pts[maxzi][1] < pts[i][1]) || ((pts[i][1] == pts[maxzi][1]) && (pts[i][0] < pts[maxzi][0])))
        {
            maxzi = i;
        }
    }

    for (i = 1; i < 8; i++)
    {
        if ((pts[maxxi][0] < pts[i][0]) || ((pts[i][0] == pts[maxxi][0]) && (pts[maxxi][1] < pts[i][1])))
        {
            maxxi = i;
        }
    }

    for (i = 1; i < 8; i++)
    {
        if ((pts[i][1] < pts[minzi][1]) || ((pts[i][1] == pts[minzi][1]) && (pts[minzi][0] < pts[i][0])))
        {
            minzi = i;
        }
    }

    lim = 8;
    cnt = 0;
    i = 0;

filterloop:
    if ((((i != minxi) && (i != maxxi)) && (i != maxzi)) && (i != minzi))
    {
        rem[cnt] = i;
        cnt++;
    }

    i++;

    if (i < lim)
    {
        goto filterloop;
    }

    cnt = 0;
    poly->points[cnt].x = pts[minxi][0];
    poly->points[cnt].y = pts[minxi][1];
    cnt++;

    for (i = 0; i < 4; i++)
    {
        s32 index = rem[i];

        if (((pts[index][0] - pts[minzi][0]) * (pts[minxi][1] - pts[minzi][1])) < ((pts[minxi][0] - pts[minzi][0]) * (pts[index][1] - pts[minzi][1])))
        {
            poly->points[cnt].x = pts[index][0];
            poly->points[cnt].y = pts[index][1];
            cnt++;
            break;
        }
    }

    poly->points[cnt].x = pts[minzi][0];
    poly->points[cnt].y = pts[minzi][1];
    cnt++;

    for (i = 0; i < 4; i++)
    {
        s32 index = rem[i];

        if (((pts[index][0] - pts[maxxi][0]) * (pts[minzi][1] - pts[maxxi][1])) < ((pts[minzi][0] - pts[maxxi][0]) * (pts[index][1] - pts[maxxi][1])))
        {
            poly->points[cnt].x = pts[index][0];
            poly->points[cnt].y = pts[index][1];
            cnt++;
            break;
        }
    }

    poly->points[cnt].x = pts[maxxi][0];
    poly->points[cnt].y = pts[maxxi][1];
    cnt++;

    for (i = 0; i < 4; i++)
    {
        s32 index = rem[i];

        if (((pts[index][0] - pts[maxzi][0]) * (pts[maxxi][1] - pts[maxzi][1])) < ((pts[maxxi][0] - pts[maxzi][0]) * (pts[index][1] - pts[maxzi][1])))
        {
            poly->points[cnt].x = pts[index][0];
            poly->points[cnt].y = pts[index][1];
            cnt++;
            break;
        }
    }

    poly->points[cnt].x = pts[maxzi][0];
    poly->points[cnt].y = pts[maxzi][1];
    cnt++;

    for (i = 0; i < 4; i++)
    {
        s32 index = rem[i];

        if (((pts[index][0] - pts[minxi][0]) * (pts[maxzi][1] - pts[minxi][1])) < ((pts[maxzi][0] - pts[minxi][0]) * (pts[index][1] - pts[minxi][1])))
        {
            poly->points[cnt].x = pts[index][0];
            poly->points[cnt].y = pts[index][1];
            cnt++;
            break;
        }
    }

    collision->edges = cnt;

    for (i = 0; i < cnt; i++)
    {
        poly->points[i].x += m->m[3][0];
        poly->points[i].y += m->m[3][2];
    }
}


void sub_GAME_7F03F540(struct ModelRoData_BoundingBoxRecord *bbox, Mtxf* arg1, struct rect4f* arg2, struct collision_data* arg3)
{
    sub_GAME_7F03ECC0(bbox->Bounds.xmin, bbox->Bounds.xmax, bbox->Bounds.ymin, bbox->Bounds.ymax, bbox->Bounds.zmin, bbox->Bounds.zmax, arg1, arg2, arg3);
}


/**
 * Address: 7F03F598
 *
 * Tests whether a world-space point is inside a bound pad's local bbox plus
 * a padding on all axes defined by the radius parameter.
 */
bool chrpropTestPointInPaddedBoundPad(coord3d *pos, f32 radius, BoundPadRecord *pad)
{
    f32 dx;
    f32 dy;
    f32 dz;
    f32 side[3];
    f32 d;

    dx = pos->x - pad->pos.x;
    dy = pos->y - pad->pos.y;
    dz = pos->z - pad->pos.z;

    side[0] = (pad->up.y * pad->look.z) - (pad->look.y * pad->up.z);
    side[1] = (pad->up.z * pad->look.x) - (pad->look.z * pad->up.x);
    side[2] = (pad->up.x * pad->look.y) - (pad->look.x * pad->up.y);

    d = (pad->look.z * dz) + ((dx * pad->look.x) + (dy * pad->look.y));
    if ((pad->bbox.zmax + radius < d) || (d < pad->bbox.zmin - radius))
    {
        return FALSE;
    }

    d = (pad->up.z * dz) + ((dx * pad->up.x) + (dy * pad->up.y));
    if ((pad->bbox.ymax + radius < d) || (d < pad->bbox.ymin - radius))
    {
        return FALSE;
    }

    d = (dx * side[0]) + (dy * side[1]) + (side[2] * dz);
    if ((pad->bbox.xmax + radius < d) || (d < pad->bbox.xmin - radius))
    {
        return FALSE;
    }

    return TRUE;
}


/*
* Address: 7F03F748
*/
void modelGetAxisExtents(Model* model, f32* max, f32* min, s32 axis)
{
    ModelNode *node = model->obj->RootNode;
    bool first = TRUE;

    while (node)
    {
        u32 type = node->Opcode & 0xFF;

        if (type == MODELNODE_OPCODE_BBOX)
        {
            struct ModelRoData_BoundingBoxRecord *bbox = &node->Data->BoundingBox;
            Mtxf *mtx = modelFindNodeMtx(model, node, 0);
            f32 dist1;
            f32 dist2;

            if (axis == 0)
            {
                dist1 = chrpropSumMatrixNegX(bbox, mtx) + mtx->m[3][0];
                dist2 = chrpropSumMatrixPosX(bbox, mtx) + mtx->m[3][0];
            }
            else if (axis == 1)
            {
                dist1 = chrpropSumMatrixNegY(bbox, mtx) + mtx->m[3][1];
                dist2 = chrpropSumMatrixPosY(bbox, mtx) + mtx->m[3][1];
            }
            else
            {
                dist1 = chrpropSumMatrixNegZ(bbox, mtx) + mtx->m[3][2];
                dist2 = chrpropSumMatrixPosZ(bbox, mtx) + mtx->m[3][2];
            }

            if (first || dist1 > *max)
            {
                *max = dist1;
            }

            if (first || dist2 < *min)
            {
                *min = dist2;
            }

            first = FALSE;
        }
        else
        {
            // empty
        }

        if (node->Child)
        {
            node = node->Child;
        }
        else
        {
            while (node)
            {
                if (node->Next)
                {
                    node = node->Next;
                    break;
                }

                node = node->Parent;
            }
        }
    }
}


void modelGetXYExtents(Model *model, f32 *arg1, f32 *arg2, f32 *arg3, f32 *arg4)
{
    modelGetAxisExtents(model, arg1, arg2, 0);
    modelGetAxisExtents(model, arg3, arg4, 1);
}


/**
 * NTSC address 0x7F03F948.
 * Project rectangle corners to screen
*/
void projectRectCornersTo2D(struct coord3d *center, struct coord2d *arg1, struct coord2d *arg2, struct coord2d *arg3, struct coord2d *arg4)
{
    struct coord3d sp24;
    struct coord2d tout;

    sp24.f[0] = arg1->f[0];
    sp24.f[1] = center->f[1];
    sp24.f[2] = center->f[2];
    transform3Dto2DCoords(&sp24, &tout);
    arg3->f[0] = tout.f[0];

    sp24.f[0] = arg1->f[1];
    sp24.f[1] = center->f[1];
    sp24.f[2] = center->f[2];
    transform3Dto2DCoords(&sp24, &tout);
    arg4->f[0] = tout.f[0];

    sp24.f[0] = center->f[0];
    sp24.f[1] = arg2->f[1];
    sp24.f[2] = center->f[2];
    transform3Dto2DCoords(&sp24, &tout);
    arg3->f[1] = tout.f[1];

    sp24.f[0] = center->f[0];
    sp24.f[1] = arg2->f[0];
    sp24.f[2] = center->f[2];
    transform3Dto2DCoords(&sp24, &tout);
    arg4->f[1] = tout.f[1];
}




/*
* Address: 0x7F03FA44
*/
ObjectRecord *scan_position_data_table_for_normal_object_at_preset(s32 PadId) {
    PropRecord *prop;
    s16 tempPadId = PadId;

    prop = get_ptr_obj_pos_list_current_entry();
    while (prop != NULL)
    {
        if (prop->type == PROP_TYPE_OBJ)
        {
            if (tempPadId == prop->obj->pad)
            {
                return prop->obj;
            }
        }

        prop = prop->prev;
    }

    return NULL;
}




ObjectRecord * sub_GAME_7F03FAB0(struct coord3d *pos, s32 RoomID)
{
    s32 unused;
    rect4f * polygon;
    s32 edges;
    PropRecord * prop;

    prop = get_ptr_obj_pos_list_current_entry();
    while (prop != NULL)
    {
        if ((prop->type == PROP_TYPE_OBJ) && (RoomID == prop->stan->room))
        {
            chraiGetCollisionBoundsWithoutY(prop, &polygon, &edges);
            if (chrpropTestPointInPolygon(pos, polygon, edges) != 0)
            {
                return (ObjectRecord *) prop->chr;
            }
        }
        prop = prop->prev;
    }

    return NULL;
}

