#include <ultra64.h>
#include <PR/os.h>
#include <PR/gbi.h>
#include <gbi_extension.h>
#include <bondconstants.h>
#include <deb.h>
#include <fr.h>
#include <memp.h>
#include "bg.h"
#include "bondview.h"
#include "chr.h"
#include "debug_camera.h"
#include "debugmenu_handler.h"
#include "decompress.h"
#include "bgfog.h"
#include "lv.h"
#include "math_ceil.h"
#include "matrixmath.h"
#include "player.h"
#include "stan.h"
#include "explosion.h"
#include "bgroomtrans.h"


#define BG_STACK_SIZE 20
#define STAGES_MAX 38

typedef struct GlobalVisCommand {
    u8 type;
    u8 length;
    u8 padding[2];
    s32 arg;
} GlobalVisCommand;

typedef struct Unk80081600 {
    bbox2d unk0;
    s32 unk10;
    s32 unk14;
} Unk80081600;

enum GlobalVisOpcode {
    VISOP_END = 0x00,
    VISOP_PUSH = 0x01,
    VISOP_POP = 0x02,
    VISOP_AND = 0x03,
    VISOP_OR = 0x04,
    VISOP_NOT = 0x05,
    VISOP_XOR = 0x06,
    VISOP_PUSH_IF_ROOM_IN_RANGE = 0x14,
    VISOP_FORCE_VISIBLE = 0x1e,
    VISOP_MATCH_PORTAL_VIS = 0x1f,
    VISOP_ADD_VISIBLE_ROOM = 0x20,
    VISOP_REMOVE_VIS = 0x21,
    VISOP_VISIBLE_IF_SEEN_THROUGH_PORTAL = 0x22,
    VISOP_NOT_VISIBLE_IF_SEEN_THROUGH_PORTAL = 0x23,
    VISOP_DISABLE_ROOM = 0x24,
    VISOP_DISABLE_ROOM_RANGE = 0x25,
    VISOP_PRELOAD_ROOM = 0x26,
    VISOP_PRELOAD_ROOM_RANGE = 0x27,
    VISOP_IF_STATEMENT = 0x50,
    VISOP_DONT_EXEC_COMMANDS_EVEN_ON_RETURN = 0x51,
    VISOP_ENDIF_CONTINUE_EXEC = 0x52,
    VISOP_IF_STATEMENT_PULL_FROM_STACK = 0x5a,
    VISOP_TOGGLE_EXEC_VS_READONLY = 0x5b,
    VISOP_ENDIF = 0x5c
};

extern struct unk_portalstruct table_for_portals[PORTMAX];
extern s32 ptr_bgdata_offsets;
extern s32 dword_CODE_bss_8007FF88;
extern s32 *dword_CODE_bss_8007FF90;
extern f32 *dword_CODE_bss_8007FF94;
extern s_bound_info dword_CODE_bss_8007FFA0[];
#ifndef VERSION_EU
extern s32 dword_CODE_bss_8007FF98;
#endif

#ifdef VERSION_EU
#define BG_PORTAL_QUEUE_LEN 250
#else
#define BG_PORTAL_QUEUE_LEN 500
#endif

// bss
//CODE.bss:8007BF90
s32 ptr_bg_data;

//CODE.bss:8007BF94
s32 gptr_stan;

/**
 * address 8007BF98
 * EU .bss 80079ee8
*/
s32 dword_CODE_bss_8007BF98;

#ifdef VERSION_EU
s32 eu_bss_80079EEC;
#endif

/**
 * address 8007BFA0
 * EU .bss 80079ef0
*/
#ifdef VERSION_EU
char list_visible_rooms_in_cur_global_vis_packet[0x8c];
#else
char list_visible_rooms_in_cur_global_vis_packet[0x98];
#endif

/**
 * address 8007C038
 * EU .bss 80079f7c
*/
s32 num_visible_rooms_in_cur_global_vis_packet;

/* D:800413F0 Level gCurrentLevel = {0, 1.0, 1.0, 1.0, 1}; cant check this
   anymore however will concede seperate vars since below gets match? */
s32 *ptr_bg_c_debug_debug_notice_list = 0;
//D:800413F4
f32 room_data_float1 = 1.0;
//D:800413F8
f32 room_data_float2 = 1.0;
//D:800413FC Private member - use bgGetLevelVisibilityScale outside this file
f32 mCurrentLevelVisibilityScale = 1.0;
//D:80041400
s32 levelentry_index = 1;

/**
 * Something related to player screen.
 * Maybe x, y, width, height.
 * DefaultScreenXMin = 1; //always 1 (related to getvideosettings) Xmin
 * DefaultScreenYMin = 1; //always 1 (related to getvideosettings)Ymin
 * SubtractFromWidth = -1; //always -1 (related to getvideosettings)
 * SubtractFromHeight = -1; //always -1 (related to getvideosettings)
 * Address 0x80041404.
 */
s32 bgViewRelated[] = { 1, 1, -1, -1 };


/**
 * Array of info about all the rooms on the level
 * Some info is different according to which player is being rendered
 * This array is also why there's a 150 is room limit
 * Canonical name: roominf
 * Address 0x80041414
*/
s_room_info g_BgRoomInfo[MAXROOMCOUNT] = {0};
//D:800442F4 canonically roomnumber
s32 g_MaxNumRooms = MAXROOMCOUNT;

/**
 * Limits the number of rooms that can be loaded per frame.
 * Address: 0x800442F8
 */
s32 g_RoomLoadBudget = 0;

//D:800442FC
u8 D_800442FC[PORTMAX] = {0};
//D:800443C4
u8 D_800443C4[PORTMAX] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};



//D:8004448C
struct levelentry levelinfotable[] = {
/*  levelID;            bg_seg_filename;        bg_stan_filename;      levelscale;  visibility; unknownfloat;*/
    {LEVELID_BUNKER1,  "bg/bg_sev_all_p.seg",  "Tbg_sev_all_p_stanZ",  0.53931433,  1.0,        23.148148},
    {LEVELID_SILO,     "bg/bg_silo_all_p.seg", "Tbg_silo_all_p_stanZ", 0.47256002,  1.0,        29.069},
    {LEVELID_STATUE,   "bg/bg_stat_all_p.seg", "Tbg_stat_all_p_stanZ", 0.107202865, 1.0,        0.0801},
    {LEVELID_CONTROL,  "bg/bg_arec_all_p.seg", "Tbg_arec_all_p_stanZ", 0.49886572,  1.0,        80.645164},
    {LEVELID_ARCHIVES, "bg/bg_arch_all_p.seg", "Tbg_arch_all_p_stanZ", 0.50678575,  1.0,        54.347824},
    {LEVELID_TRAIN,    "bg/bg_tra_all_p.seg",  "Tbg_tra_all_p_stanZ",  0.15019713,  1.0,        19.53125},
    {LEVELID_FRIGATE,  "bg/bg_dest_all_p.seg", "Tbg_dest_all_p_stanZ", 0.44757429,  1.0,        36.764706},
    {LEVELID_BUNKER2,  "bg/bg_sevb_all_p.seg", "Tbg_sevb_all_p_stanZ", 0.53931433,  1.0,        23.148148},
    {LEVELID_AZTEC,    "bg/bg_azt_all_p.seg",  "Tbg_azt_all_p_stanZ",  0.35300568,  1.0,        52.083332},
    {LEVELID_STREETS,  "bg/bg_pete_all_p.seg", "Tbg_pete_all_p_stanZ", 0.34187999,  1.0,        42.372883},
    {LEVELID_DEPOT,    "bg/bg_depo_all_p.seg", "Tbg_depo_all_p_stanZ", 0.21847887,  1.0,        17.605633},
    {LEVELID_COMPLEX,  "bg/bg_ref_all_p.seg",  "Tbg_ref_all_p_stanZ",  0.94285715,  1.0,        37.878788},
    {LEVELID_EGYPT,    "bg/bg_cryp_all_p.seg", "Tbg_cryp_all_p_stanZ", 0.25608,     1.0,        23.584906},
    {LEVELID_DAM,      "bg/bg_dam_all_p.seg",  "Tbg_dam_all_p_stanZ",  0.23363999,  0.2,        100.0},
    {LEVELID_FACILITY, "bg/bg_ark_all_p.seg",  "Tbg_ark_all_p_stanZ",  1.20648,     1.0,        64.102562},
    {LEVELID_RUNWAY,   "bg/bg_run_all_p.seg",  "Tbg_run_all_p_stanZ",  0.089571431, 1.0,        4.5537338},
    {LEVELID_SURFACE,  "bg/bg_sevx_all_p.seg", "Tbg_sevx_all_p_stanZ", 0.45445713,  0.2,        22.603975},
    {LEVELID_JUNGLE,   "bg/bg_jun_all_p.seg",  "Tbg_jun_all_p_stanZ",  0.094662853, 1.0,        6.6844921},
    {LEVELID_TEMPLE,   "bg/bg_dish_all_p.seg", "Tbg_dish_all_p_stanZ", 0.47142857,  1.0,        147.05882},
    {LEVELID_CAVERNS,  "bg/bg_cave_all_p.seg", "Tbg_cave_all_p_stanZ", 0.26824287,  1.0,        13.44086},
    {LEVELID_CITADEL,  "bg/bg_cat_all_p.seg",  "Tbg_cat_all_p_stanZ",  0.76852286,  1.0,        38.461536},
    {LEVELID_CRADLE,   "bg/bg_crad_all_p.seg", "Tbg_crad_all_p_stanZ", 0.23571429,  1.0,        43.103451},
    {LEVELID_SHO,      "bg/bg_sho_all_p.seg",  "Tbg_sho_all_p_stanZ",  0.528,       1.0,        21.18644},
    {LEVELID_SURFACE2, "bg/bg_sevx_all_p.seg", "Tbg_sevx_all_p_stanZ", 0.45445713,  0.2,        22.603975},
    {LEVELID_ELD,      "bg/bg_eld_all_p.seg",  "Tbg_eld_all_p_stanZ",  0.94285715,  1.0,        10.123456},
    {LEVELID_BASEMENT, "bg/bg_ame_all_p.seg",  "Tbg_ame_all_p_stanZ",  0.65999997,  1.0,        37.878788},
    {LEVELID_STACK,    "bg/bg_ame_all_p.seg",  "Tbg_ame_all_p_stanZ",  0.65999997,  1.0,        37.878788},
    {LEVELID_LUE,      "bg/bg_lue_all_p.seg",  "Tbg_lue_all_p_stanZ",  0.94285715,  1.0,        10.123456},
    {LEVELID_LIBRARY,  "bg/bg_ame_all_p.seg",  "Tbg_ame_all_p_stanZ",  0.65999997,  1.0,        37.878788},
    {LEVELID_RIT,      "bg/bg_rit_all_p.seg",  "Tbg_rit_all_p_stanZ",  0.94285715,  1.0,        10.123456},
    {LEVELID_CAVES,    "bg/bg_oat_all_p.seg",  "Tbg_oat_all_p_stanZ",  0.14142857,  1.0,        10.123456},
    {LEVELID_EAR,      "bg/bg_ear_all_p.seg",  "Tbg_ear_all_p_stanZ",  0.94285715,  1.0,        10.123456},
    {LEVELID_LEE,      "bg/bg_lee_all_p.seg",  "Tbg_lee_all_p_stanZ",  0.94285715,  1.0,        10.123456},
    {LEVELID_LIP,      "bg/bg_lip_all_p.seg",  "Tbg_lip_all_p_stanZ",  0.94285715,  1.0,        10.123456},
    {LEVELID_CUBA,     "bg/bg_len_all_p.seg",  "Tbg_len_all_p_stanZ",  0.094662853, 1.0,        6.6844921},
    {LEVELID_WAX,      "bg/bg_wax_all_p.seg",  "Tbg_wax_all_p_stanZ",  0.94285715,  1.0,        10.123456},
    {LEVELID_PAM,      "bg/bg_pam_all_p.seg",  "Tbg_pam_all_p_stanZ",  0.94285715,  1.0,        10.123456},
    {LEVELID_MAX,      "bg/bgx.seg",           "TbgxZ",                0.94285715,  1.0,        1.0}
};

//D:8004481C
u32 D_8004481C[] = {0x1000100, 0};

//D:80044824
s_specialportal specialportalarray[] = {
    {0x03,
        {0x2C,0x2E,0x32, 0x37,0x3E,0x3F,0x4E, 0x56,0x59,0x5D,0x72, 0x76,0x79,0x7A,0xFF}},
    {0x11,
        {0x00,0x3A,0xFF}}
};

/**
 * Bond's current room.
 * Address 0x80044838
*/
s32 g_BgCurrentRoom = 1;

/**
 * Total number of rooms drawn for the current frame.
 * Address 0x8004483C
*/
s32 g_BgNumberOfRoomsDrawn = 0;

#if defined(VERSION_EU)
s32 eu_cdata_0x1f0d0 = 0;
s32 eu_cdata_0x1f0d4 = 0;
#endif

//D:80044840
Lights1 GlobalLight = gdSPDefLights1(
    150,150,150,        /* ambient color grey */ //D:80044840
    255,255,255,
    77,77,46    /* white light from the upper west-south-west (42 up, 244') */ //D:80044848
);



//D:80044858
s32 D_80044858 = 0;
//D:8004485C
s32 D_8004485C = 1;

// forward declarations

void unload_rooms(void);
Gfx *sub_GAME_7F0B8D78(Gfx *arg0);
Gfx *sub_GAME_7F0B3C8C(Gfx *arg0);
s32 bgCheckIfRoomModelNeedsLoad(s32 roomID);
void bgLoadRoomModelData(s32 room);
void bgBuildRoomVtxBounds(s32 roomID);
Gfx *bgRenderRoomPrimary(Gfx *gdl, s32 room_index);
Gfx *bgRenderRoomSecondary(Gfx *gdl, s32 room_index);

Gfx *bgScissorCurrentPlayerView(Gfx *arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

bool bgIsRoomOnScreen(s32 roomID, struct rectbbox *screenbox);
s32 sub_GAME_7F0B39BC(s32 curroom, s32 unk1, bbox2d *screensize, s32 next);
void bgUpdateCurrentPlayerScreenMinMax(void);
void *sub_GAME_7F0B8A24(s32 *pc);
void bgDetermineVisibleRooms(void);
s32 sub_GAME_7F0B5864(s32 portalnum, bbox2d *screenbox);
f32 sub_GAME_7F0B9990(s32 portalnum);
void sub_GAME_7F0B95D8(s32 roomID);
#if defined(VERSION_EU)
void sub_GAME_7F0B7F84(s32 roomnum, s32 portalnum, s32 depth, bbox2d *parentbox);
#endif
void sub_GAME_7F0B4810(f32 arg0);
s32 sub_GAME_7F0B5528(s32 portalnum, f32 scale, coord3d *points);
void bgOrderPortal(s32 portalnum);

// end forward declarations


void bgInit(void) 
{
    debTryAdd(&ptr_bg_c_debug_debug_notice_list, "bg_c_debug");
}


void sub_GAME_7F0B37EC(void) {
    u8 *ptr;
    u8 *end;
    u8 portal;
    u8 cur;
    u32 masked;

    ptr = (u8 *)specialportalarray;
    end = (u8 *)&g_BgCurrentRoom;

    do {
        if (levelentry_index == *ptr++) {
            do {
                portal = ptr[0];
                while (ptr[1] >= portal) {
                    ((u8 *)g_BgPortals)[(portal << 3) + 6] |= 2;
                    portal++;
                }

                ptr += 2;
            } while (ptr[0] != 0xff);
        } else {
            do {
                ptr += 2;
            } while (ptr[0] != 0xff);
        }

        ptr++;
    } while ((u32)ptr < (u32)end);
}


/*
* Unused function
* Address: 7F0B38B4
*/
void sub_GAME_7F0B38B4(s32 arg0, u8 *arg1) {

    u8 *s0;
    u8 *entry_start;
    s32 v0;
    s32 v1;

    s0 = arg1;
    v0 = *arg1;
    v1 = arg0 & 0xff;

    do {
        entry_start = s0;

        do {
            if (v1 == v0) goto found;
            v0 = *(s0 + 1);
            s0++;
        } while (v0 != 0);

        do { s0++; } while (*s0 != 0);
        s0++;
        goto next_entry;

found:
        s0 = entry_start;
        for (v0 = *entry_start; ; ) {
            if (bgIsRoomOnScreen(v0 ^ 0, &g_CurrentPlayer->screensize)) {
                sub_GAME_7F0B39BC(*s0, 0, &g_CurrentPlayer->screensize, 1);
            }
            v0 = *(s0 + 1);
            s0++;
            if (v0 == 0) break;
        }
        s0++;
        v0 = *s0;
        do {
            if (bgIsRoomOnScreen(v0 ^ 0, &g_CurrentPlayer->screensize)) {
                sub_GAME_7F0B39BC(*s0, 0, &g_CurrentPlayer->screensize, 1);
            }
            v0 = *(s0 + 1);
            s0++;
        } while (v0 != 0);
        return;

next_entry:
        v0 = *s0;
    } while (v0 != 0);

}


/**
 * Address 0x7F0B39BC.
 *
*/
s32 sub_GAME_7F0B39BC(int curroom,int unk1, bbox2d * screensize, s32 next)
{
    int i;
    int temp;

    g_BgRoomInfo[curroom].room_rendered = '\x01';

    if (g_BgRoomInfo[curroom].room_loaded_mask != '\0') {
        return 0;
    }

    // Need g_BgNumberOfRoomsDrawn in a3
    for (i = 0; i < g_BgNumberOfRoomsDrawn; i++)
    {
        if (curroom == dword_CODE_bss_8007FFA0[i].roomid) {
            if (dword_CODE_bss_8007FFA0[i].unk1 < unk1) {
                dword_CODE_bss_8007FFA0[i].unk1 = unk1;
            }
            bgRectOutersect(screensize,&dword_CODE_bss_8007FFA0[i].bbox);
            temp = dword_CODE_bss_8007FFA0[i].next;
            dword_CODE_bss_8007FFA0[i].bbox.min.x = screensize->min.x;
            dword_CODE_bss_8007FFA0[i].bbox.min.y = screensize->min.y;
            dword_CODE_bss_8007FFA0[i].bbox.max.x = screensize->max.x;
            dword_CODE_bss_8007FFA0[i].bbox.max.y = screensize->max.y;
            dword_CODE_bss_8007FFA0[i].next = temp | next;

            return temp;
        }
    }

#if defined(VERSION_EU)
    i = g_BgNumberOfRoomsDrawn;
    if (i >= 0x78) {
        i = 0x77;
    }
    dword_CODE_bss_8007FFA0[i].roomid = curroom;
    dword_CODE_bss_8007FFA0[i].unk1 = unk1;
    dword_CODE_bss_8007FFA0[i].bbox.min.x = screensize->min.x;
    dword_CODE_bss_8007FFA0[i].bbox.min.y = screensize->min.y;
    dword_CODE_bss_8007FFA0[i].bbox.max.x = screensize->max.x;
    dword_CODE_bss_8007FFA0[i].bbox.max.y = screensize->max.y;
    dword_CODE_bss_8007FFA0[i].next = next;
    eu_cdata_0x1f0d0++;
    if (eu_cdata_0x1f0d0 < 0x78) {
        g_BgNumberOfRoomsDrawn = eu_cdata_0x1f0d0;
    }

    return 0;
#else
    i = g_BgNumberOfRoomsDrawn;
    dword_CODE_bss_8007FFA0[i].roomid = curroom;
    dword_CODE_bss_8007FFA0[i].unk1 = unk1;
    dword_CODE_bss_8007FFA0[i].bbox.min.x = screensize->min.x;
    dword_CODE_bss_8007FFA0[i].bbox.min.y = screensize->min.y;
    dword_CODE_bss_8007FFA0[i].bbox.max.x = screensize->max.x;
    dword_CODE_bss_8007FFA0[i].bbox.max.y = screensize->max.y;
    dword_CODE_bss_8007FFA0[i].next = next;
    g_BgNumberOfRoomsDrawn = i + 1;

    if (g_BgNumberOfRoomsDrawn) {}

    return 0;
#endif
}


/*
* Unused function
* Address: ?
*/
void bgZeroPortalsToRoom(s32 roomnum)
{
  g_BgRoomInfo[roomnum].portal_visit_count = 0;
}


/*
* Unused function
* Address: 7F0B3B20
*/
s32 bgFindFirstPortalVisitedRoom(void)
{
    s32 i;

    for (i=0;i<MAXROOMCOUNT;i++)
    {
        if (g_BgRoomInfo[i].portal_visit_count) {
            return i;
        };
    }
    return -1;
}


/**
 * Address: 7F0B3BC4
 */
void bgResetPortalVisitCounts(void)
{
  s32 i;

  g_BgNumberOfRoomsDrawn = 0;
#ifdef VERSION_EU
  eu_cdata_0x1f0d0 = 0;
#endif
  i = 0;
  while (i != MAXROOMCOUNT)
  {
    g_BgRoomInfo[i].portal_visit_count = 0;
    i++;
  }
}


/**
 * Searches dword_CODE_bss_8007FFA0 for matching id.
 * If found, sets result to 2d bbox and returns 1.
 * Otherwise result is set to empty 2d bbox and returns 0.
 *
 * @param room_id: room id to search for.
 * @param result: Out parameter. Contains result 2d bbox.
 *
 * Address 0x7F0B3C0C.
*/
s32 bgGet2dBboxByRoomId(s32 room_id, struct bbox2d *result)
{
    s32 i;
    for (i=0; i<g_BgNumberOfRoomsDrawn; i++)
    {
        if (room_id == dword_CODE_bss_8007FFA0[i].roomid)
        {
            result->f[0][0] = dword_CODE_bss_8007FFA0[i].bbox.f[0][0];
            result->f[0][1] = dword_CODE_bss_8007FFA0[i].bbox.f[0][1];
            result->f[1][0] = dword_CODE_bss_8007FFA0[i].bbox.f[1][0];
            result->f[1][1] = dword_CODE_bss_8007FFA0[i].bbox.f[1][1];

            return 1;
        }
    }

    // It's pointless to set those because when this function returns false, result is unused
    result->f[0][0] = 0.0f;
    result->f[0][1] = 0.0f;
    result->f[1][0] = 0.0f;
    result->f[1][1] = 0.0f;

    return 0;
}


/**
 * Address: 7F0B3C8C
 */
Gfx *sub_GAME_7F0B3C8C(Gfx *gdl)
{
#ifdef VERSION_EU
    s16 i;
#else
    s32 i;
#endif
    s32 j;
#ifdef VERSION_EU
    s16 b_max;
    s16 b_min;
#else
    s32 b_max;
    s32 b_min;
#endif
    s32 notdone;
#ifdef VERSION_EU
    b_max = 0;
    b_min = 32767;
#else
    b_min = 99999999;
    b_max = 0;
#endif
 
    for (j = 0; j < g_BgNumberOfRoomsDrawn; j++)
    {
        if (b_max < dword_CODE_bss_8007FFA0[j].unk1)
        {
            b_max = dword_CODE_bss_8007FFA0[j].unk1;
        }
 
        if (dword_CODE_bss_8007FFA0[j].unk1 < b_min)
        {
            b_min=dword_CODE_bss_8007FFA0[j].unk1;
        }
    }
 
    for (i = b_min; i <= b_max; i++)
    {
    #ifdef DEBUG
        notdone = g_BgNumberOfRoomsDrawn;
    #endif
        for (j = 0; j < g_BgNumberOfRoomsDrawn; j++)
        {
            if (i == dword_CODE_bss_8007FFA0[j].unk1)
            {
                gSPMatrix(gdl++, osVirtualToPhysical((void*)currentPlayerGetProjectionMatrix()), (G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION));
                gdl = fogRenderClearFogMode(gdl);
 
                if (get_debug_do_draw_obj())
                {
                    if (sub_GAME_7F0BD8F0())
                    {
                        gdl = chrpropsRenderPass(gdl, dword_CODE_bss_8007FFA0[j].roomid, 0);
                    }
                }
 
                gSPMatrix(gdl++, osVirtualToPhysical((void*)get_BONDdata_field_10E0()), (G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION));
                gdl = fogSetRenderFogColor(
                    bgScissorCurrentPlayerViewF(
                        gdl++,
                        dword_CODE_bss_8007FFA0[j].bbox.min.x,
                        dword_CODE_bss_8007FFA0[j].bbox.min.y,
                        dword_CODE_bss_8007FFA0[j].bbox.max.x,
                        dword_CODE_bss_8007FFA0[j].bbox.max.y),
                    0);
 
                if (get_debug_do_draw_bg())
                {
                    if (sub_GAME_7F0BD8F0())
                    {
                        gdl = bgRenderRoomPrimary(gdl, dword_CODE_bss_8007FFA0[j].roomid);
                    }
                }
 
                gSPMatrix(gdl++, osVirtualToPhysical((void*)currentPlayerGetProjectionMatrix()), (G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION));
                gdl = fogRenderClearFogMode(gdl);
 
                if (get_debug_do_draw_obj())
                {
                    if (sub_GAME_7F0BD8F0())
                    {
                        gdl = chrpropsRenderPass(gdl, dword_CODE_bss_8007FFA0[j].roomid, 2);
                    }
                }
 
                if (1);
            }
    #ifdef DEBUG
            notdone--;
    #endif
        }
    }
    #ifdef DEBUG
    assert(notdone == 0);
    #endif
 
    gdl = bgScissorCurrentPlayerViewDefault(fogRenderClearFogMode(gdl));
    gSPMatrix(gdl++, osVirtualToPhysical((void*)get_BONDdata_field_10E0()), (G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION));
 
    if (sub_GAME_7F0BD8F0())
    {
        gdl = explosionRenderScorchBuffer(gdl);
        gdl = explosionCallRenderBulletImpactOnProp(gdl);
    }
 
    if (g_BgNumberOfRoomsDrawn);
    if (dword_CODE_bss_8007FFA0);
 
    for (i = b_max; i >= b_min; i--)
    {
    #ifdef DEBUG
        notdone = g_BgNumberOfRoomsDrawn;
    #endif
 
        for (j = 0; j < g_BgNumberOfRoomsDrawn; j++)
        {
            if (i == dword_CODE_bss_8007FFA0[j].unk1)
            {
                gSPMatrix(gdl++, osVirtualToPhysical((void*)get_BONDdata_field_10E0()), (G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION));
                gdl = fogSetRenderFogColor(
                    bgScissorCurrentPlayerViewF(
                        gdl++,
                        dword_CODE_bss_8007FFA0[j].bbox.min.x,
                        dword_CODE_bss_8007FFA0[j].bbox.min.y,
                        dword_CODE_bss_8007FFA0[j].bbox.max.x,
                        dword_CODE_bss_8007FFA0[j].bbox.max.y),
                    1);
 
                if (get_debug_do_draw_bg())
                {
                    if (sub_GAME_7F0BD8F0())
                    {
                        gdl = bgRenderRoomSecondary(gdl, dword_CODE_bss_8007FFA0[j].roomid);
                    }
                }
 
                gSPMatrix(gdl++, osVirtualToPhysical((void*)currentPlayerGetProjectionMatrix()), (G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION));
                gdl = fogRenderClearFogMode(gdl);
 
                if (get_debug_do_draw_obj())
                {
                    if (sub_GAME_7F0BD8F0())
                    {
                        gdl = chrpropsRenderPass(gdl, dword_CODE_bss_8007FFA0[j].roomid, 1);
                    }
                }
 
                if (1);
            }
    #ifdef DEBUG
            notdone--;
    #endif
        }
    }
    #ifdef DEBUG
    assert(notdone == 0);
    #endif
 
    return gdl;
}


/*
 * Address: 0x7F0B4034
*/
s32 getPriMappingBinCount(s32 room)
{
    s32 i = room;

    while (ptr_bgdata_room_fileposition_list[i].pPriMappingBin == 0)
    {
        i++;
    }

    return i;
}


/*
 * Address: 0x7F0B4084
*/
s32 getSecMappingBinCount(s32 room)
{
    s32 i = room;

    while (ptr_bgdata_room_fileposition_list[i].pSecMappingBin == 0)
    {
        i++;
    }

    return i;
}


/*
 * Address: 0x7F0B40D4
*/
s32 getPointTableBinCount(s32 room)
{
    s32 i = room;
    
    while (ptr_bgdata_room_fileposition_list[i].pPointTableBin == 0)
    {
        i++;
    }

    return i;
}


/*
 * Address: 0x7F0B4124
*/
void load_bg_file(LEVEL_INDEX levelid)
{
    typedef struct bg_envdata_entry_local {
        u8 type;
        u8 pad[3];
        s32 data;
    } bg_envdata_entry_local;
    s32 i;
    s32 size;
    s32 header[0x10];
    s32 *data;
 
    levelentry_index = 0;

    for (i = 0; i < MAXROOMCOUNT; i++) 
    {
        g_BgRoomInfo[i].vtx_batch_bounds = NULL;
    }
 
    for (i = 0; i < STAGES_MAX; i++)
    {
        if (levelinfotable[i].levelID == levelid)
        {
            levelentry_index = i;
        }
    }
 
    lightFixtureInitTables();
 
    ptr_bg_data = (s32)header;
    obLoadBGFileBytesAtOffset(levelinfotable[levelentry_index].bg_seg_filename, (u8 *) ptr_bg_data, 0, 0x40);

    if (((levelid && ptr_bg_data) && levelentry_index));

    ptr_bgdata_offsets = ptr_bg_data;
    ptr_bgdata_room_fileposition_list = (bg_room_data *) BG_SEG_TO_PTR(ptr_bg_data, ((s32 *)ptr_bg_data)[1]);
 
    size = (((((u32) ptr_bgdata_room_fileposition_list[1].pPointTableBin) & 0x00ffffff) - 1) | 0xf) + 1;
 
    ptr_bg_data = (s32) mempAllocBytesInBank(size, 4);
    obLoadBGFileBytesAtOffset(levelinfotable[levelentry_index].bg_seg_filename, (u8 *) ptr_bg_data, 0, size);
 
    gptr_stan = (s32) _fileNameLoadToBank(levelinfotable[levelentry_index].bg_stan_filename, 2, 0, 4);
 
    stanDetermineEOF((struct StanPrefixRecord *) gptr_stan, 0, (u8 *) gptr_stan);
    stanLoadFile((struct StanPrefixRecord *) gptr_stan);
 
    sub_GAME_7F0B4810(levelinfotable[levelentry_index].levelscale);
    setLevelScale(levelinfotable[levelentry_index].levelscale);
    setDebugCameraScale(levelinfotable[levelentry_index].levelscale);
    chrRemoved7F022E1C(levelinfotable[levelentry_index].levelscale);
 
    mCurrentLevelVisibilityScale = levelinfotable[levelentry_index].visibility;
 
    sub_GAME_7F08976C(mCurrentLevelVisibilityScale);
    matrix_4x4_7F058C4C(mCurrentLevelVisibilityScale);
 
    data = (s32 *)ptr_bg_data;
    dword_CODE_bss_8007BF98 = *data;
    dword_CODE_bss_8007FF88 = 1;
 
    if (dword_CODE_bss_8007BF98 == 0)
    {
        dword_CODE_bss_8007FF88 = 2;
        ptr_bgdata_offsets = (s32)data;
        ptr_bgdata_room_fileposition_list = (bg_room_data *) BG_SEG_TO_PTR(data, ((s32 *)ptr_bgdata_offsets)[1]);
        
        // Keep this fake goto for matching.
        goto dummy_label_543534; dummy_label_543534: ;
 
        g_MaxNumRooms = 0;

        for (i = 1; ptr_bgdata_room_fileposition_list[i].pPriMappingBin != NULL; i++) 
        {
            g_MaxNumRooms++;  
        }
 
        g_BgPortals = (bg_portal_data_entry *) BG_SEG_TO_PTR(data, ((s32 *)ptr_bgdata_offsets)[2]);

        if (1);

        if (((s32 *)ptr_bgdata_offsets)[3] == 0)
        {
            dword_CODE_bss_8007FF90 = 0;
        }
        else
        {
            dword_CODE_bss_8007FF90 = (s32 *) BG_SEG_TO_PTR(data, ((s32 *)ptr_bgdata_offsets)[3]);
 
            if (((s32 *)ptr_bgdata_offsets)[4] == 0)
            {
                dword_CODE_bss_8007FF94 = NULL;
            }
            else
            {
                dword_CODE_bss_8007FF94 = (f32 *) BG_SEG_TO_PTR(data, ((s32 *)ptr_bgdata_offsets)[4]);
            }
        }
 
        for (i = 0; g_BgPortals[i].offset_portal != (NULL); i++)
        {
            g_BgPortals[i].offset_portal = (bg_portal_entry *) BG_SEG_TO_PTR(ptr_bg_data, g_BgPortals[i].offset_portal);
        }
 
        if (dword_CODE_bss_8007FF90 != NULL)
        {
            for (i = 0; ((bg_envdata_entry_local *)dword_CODE_bss_8007FF90)[i].type != 0; i++)
            {
                if (((bg_envdata_entry_local *)dword_CODE_bss_8007FF90)[i].type == ENVIRONMENTDATA_ALT)
                {
                    ((bg_envdata_entry_local *)dword_CODE_bss_8007FF90)[i].data = getIndexOfPORTALID((s32) BG_SEG_TO_PTR(ptr_bg_data, ((bg_envdata_entry_local *)dword_CODE_bss_8007FF90)[i].data));
                }
            }
        }
 
        for (i = 1; i < g_MaxNumRooms; i++)
        {
            g_BgRoomInfo[i].model_bin_loaded = 0;
            g_BgRoomInfo[i].field_35 = 0;
 
            if (ptr_bgdata_room_fileposition_list[i].pPriMappingBin != (NULL))
            {
                s32 primaryindex;
                s32 secondaryindex;
                primaryindex = getPriMappingBinCount(i + 1);
                secondaryindex = getSecMappingBinCount(i);
 
                if (primaryindex <= secondaryindex)
                {
                    g_BgRoomInfo[i].csize_primary_DL_binary = ((s32) ptr_bgdata_room_fileposition_list[primaryindex].pPriMappingBin) - ((s32) ptr_bgdata_room_fileposition_list[i].pPriMappingBin);
                }
                else
                {
                    g_BgRoomInfo[i].csize_primary_DL_binary = ((s32) ptr_bgdata_room_fileposition_list[secondaryindex].pSecMappingBin) - ((s32) ptr_bgdata_room_fileposition_list[i].pPriMappingBin);
                }
            }
            else
            {
                g_BgRoomInfo[i].csize_primary_DL_binary = 0;
            }
 
            if (ptr_bgdata_room_fileposition_list[i].pSecMappingBin != (NULL))
            {
                s32 primaryindex;
                s32 secondaryindex;
                primaryindex = getPriMappingBinCount(i + 1);
                secondaryindex = getSecMappingBinCount(i + 1);
 
                if (primaryindex <= secondaryindex)
                {
                    g_BgRoomInfo[i].csize_secondary_DL_binary = ((s32) ptr_bgdata_room_fileposition_list[primaryindex].pPriMappingBin) - ((s32) ptr_bgdata_room_fileposition_list[i].pSecMappingBin);
                }
                else
                {
                    g_BgRoomInfo[i].csize_secondary_DL_binary = ((s32) ptr_bgdata_room_fileposition_list[secondaryindex].pSecMappingBin) - ((s32) ptr_bgdata_room_fileposition_list[i].pSecMappingBin);
                }
            }
            else
            {
                g_BgRoomInfo[i].csize_secondary_DL_binary = 0;
            }
 
            if (ptr_bgdata_room_fileposition_list[i].pPointTableBin != (NULL))
            {
                s32 pointindex;
                pointindex = getPointTableBinCount(i + 1);
                g_BgRoomInfo[i].csize_point_index_binary = ((s32) ptr_bgdata_room_fileposition_list[pointindex].pPointTableBin) - ((s32) ptr_bgdata_room_fileposition_list[i].pPointTableBin);
            }
            else
            {
                g_BgRoomInfo[i].csize_point_index_binary = 0;
            }
 
            g_BgRoomInfo[i].cur_room_totalsize = -1;
        }
 
        initializeRoomData();
 
        for (i = 1; i < g_MaxNumRooms; i++)
        {
            sub_GAME_7F0B9338(i);
        }
 
        for (i = 0; g_BgPortals[i].offset_portal != (NULL); i++)
        {
            D_800443C4[i] = sub_GAME_7F0B993C(i);
        }
 
        for (i = 0; g_BgPortals[i].offset_portal != (NULL); i++)
        {
            bgOrderPortal(i);
        }
 
        for (i = 0; i < g_MaxNumRooms; i++)
        {
            sub_GAME_7F0B95D8(i);
        }
 
        for (i = 0; g_BgPortals[i].offset_portal != (NULL); i++)
        {
            g_BgPortals[i].controlbytes1 &= 0xfe;
        }
 
        sub_GAME_7F0B37EC();
    }
 
    fogRemoved7F0BAA5C(levelid);
    g_RoomLoadBudget = 200;
}


void cleanup_rooms(void) {
    unload_rooms();
    matrix_4x4_7F058C4C(1.0);
}


void sub_GAME_7F0B4810(f32 arg0) {
    room_data_float1 = arg0;
    room_data_float2 = (f32) (1.0f / arg0);
}


f32 get_room_data_float2(void){
  return room_data_float2;
}


f32 get_room_data_float1(void){
  return room_data_float1;
}


f32 sub_GAME_7F0B4848(void)
{
    return levelinfotable[levelentry_index].unknownfloat / levelinfotable[levelentry_index].levelscale;
}


//sub_GAME_7F0B4878
f32 bgGetLevelVisibilityScale(void) {
    return mCurrentLevelVisibilityScale;
}


// defined later in the same file
extern void bgRoomsTickUnload(void);

void bgRoomVisibilityRelated(void)
{
    bg_portal_data_entry *portal;
    bg_portal_entry *next;
    coord3d *pos;
    coord3d *pos3;
    u8 *portalflags;
    s32 room;
    s32 portalnum;
    s32 lastportal;
    s32 depth;
    s32 maxdepth;
    s32 offset;
    s32 cammode;

    lastportal = -1;

    num_visible_rooms_in_cur_global_vis_packet = 0;

    if (get_player_position_in_shuffled(get_cur_playernum()) == 0) {
        bgRoomsTickUnload();
    }

    cammode = bondviewGetCameraMode();

    g_RoomLoadBudget = 3;

    switch (cammode) {
    case CAMERAMODE_INTRO:
    case CAMERAMODE_FADESWIRL:
    case CAMERAMODE_SWIRL:
    case CAMERAMODE_POSEND:
    case CAMERAMODE_MP:
        g_RoomLoadBudget = 0xc8;
        break;
    case CAMERAMODE_FP:
    case CAMERAMODE_DEATH_CAM_SP:
    case CAMERAMODE_DEATH_CAM_MP:
    case CAMERAMODE_FP_NOINPUT:
        break;
    }

    room = bondviewGetCurrentPlayersRoom();
    g_BgCurrentRoom = room;

    pos = bondviewGetCurrentPlayersPosition();
    pos3 = bondviewGetCurrentPlayersPosition3();

    // FAKE
    if (1) {}

    for (depth = 0, maxdepth = 11; depth != maxdepth; depth++) {
        for (portalnum = 0; g_BgPortals[portalnum].offset_portal != NULL; portalnum++) {

            if (D_800443C4[portalnum] != 0) {
                continue;
            }
            if (portalnum == lastportal) {
                continue;
            }

            if (((room == g_BgPortals[portalnum].connectedRoom1 || room == g_BgPortals[portalnum].connectedRoom2) && sub_GAME_7F0B9F14(portalnum, pos, pos3))) {
                lastportal = portalnum;

                room = (room ^ g_BgPortals[portalnum].connectedRoom1) ^ g_BgPortals[portalnum].connectedRoom2;

                break;
            }
        }

        if (g_BgPortals[portalnum].offset_portal == NULL) {
            break;
        }
    }

    g_BgCurrentRoom = room;
    bgDetermineVisibleRooms();
}



void addToByteSetMaxSize15(u8* set, u8 newElement) {
    s32 i = 0;

    while (i < 0x10 && set[i] != 0xFF) {
        if (newElement == set[i]) {
            return;
        }
        i++;
    }

    if (i < 0xF) {
        set[i] = newElement;
        set[i+1] = -1;
    }
    return;
}


/**
 * Address: 7F0B4AB4
 */
void bgFindRoomsAlongSegment(coord3d *fromPos, coord3d *toPos, u8 *fromRooms, u8 *finalRooms, s32 *traversedRooms, s32 *traversedRoomCount, s32 maxTraversedRooms)
{
    u8 sourceRooms[16];
    u8 foundRooms[16];
    u8 allRooms[16];
    s32 portalnum;
    s32 i;
    s32 count;
    u8 portalStatuses[PORTMAX];

    for (portalnum = 0; g_BgPortals[portalnum].offset_portal != 0; portalnum++)
    {
        portalStatuses[portalnum] = sub_GAME_7F0B9F14(portalnum, fromPos, toPos);
    }

    for (count = 0; count < 8; count++)
    {
        sourceRooms[count] = fromRooms[count];
    }

    for (count = 0; count < 8; count++)
    {
        allRooms[count] = fromRooms[count];
    }

    count = 0;

    while ((sourceRooms[count] != 0xff) && (count < 16))
    {
        count++;
    }

    count = 0;

    do
    {
        foundRooms[0] = 0xff;

        for (portalnum = 0; g_BgPortals[portalnum].offset_portal != 0; portalnum++)
        {
            for (i = 0; (sourceRooms[i] != 0xff) && (i < 16); i++)
            {
                if (portalStatuses[portalnum] == 1)
                {
                    if (g_BgPortals[portalnum].connectedRoom1 == sourceRooms[i])
                    {
                        addToByteSetMaxSize15(foundRooms, g_BgPortals[portalnum].connectedRoom2);
                        addToByteSetMaxSize15(allRooms, g_BgPortals[portalnum].connectedRoom2);
                        portalStatuses[portalnum] = 0;
                    }
                }

                if (portalStatuses[portalnum] == 2)
                {
                    if (g_BgPortals[portalnum].connectedRoom2 == sourceRooms[i])
                    {
                        addToByteSetMaxSize15(foundRooms, g_BgPortals[portalnum].connectedRoom1);
                        addToByteSetMaxSize15(allRooms, g_BgPortals[portalnum].connectedRoom1);
                        portalStatuses[portalnum] = 0;
                    }
                }
            }
        }

        if (foundRooms[0] == 0xff)
        {
            break;
        }

        for (count = 0; count < 16; count++)
        {
            sourceRooms[count] = foundRooms[count];
        }

        count = 0;

        while ((foundRooms[count] != 0xff) && (count < 16))
        {
            count++;
        }

        count = 0;
    }
    while (foundRooms[count] != 0xff);

    for (count = 0; (count < 7) && (sourceRooms[count] != 0xff); count++)
    {
        finalRooms[count] = sourceRooms[count];
    }

    finalRooms[count] = 0xff;

    for (count = 0; (count < maxTraversedRooms) && (allRooms[count] != 0xff); count++)
    {
        traversedRooms[count] = allRooms[count];
    }

    *traversedRoomCount = count;
}


/**
 * Address 0x7F0B4E40.
*/
Gfx *bgLevelRender(Gfx *arg0)
{
    gSPSetLights1(arg0++, GlobalLight);
    gSPLookAt(arg0++, sub_GAME_7F078474());
    gSPSegment(arg0++, SPSEGMENT_BG_DL, ptr_bg_data);

    if (dword_CODE_bss_8007FF88 == 1)
    {
        gSPDisplayList(arg0++, dword_CODE_bss_8007BF98);
    }
    else
    {
        arg0 = fogRenderClearFogMode(bgScissorCurrentPlayerViewDefault(sub_GAME_7F0B8D78(fogSetRenderFogColor(arg0, 0))));
    }

    gSPMatrix(arg0++, g_viProjectionMatrix, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION);

    return bondviewGfxPlayerField5cMatrix(arg0++);
}





f32 sub_GAME_7F0B4F9C(s32 arg0)
{
	return dword_CODE_bss_8007FF94[arg0 + 1];
}






/**
 * Calls @see bgScissorCurrentPlayerView with default current player values.
 * Address 0x7F0B4FB4.
 */
Gfx* bgScissorCurrentPlayerViewDefault(Gfx* arg0)
{
    return bgScissorCurrentPlayerView(
        arg0,
        g_CurrentPlayer->viewleft,
        g_CurrentPlayer->viewtop,
        g_CurrentPlayer->viewleft + g_CurrentPlayer->viewx,
        g_CurrentPlayer->viewtop + g_CurrentPlayer->viewy);
}





/**
 * Same as @see bgScissorCurrentPlayerView, but accepts float parameters.
 * Address 0x7F0B4FF4.
 */
Gfx* bgScissorCurrentPlayerViewF(Gfx* arg0, f32 arg1, f32 arg2, f32 arg3, f32 arg4)
{
    return bgScissorCurrentPlayerView(
        arg0,
        (s32)arg1,
        (s32)arg2,
        ceilFloatToInt(arg3),
        ceilFloatToInt(arg4));
}





/**
 * Specifies the drawing area (the scissoring box).
 * View is bound to current player view properties, but parameters can clip to smaller area.
 *
 * @param arg0: Display list pointer
 * @param left: Screen's left edge coordinates. Must be >= g_CurrentPlayer->viewleft otherwise ignored.
 * @param top: Screen's top edge coordinates. Must be >= g_CurrentPlayer->viewtop otherwise ignored.
 * @param width: Screen's right edge coordinates. Must be <= g_CurrentPlayer->viewleft+viewx otherwise ignored.
 * @param height: Screen's left bottom coordinates. Must be <= g_CurrentPlayer->viewtop+viewy otherwise ignored.
 *
 * Address 0x7F0B5058.
*/
Gfx *bgScissorCurrentPlayerView(Gfx *arg0, s32 left, s32 top, s32 width, s32 height)
{
    struct player *temp_v0;

    temp_v0 = g_CurrentPlayer;

    if (left < (s32) temp_v0->viewleft)
    {
        left = (s32) temp_v0->viewleft;
    }

    if (top < (s32) temp_v0->viewtop)
    {
        top = (s32) temp_v0->viewtop;
    }

    if (temp_v0->viewleft + temp_v0->viewx < width)
    {
        width = temp_v0->viewleft + temp_v0->viewx;
    }

    if (temp_v0->viewtop + temp_v0->viewy < height)
    {
        height = temp_v0->viewtop + temp_v0->viewy;
    }

    gDPSetScissor(arg0++, G_SC_NON_INTERLACE, left, top, width, height);

    return arg0;
}


void sub_GAME_7F0B5168(void) 
{
    s32 i;

    for (i = 0; i < PORTMAX; i++) 
    {
        table_for_portals[i].unk0 = -1;
    }
}


/**
 * Unreferenced.
 *
 * Loosely checks that arg1 surrounds arg0. Requires points be ordered according to min/max.
 *
 * Address 0x7F0B519C.
 */
s32 bgRectIsInside(struct bbox2d *arg0, struct bbox2d *arg1)
{
    if (arg1->min.x <= arg0->min.x)
    {
        if (arg0->min.x <= arg1->max.x)
        {
            if (arg1->min.y <= arg0->min.y)
            {
                if (arg0->min.y <= arg1->max.y)
                {
                    return 1;
                }
            }
        }
    }
    return 0;
}


/**
 * Address: 7F0B5208
 */
bool bgIsRoomOnScreen(s32 roomID, struct rectbbox *screenbox)
{
    s32 i;
    coord3d projected;
    coord3d corner;
    s32 count_z;
    s32 count_failed_projection;
    s32 count_left;
    s32 count_right;
    s32 count_top;
    s32 count_bottom;
    f32 zrange[2];

    count_z = 0;
    count_failed_projection = 0;
    count_left = 0;
    count_right = 0;
    count_top = 0;
    count_bottom = 0;

    viGetZRange(zrange);

    zrange[1] = zrange[1] / mCurrentLevelVisibilityScale;

    for (i = 0; i < 8; i++) {
        if (i & 1) {
            corner.x = g_BgRoomInfo[roomID].minbounds.x;
        } else {
            corner.x = g_BgRoomInfo[roomID].maxbounds.x;
        }

        if (i & 2) {
            corner.y = g_BgRoomInfo[roomID].minbounds.y;
        } else {
            corner.y = g_BgRoomInfo[roomID].maxbounds.y;
        }

        if (i & 4) {
            corner.z = g_BgRoomInfo[roomID].minbounds.z;
        } else {
            corner.z = g_BgRoomInfo[roomID].maxbounds.z;
        }

        if (bgProjectRoomCoordToScreen(&corner, &projected) == 0) {
            if (zrange[1] <= -projected.z) {
                count_z++;
            }

            if (screenbox->left <= projected.x) {
                count_left++;
            }

            if (projected.x <= screenbox->right) {
                count_right++;
            }

            if (screenbox->up <= projected.y) {
                count_top++;
            }

            if (projected.y <= screenbox->down) {
                count_bottom++;
            }

            count_failed_projection++;
        } else {
            if (zrange[1] <= -projected.z) {
                count_z++;
            }

            if (projected.x <= screenbox->left) {
                count_left++;
            } else if (screenbox->right <= projected.x) {
                count_right++;
            }

            if (projected.y <= screenbox->up) {
                count_top++;
            } else if (screenbox->down <= projected.y) {
                count_bottom++;
            }
        }
    }

    /**
     * If all 8 of the room's bounding box corners are behind the camera,
     * reject this room
     */
    if (count_failed_projection == 8
            || count_z == 8
            || count_left == 8
            || count_right == 8
            || count_top == 8
            || count_bottom == 8) {
        return FALSE;
    }

    return TRUE;
}


/**
 * Address: 7F0B5488
 */
bool bgProjectRoomCoordToScreen(coord3d* src, coord3d* dst)
{
    Mtxf* temp_a0;
    s32 var_v0;

    temp_a0 = camGetWorldToScreenMtxf();
    dst->x = src->x * room_data_float2;
    dst->y = src->y * room_data_float2;
    dst->z = src->z * room_data_float2;
    mtx4TransformVecInPlace(temp_a0, dst);

    transform3Dto2DWithZScaling(dst, dst);

    if (dst->z > 0.0f)
    {
        return FALSE;
    }
    else
    {
        return TRUE;
    }
}


s32 sub_GAME_7F0B5528(s32 portalnum, f32 arg1, coord3d *arg2)
{
    Mtxf *matrix;
    coord3d *point;
    s32 i;
    s32 next;
    f32 zrange[2];
    s32 allbehind;
    struct PortalMetric metric;
    s32 len;

    matrix = camGetWorldToScreenMtxf();
    allbehind = 1;
    viGetZRange(zrange);
    zrange[1] /= mCurrentLevelVisibilityScale;

    for (i = 0; i < g_BgPortals[portalnum].offset_portal->numPoints; i++) {
        point = &arg2[i];

        point->x = (&g_BgPortals[portalnum].offset_portal->point)[i].x;
        point->y = (&g_BgPortals[portalnum].offset_portal->point)[i].y;
        point->z = (&g_BgPortals[portalnum].offset_portal->point)[i].z;

        if (arg1 != 0.0f) {
            sub_GAME_7F0B96CC(portalnum, (f32 *) &metric);

            point->x += metric.normal.x * arg1;
            point->y += metric.normal.y * arg1;
            point->z += metric.normal.z * arg1;
        }

        point = &arg2[i];
        point->x *= room_data_float2;
        point->y *= room_data_float2;
        point->z *= room_data_float2;

        mtx4TransformVecInPlace(matrix, point);

        if (-zrange[1] * 0.9f < point->z) {
            allbehind = 0;
        }
    }

    if (allbehind) {
        return 0;
    }

    len = g_BgPortals[portalnum].offset_portal->numPoints;

    for (i = 0; i < g_BgPortals[portalnum].offset_portal->numPoints; i++) {
        point = &arg2[i];
        next = (i + 1) % g_BgPortals[portalnum].offset_portal->numPoints;

        if ((point->z > 0.0f && arg2[next].z <= 0.0f) || (point->z <= 0.0f && arg2[next].z > 0.0f)) {
            f32 scale = -point->z / (arg2[next].z - point->z);

            arg2[len].x = point->x + ((arg2[next].x - point->x) * scale);
            arg2[len].y = point->y + ((arg2[next].y - point->y) * scale);
            point = &arg2[i];
            arg2[len].z = 0.0f;
            len++;
        }
    }

    return len;
}


s32 sub_GAME_7F0B5864(s32 portalnum, bbox2d *bbox)
{
    f32 scale;
    PortalCache *cache;
    s32 j;
    coord3d points[19];
    coord2d screenpos;
    bbox2d bounds;
    s32 pointcount;
    s32 onscreencount;
    s32 i;

    cache = &table_for_portals[portalnum];

    if (cache->count >= 0)
    {
        bbox->min.x = cache->bbox.min.x;
        bbox->min.y = cache->bbox.min.y;
        bbox->max.x = cache->bbox.max.x;
        bbox->max.y = cache->bbox.max.y;
        
        return cache->count;
    }

    scale = sub_GAME_7F0B9990(portalnum);
    pointcount = sub_GAME_7F0B5528(portalnum, scale, points);

    if (scale > 0.0f)
    {
        pointcount += sub_GAME_7F0B5528(portalnum, -scale, &points[pointcount]);
    }

    onscreencount = 0;
    i = 0;

    if (pointcount > 0)
    {
        j = 0;

        do
        {
            if (points[j].z <= 0.0f)
            {
                transform3Dto2DWithZScaling(&points[j], &screenpos);

                if (onscreencount == 0)
                {
                    bounds.min.x = (bounds.max.x = screenpos.x);
                    bounds.min.y = (bounds.f[1][1] = screenpos.f[1]);
                }
                else
                {
                    if (screenpos.x < bounds.min.x)
                    {
                        bounds.min.x = screenpos.x;
                    }
                    
                    if (bounds.max.x < screenpos.x)
                    {
                        bounds.max.x = screenpos.x;
                    }
                    
                    if (screenpos.y < bounds.min.y)
                    {
                        bounds.min.y = screenpos.y;
                    }
                    
                    if (bounds.max.y < screenpos.y)
                    {
                        bounds.max.y = screenpos.y;
                    }
                }

            onscreencount++;
            }

        i++;
        j++;
            
        }
        while (i != pointcount);
    }

    if (onscreencount == 0)
    {
        bounds.max.y = (bounds.min.x = 0.0f);
        bounds.min.y = 0.0f;
        bounds.max.x = 0.0f;
    }
    else
    {
        if ((bounds.max.x <= bounds.min.x) || (bounds.max.y <= bounds.min.y))
        {
            bounds.min.x = g_CurrentPlayer->screensize.min.x;
            bounds.min.y = g_CurrentPlayer->screensize.min.y;
            bounds.max.x = g_CurrentPlayer->screensize.max.x;
            bounds.max.y = g_CurrentPlayer->screensize.max.y;
        }
    }

    bbox->min.x = bounds.min.x;
    bbox->min.y = bounds.min.y;
    bbox->max.x = bounds.max.x;
    bbox->max.y = bounds.max.y;

    cache->bbox.min.x = bbox->min.x;
    cache->bbox.min.y = bbox->min.y;
    cache->bbox.max.x = bbox->max.x;
    cache->bbox.max.y = bbox->max.y;
    cache->count = onscreencount;

    return onscreencount;
}


/**
 * Unreferenced.
 *
 * Address 0x7F0B5B14.
 */
Gfx *bgFillRectangle(Gfx *gdl, s32 ulx, s32 uly, s32 lrx, s32 lry)
{
    gDPFillRectangle(gdl++, ulx, uly, lrx + 1, lry + 1);
    return gdl;
}


/**
 * Unreferenced.
 */
void bgFillRectangleWithSides(Gfx *gdl, s32 ulx, s32 uly, s32 lrx, s32 lry)
{
    bgFillRectangle(
        bgFillRectangle(
            bgFillRectangle(
                bgFillRectangle(gdl,
                ulx, uly, lrx, uly), /* full rectangle */
                lrx, uly, lrx, lry), /* right side */
                ulx, lry, lrx, lry), /* bottom */
                ulx, uly, ulx, lry); /* top */
}



/**
 * Determines if two rectangles overlap, adjusting first argument to be the intersection.
 *
 * Returns 0 if no intersection (or edge equality), 1 otherwise.
 *
 * Address 0x7F0B5BDC.
 */
s32 bgRectIntersect(struct bbox2d *a, struct bbox2d *b)
{
	a->min.x = a->min.x > b->min.x ? a->min.x : b->min.x;
	a->min.y = a->min.y > b->min.y ? a->min.y : b->min.y;
	a->max.x = b->max.x > a->max.x ? a->max.x : b->max.x;
	a->max.y = b->max.y > a->max.y ? a->max.y : b->max.y;

	if (a->min.x >= a->max.x) {
		a->min.x = a->max.x;
		return FALSE;
	}

	if (a->max.y <= a->min.y) {
		a->min.y = a->max.y;
		return FALSE;
	}

	return TRUE;
}


// Address: 0x7F0B5CC0
// Does a union. Increases the size of 'a' so it contains 'b'.
void bgRectOutersect(struct bbox2d *a, struct bbox2d *b)
{
    (a->min).x = ((a->min).x < (b->min).x) ? (a->min).x : (b->min).x;
    (a->min).y = ((a->min).y < (b->min).y) ? (a->min).y : (b->min).y;
    (a->max).x = ((a->max).x > (b->max).x) ? (a->max).x : (b->max).x;
    (a->max).y = ((a->max).y > (b->max).y) ? (a->max).y : (b->max).y;
}

// Address: 0x7f0b5d58
// Does a shallow copy of 'b' into 'a'. Equivalent to '*a = *b;'.
void bbox2dCopy(struct bbox2d *a, struct bbox2d *b)
{
    (a->min).x = (b->min).x;
    (a->min).y = (b->min).y;
    (a->max).x = (b->max).x;
    (a->max).y = (b->max).y;
}


/**
 * Formats a portal ID for debug output.
 */
char *bgDebPrintPORTALID(s32 portID)
{
    static char bgDebPortalOutBuffer[10][9];
    static s32 bgDebPortalOutLineNum = 0;
    char *portIdStr;

    bgDebPortalOutLineNum = (bgDebPortalOutLineNum + 1) % 10;
    portIdStr = bgDebPortalOutBuffer[bgDebPortalOutLineNum];

    sprintf(portIdStr, "PORT%d", portID);

    return portIdStr;
}


/**
 * Formats a room ID for debug output.
 */
char *bgDebPrintROOMID(s32 roomId)
{
    static char bgDebRoomOutBuffer[10][9];
    static s32 bgDebRoomOutLineNum = 0;
    char *roomIdStr;

    bgDebRoomOutLineNum = (bgDebRoomOutLineNum + 1) % 10;
    roomIdStr = bgDebRoomOutBuffer[bgDebRoomOutLineNum];

    sprintf(roomIdStr, "ROOM%d", roomId);

    return roomIdStr;
}


/**
 * These definitions must come AFTER bgDebPrintROOMID for matching.
 */

/**
 * address 8007C100
 * EU .bss 8007A040
*/
bg_queued_portal_entry g_BgPortalQueue[BG_PORTAL_QUEUE_LEN];

#ifdef VERSION_EU
GLOBAL_ASM(
.bss
.balign 4
glabel g_BgPortals
.space 4
glabel ptr_bgdata_offsets
.space 4
glabel dword_CODE_bss_8007FF88
.space 4
glabel ptr_bgdata_room_fileposition_list
.space 4
glabel dword_CODE_bss_8007FF90
.space 4
glabel dword_CODE_bss_8007FF94
.space 4
glabel dword_CODE_bss_8007FFA0
.space 0xba0
)

u32 missingeubytes[4];
s32 dword_CODE_bss_800815f0;
s32 dword_CODE_bss_800815f4;
s32 dword_CODE_bss_800815f8;
s32 eu_bss_8007BFA0;
s32 eu_bss_8007BFA4;
#else
GLOBAL_ASM(
.bss
.balign 4
glabel g_BgPortals
.space 4
glabel ptr_bgdata_offsets
.space 4
glabel dword_CODE_bss_8007FF88
.space 4
glabel ptr_bgdata_room_fileposition_list
.space 4
glabel dword_CODE_bss_8007FF90
.space 4
glabel dword_CODE_bss_8007FF94
.space 4
)

s32 dword_CODE_bss_8007FF98;
s32 dword_CODE_bss_8007FF9C;

GLOBAL_ASM(
.bss
.balign 4
glabel dword_CODE_bss_8007FFA0
.space 0x1650
)

s32 dword_CODE_bss_800815f0;
s32 dword_CODE_bss_800815f4;
s32 dword_CODE_bss_800815f8;
#endif


//D:80044868
BoundVec D_80044868 = {0x7FFF, 0x7FFF, 0x7FFF};
//D:80044874
BoundVec D_80044874 = {-0x8000, -0x8000, -0x8000};
//D:80044880
BoundVec D_80044880 = {0x7FFF, 0x7FFF, 0x7FFF};
//D:8004488C
BoundVec D_8004488C = {-0x8000, -0x8000, -0x8000};
//D:80044898
s32 D_80044898 = 0;
//D:8004489C
s32 D_8004489C = 0xF;
//D:800448A0
s32 g_BgPortalQueueWriteIndex = 0;
//D:800448A4
s32 g_BgPortalQueueReadIndex = 0;

/**
 * Local stack.
 *
 * Address 0x800448A8.
 */
s32 g_BgStack[BG_STACK_SIZE] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

/**
 * Current top of the stack.
 *
 * Address 0x800448F8.
 */
s32 g_BgStackCount = 0;

//D:800448FC
s32 current_visibility = 0;

//D:80044900
f32 D_80044900 = 0;

//D:80044904
s32 D_80044904 = 0x7F7FFFFF;
//D:80044908
s32 D_80044908 = 0x7F7FFFFF;
//D:8004490C
s32 D_8004490C = 0x7F7FFFFF;
//D:80044910
s32 D_80044910 = 0xFF7FFFFF;
//D:80044914
s32 D_80044914 = 0xFF7FFFFF;
//D:80044918
s32 D_80044918 = 0xFF7FFFFF;
//D:8004491C
u32 D_8004491C = 0;
//D:80044920
u32 D_80044920 = 0;
//D:80044924
u32 D_80044924 = 0;

#if defined(VERSION_EU)
s32 eu_cdata_0x1f1c0 = 0;
s32 eu_cdata_0x1f1c4 = 0;
#endif

/*
    //###RenderMode / Combiner Look - Up - Tables
    The reason for this LUT is to dynamicly change the rendermode and combiner to
    FOG / NoFog or any other setting they might have wanted to test during development
    as it applies during runtime

//###Reminder:
    1cycle combiners repeat both cycles
    gDPSetCombineMode(G_CC_MODULATERGBA, G_CC_MODULATERGBA2)

    combiner macros are a list of parameters that form a mathematical sum.
                            (       -  )*     +  ,  (       -  )*     +
    G_CC_MODULATERGBA2	    COMBINED, 0, SHADE, 0, COMBINED, 0, SHADE, 0
*/
#if 0
//New Defines to be added to gbi.h
/*custom combiner for triangle alpha*/
#define	ModulateRGB_EnvA 	TEXEL0, 0, SHADE, 0, 0, 0, 0, ENVIRONMENT
/*custom combiner for triangle alpha*/
#define	ModulateRGB_EnvA2 	COMBINED, 0, SHADE, 0, 0, 0, 0, ENVIRONMENT
/*custom combiner for Texture*triangle alpha*/
#define	ModulateRGBA_EnvA 	TEXEL0, 0, SHADE, 0, TEXEL0, 0, ENVIRONMENT, 0
/*custom combiner for texture*triangle alpha*/
#define	ModulateRGBA_EnvA2 	COMBINED, 0, SHADE, 0, COMBINED, 0, ENVIRONMENT, 0
/*custom combiner for triangle alpha*/
#define	SHADE_EnvA 		    0, 0, SHADE, 0, 0, 0, 0, ENVIRONMENT
/*Tri-linear filter colour, flat tile alpha (for cutouts)*/
#define TLRGB_ATile1        TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0, 1, 0, TEXEL1, 0
#endif
//D:80044928
Gfx DL_LUT_UNKNOWN[] = {
    gsDPSetCombineMode(G_CC_TRILERP, G_CC_MODULATEIA2),
    gsDPSetCombineLERP(TEXEL1, 0, SCALE, 0,  TEXEL1, 0, PRIM_LOD_FRAC, 0,  0, 0, 0, COMBINED,  0, 0, 0, COMBINED),
    0,0
};

//D:80044940 - Primary
Gfx DL_LUT_PRIMARY_ADDFOG[] = {
    //Add FOG to all rendermodes
    //Standard HiQuality Surface to Standard Fogable HiQuality Surface
    gsDPSetRenderMode(G_RM_PASS, G_RM_AA_ZB_OPA_SURF2),
    gsDPSetRenderMode(G_RM_FOG_SHADE_A, G_RM_AA_ZB_OPA_SURF2),
    //Terrain to Fogable Terrain
    gsDPSetRenderMode(G_RM_PASS, G_RM_AA_ZB_OPA_TERR2),
    gsDPSetRenderMode(G_RM_FOG_SHADE_A, G_RM_AA_ZB_OPA_TERR2),
    //Standard DECAL to FOG DECAL
    gsDPSetRenderMode(G_RM_PASS, G_RM_AA_ZB_OPA_DECAL2),
    gsDPSetRenderMode(G_RM_FOG_SHADE_A, G_RM_AA_ZB_OPA_DECAL2),
    //Transparent DECAL to  FOG Transparent DECAL
    gsDPSetRenderMode(G_RM_PASS, G_RM_AA_ZB_XLU_DECAL2),
    gsDPSetRenderMode(G_RM_FOG_SHADE_A, G_RM_AA_ZB_XLU_DECAL2),
    //Transparent Surface to FOG Transparent Surface
    gsDPSetRenderMode(G_RM_PASS, G_RM_AA_ZB_XLU_SURF2),
    gsDPSetRenderMode(G_RM_FOG_SHADE_A, G_RM_AA_ZB_XLU_SURF2),
    // Billboard Cut-out to FOG Billboard Cut-out - eg, Mario Tree or Depot lamp
    // See PGDLists\Transparent Textures.htm for more info
    gsDPSetRenderMode(G_RM_PASS, G_RM_AA_ZB_TEX_EDGE2),
    gsDPSetRenderMode(G_RM_FOG_SHADE_A, G_RM_AA_ZB_TEX_EDGE2),
    //Standard Z-Less OPA to Standard FOG Z-Less OPA
    gsDPSetRenderMode(G_RM_PASS, G_RM_AA_OPA_SURF2),
    gsDPSetRenderMode(G_RM_FOG_SHADE_A, G_RM_AA_OPA_SURF2),
    //Z-Less OPA Terrain to Z-Less Fog OPA Terrain
    gsDPSetRenderMode(G_RM_PASS, G_RM_AA_OPA_TERR2),
    gsDPSetRenderMode(G_RM_FOG_SHADE_A, G_RM_AA_OPA_TERR2),
    0x0, 0x0
};

//D:800449C8 - Secondary
Gfx DL_LUT_SECONDARY_ADDFOG[] = {
    //Add FOG to Rendermodes
    //Transparent DECAL to  FOG Transparent DECAL
    gsDPSetRenderMode(G_RM_PASS, G_RM_AA_ZB_XLU_DECAL2),
    gsDPSetRenderMode(G_RM_FOG_SHADE_A, G_RM_AA_ZB_XLU_DECAL2),
    //Transparent Surface to FOG Transparent Surface
    gsDPSetRenderMode(G_RM_PASS, G_RM_AA_ZB_XLU_SURF2),
    gsDPSetRenderMode(G_RM_FOG_SHADE_A, G_RM_AA_ZB_XLU_SURF2),

    // Billboard Cut-out to FOG Billboard Cut-out - eg, Mario Tree or Depot lamp
    gsDPSetRenderMode(G_RM_PASS, G_RM_AA_ZB_TEX_EDGE2),
    gsDPSetRenderMode(G_RM_FOG_SHADE_A, G_RM_AA_ZB_TEX_EDGE2),

    // Swap all refrences to Shade in Alpha to Environment
    gsDPSetCombineMode(G_CC_TRILERP, G_CC_MODULATEIA2),
    gsDPSetCombineLERP(TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0,  TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0,  COMBINED, 0, SHADE, 0,  COMBINED, 0, ENVIRONMENT, 0),
    gsDPSetCombineLERP(TEXEL0, 0, SHADE, 0,  TEXEL0, 0, SHADE, 0,  TEXEL0, 0, SHADE, 0,  TEXEL0, 0, SHADE, 0),
    gsDPSetCombineLERP(TEXEL0, 0, SHADE, 0,  TEXEL0, 0, ENVIRONMENT, 0,  TEXEL0, 0, SHADE, 0,  TEXEL0, 0, ENVIRONMENT, 0),
    gsDPSetCombineMode(G_CC_TRILERP, G_CC_MODULATEI2),
    gsDPSetCombineLERP(TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0,  TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0,  COMBINED, 0, SHADE, 0,  0, 0, 0, ENVIRONMENT),
    gsDPSetCombineLERP(TEXEL0, 0, SHADE, 0,  0, 0, 0, SHADE,  TEXEL0, 0, SHADE, 0,  0, 0, 0, SHADE),
    gsDPSetCombineLERP(TEXEL0, 0, SHADE, 0,  0, 0, 0, ENVIRONMENT,  TEXEL0, 0, SHADE, 0,  0, 0, 0, ENVIRONMENT),
    gsDPSetCombineMode(G_CC_TRILERP, G_CC_MODULATEIA2),
    gsDPSetCombineLERP(TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0,  TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0,  COMBINED, 0, SHADE, 0,  COMBINED, 0, ENVIRONMENT, 0),
    gsDPSetCombineLERP(TEXEL0, 0, SHADE, 0,  TEXEL0, 0, SHADE, 0,  TEXEL0, 0, SHADE, 0,  TEXEL0, 0, SHADE, 0),
    gsDPSetCombineLERP(TEXEL0, 0, SHADE, 0,  TEXEL0, 0, ENVIRONMENT, 0,  TEXEL0, 0, SHADE, 0,  TEXEL0, 0, ENVIRONMENT, 0),
    gsDPSetCombineMode(G_CC_TRILERP, G_CC_MODULATEI2),
    gsDPSetCombineLERP(TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0,  TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0,  COMBINED, 0, SHADE, 0,  0, 0, 0, ENVIRONMENT),
    gsDPSetCombineLERP(TEXEL0, 0, SHADE, 0,  0, 0, 0, SHADE,  TEXEL0, 0, SHADE, 0,  0, 0, 0, SHADE),
    gsDPSetCombineLERP(TEXEL0, 0, SHADE, 0,  0, 0, 0, ENVIRONMENT,  TEXEL0, 0, SHADE, 0,  0, 0, 0, ENVIRONMENT),
    gsDPSetCombineLERP(0, 0, 0, SHADE,  0, 0, 0, SHADE,  0, 0, 0, COMBINED,  0, 0, 0, COMBINED),
    gsDPSetCombineLERP(0, 0, 0, SHADE,  0, 0, 0, ENVIRONMENT,  0, 0, 0, COMBINED,  0, 0, 0, COMBINED),
    gsDPSetCombineLERP(0, 0, 0, SHADE, 0, 0, 0, SHADE, 0, 0, 0, SHADE, 0, 0, 0, SHADE),//gsDPSetCombineMode(G_CC_SHADE, G_CC_SHADE),
    gsDPSetCombineLERP(0, 0, 0, SHADE,  0, 0, 0, ENVIRONMENT,  0, 0, 0, SHADE,  0, 0, 0, ENVIRONMENT), //gsDPSetCombineMode(G_CC_SHADE_EnvA, G_CC_SHADE_EnvA),
    // This one is an oddball... its extra here AND is weird using Tile1 only for Alpha
    gsDPSetCombineLERP(TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0,  1, 0, TEXEL1, 0,  COMBINED, 0, SHADE, 0,  COMBINED, 0, SHADE, 0),
    gsDPSetCombineLERP(TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0,  1, 0, TEXEL1, 0,  COMBINED, 0, SHADE, 0,  COMBINED, 0, ENVIRONMENT, 0),
    0x0,
    0x0
};

//D:80044AB0
// Loaded once on first time entering level, only once ever
// Swap all refrences to Shade in Alpha to Environment
Gfx DL_LUT_PRIMARY[] = {
    gsDPSetCombineMode(G_CC_TRILERP, G_CC_MODULATEIA2),
    gsDPSetCombineLERP(TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0,  TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0,  COMBINED, 0, SHADE, 0,  COMBINED, 0, ENVIRONMENT, 0),
    gsDPSetCombineLERP(TEXEL0, 0, SHADE, 0,  TEXEL0, 0, SHADE, 0,  TEXEL0, 0, SHADE, 0,  TEXEL0, 0, SHADE, 0),
    gsDPSetCombineLERP(TEXEL0, 0, SHADE, 0,  TEXEL0, 0, ENVIRONMENT, 0,  TEXEL0, 0, SHADE, 0,  TEXEL0, 0, ENVIRONMENT, 0),
    gsDPSetCombineMode(G_CC_TRILERP, G_CC_MODULATEI2),
    gsDPSetCombineLERP(TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0,  TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0,  COMBINED, 0, SHADE, 0,  0, 0, 0, ENVIRONMENT),
    gsDPSetCombineLERP(TEXEL0, 0, SHADE, 0,  0, 0, 0, SHADE,  TEXEL0, 0, SHADE, 0,  0, 0, 0, SHADE),
    gsDPSetCombineLERP(TEXEL0, 0, SHADE, 0,  0, 0, 0, ENVIRONMENT,  TEXEL0, 0, SHADE, 0,  0, 0, 0, ENVIRONMENT),
    gsDPSetCombineMode(G_CC_TRILERP, G_CC_MODULATEIA2),
    gsDPSetCombineLERP(TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0,  TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0,  COMBINED, 0, SHADE, 0,  COMBINED, 0, ENVIRONMENT, 0),
    gsDPSetCombineLERP(TEXEL0, 0, SHADE, 0,  TEXEL0, 0, SHADE, 0,  TEXEL0, 0, SHADE, 0,  TEXEL0, 0, SHADE, 0),
    gsDPSetCombineLERP(TEXEL0, 0, SHADE, 0,  TEXEL0, 0, ENVIRONMENT, 0,  TEXEL0, 0, SHADE, 0,  TEXEL0, 0, ENVIRONMENT, 0),
    gsDPSetCombineMode(G_CC_TRILERP, G_CC_MODULATEI2),
    gsDPSetCombineLERP(TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0,  TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0,  COMBINED, 0, SHADE, 0,  0, 0, 0, ENVIRONMENT),
    gsDPSetCombineLERP(TEXEL0, 0, SHADE, 0,  0, 0, 0, SHADE,  TEXEL0, 0, SHADE, 0,  0, 0, 0, SHADE),
    gsDPSetCombineLERP(TEXEL0, 0, SHADE, 0,  0, 0, 0, ENVIRONMENT,  TEXEL0, 0, SHADE, 0,  0, 0, 0, ENVIRONMENT),
    gsDPSetCombineLERP(0, 0, 0, SHADE,  0, 0, 0, SHADE,  0, 0, 0, COMBINED,  0, 0, 0, COMBINED),
    gsDPSetCombineLERP(0, 0, 0, SHADE,  0, 0, 0, ENVIRONMENT,  0, 0, 0, COMBINED,  0, 0, 0, COMBINED),
    gsDPSetCombineLERP(0, 0, 0, SHADE,  0, 0, 0, SHADE,  0, 0, 0, SHADE,  0, 0, 0, SHADE),
    gsDPSetCombineLERP(0, 0, 0, SHADE,  0, 0, 0, ENVIRONMENT,  0, 0, 0, SHADE,  0, 0, 0, ENVIRONMENT),
    0,0
};

//D:80044B58
// Loaded once on first time entering level, only once ever
// Swap all refrences to Shade in Alpha to Environment
Gfx DL_LUT_SECONDARY[] = {
    gsDPSetCombineMode(G_CC_TRILERP, G_CC_MODULATEIA2),
    gsDPSetCombineLERP(TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0,  TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0,  COMBINED, 0, SHADE, 0,  COMBINED, 0, ENVIRONMENT, 0),
    gsDPSetCombineLERP(TEXEL0, 0, SHADE, 0,  TEXEL0, 0, SHADE, 0,  TEXEL0, 0, SHADE, 0,  TEXEL0, 0, SHADE, 0),
    gsDPSetCombineLERP(TEXEL0, 0, SHADE, 0,  TEXEL0, 0, ENVIRONMENT, 0,  TEXEL0, 0, SHADE, 0,  TEXEL0, 0, ENVIRONMENT, 0),
    gsDPSetCombineMode(G_CC_TRILERP, G_CC_MODULATEI2),
    gsDPSetCombineLERP(TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0,  TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0,  COMBINED, 0, SHADE, 0,  0, 0, 0, ENVIRONMENT),
    gsDPSetCombineLERP(TEXEL0, 0, SHADE, 0,  0, 0, 0, SHADE,  TEXEL0, 0, SHADE, 0,  0, 0, 0, SHADE),
    gsDPSetCombineLERP(TEXEL0, 0, SHADE, 0,  0, 0, 0, ENVIRONMENT,  TEXEL0, 0, SHADE, 0,  0, 0, 0, ENVIRONMENT),
    gsDPSetCombineMode(G_CC_TRILERP, G_CC_MODULATEIA2),
    gsDPSetCombineLERP(TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0,  TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0,  COMBINED, 0, SHADE, 0,  COMBINED, 0, ENVIRONMENT, 0),
    gsDPSetCombineLERP(TEXEL0, 0, SHADE, 0,  TEXEL0, 0, SHADE, 0,  TEXEL0, 0, SHADE, 0,  TEXEL0, 0, SHADE, 0),
    gsDPSetCombineLERP(TEXEL0, 0, SHADE, 0,  TEXEL0, 0, ENVIRONMENT, 0,  TEXEL0, 0, SHADE, 0,  TEXEL0, 0, ENVIRONMENT, 0),
    gsDPSetCombineMode(G_CC_TRILERP, G_CC_MODULATEI2),
    gsDPSetCombineLERP(TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0,  TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0,  COMBINED, 0, SHADE, 0,  0, 0, 0, ENVIRONMENT),
    gsDPSetCombineLERP(TEXEL0, 0, SHADE, 0,  0, 0, 0, SHADE,  TEXEL0, 0, SHADE, 0,  0, 0, 0, SHADE),
    gsDPSetCombineLERP(TEXEL0, 0, SHADE, 0,  0, 0, 0, ENVIRONMENT,  TEXEL0, 0, SHADE, 0,  0, 0, 0, ENVIRONMENT),
    gsDPSetCombineLERP(0, 0, 0, SHADE,  0, 0, 0, SHADE,  0, 0, 0, COMBINED,  0, 0, 0, COMBINED),
    gsDPSetCombineLERP(0, 0, 0, SHADE,  0, 0, 0, ENVIRONMENT,  0, 0, 0, COMBINED,  0, 0, 0, COMBINED),
    gsDPSetCombineLERP(0, 0, 0, SHADE,  0, 0, 0, SHADE,  0, 0, 0, SHADE,  0, 0, 0, SHADE),
    gsDPSetCombineLERP(0, 0, 0, SHADE,  0, 0, 0, ENVIRONMENT,  0, 0, 0, SHADE,  0, 0, 0, ENVIRONMENT),
    0,0
};

//D:80044C00
Gfx DL_LUT_BILLBOARD[] = {
    //Transparent 1Cycle to  BillBoard 1Cycle
    gsDPSetRenderMode(G_RM_AA_ZB_XLU_SURF, G_RM_AA_ZB_XLU_SURF2), gsDPSetRenderMode(G_RM_AA_ZB_TEX_EDGE, G_RM_AA_ZB_TEX_EDGE2),
    //Transparent Surface to Billboard
    gsDPSetRenderMode(G_RM_PASS, G_RM_AA_ZB_XLU_SURF2), gsDPSetRenderMode(G_RM_PASS, G_RM_AA_ZB_TEX_EDGE2),
    0x0,0x0
};

//D:80044C28
//water
Gfx DL_LUT_WATER[] = {
    0xB900031D, 0x00552078, 0xB900031D, 0x00502078,
    0xB900031D, 0x0C192078, 0xB900031D, 0x0C182078,
    /*
    //1 Cycle Opa to Particle
    gsDPSetRenderMode(RM_AA_ZB_OPA_SURF, RM_AA_ZB_OPA_SURF2), gDPSetRenderMode(G_RM_AA_ZB_PCL_SURF, G_RM_AA_ZB_PCL_SURF2),
    //2 cycle Opa to Particle
    gsDPSetRenderMode(G_RM_PASS, G_RM_AA_ZB_OPA_SURF2), gDPSetRenderMode(G_RM_PASS, G_RM_AA_ZB_PCL_SURF2),
    */
    0x0, 0
};

//D:80044C50
Gfx DL_LUT_CLOUD[] = {
    //Transparent to Cloud (Saves AA - Stops Jaggies from appearing behind BillBoard)
    gsDPSetRenderMode(G_RM_PASS, G_RM_AA_ZB_XLU_SURF2), gsDPSetRenderMode(G_RM_PASS, G_RM_ZB_CLD_SURF2),
    0,0
};

//D:80044C68
//(Wallet Bond - Main Menu)
Gfx DL_LUT_WALLETBOND[] = {
    gsDPSetCycleType(G_CYC_1CYCLE),
    gsDPSetCycleType(G_CYC_2CYCLE), //1Cycle --> 2Cycle
    0xB900031D, 0x00502048,
    0xB900031D, 0x08D02048,
    gsDPSetCombineLERP(TEXEL0, 0, SHADE, 0,  0, 0, 0, SHADE,  TEXEL0, 0, SHADE, 0,  0, 0, 0, SHADE),
    gsDPSetCombineLERP(TEXEL0, 0, SHADE, 0,  0, 0, 0, SHADE,  0, 0, 0, COMBINED,  0, 0, 0, COMBINED),
    /*
    //1 Cycle particle Surface to 2 Cycle colour + 1-a*Fog ???
    gsDPSetCycleType(G_CYC_2CYCLE),
    gsDPSetRenderMode(G_RM_AA_PCL_SURF, G_RM_AA_PCL_SURF2), gDPSetRenderMode(AA_EN | IM_RD | CVG_DST_CLAMP | ALPHA_CVG_SEL | ZMODE_OPA | GBL_c1(G_BL_CLR_IN, G_BL_A_SHADE, G_BL_CLR_FOG, G_BL_1MA) | GBL_c2(G_BL_CLR_IN, G_BL_A_IN, G_BL_CLR_MEM, G_BL_1MA)),
    gsDPSetCombineMode(G_CC_MODULATERGBA, G_CC_PASS2), gDPSetCombineMode(G_CC_TRILERP, G_CC_MODULATERGBA2),
    */
    0x0, 0
};

//D:80044CA0
Gfx DL_LUT_FIXFOGALPHA3[] = {
    gsDPSetCombineLERP(TEXEL0, 0, SHADE, 0,  0, 0, 0, SHADE,  TEXEL0, 0, SHADE, 0,  0, 0, 0, SHADE),
    gsDPSetCombineLERP(TEXEL0, 0, SCALE, 0,  0, 0, 0, ENVIRONMENT,  TEXEL0, 0, SCALE, 0,  0, 0, 0, ENVIRONMENT),
    gsDPSetCombineLERP(TEXEL0, 0, SHADE, 0,  TEXEL0, 0, SHADE, 0,  TEXEL0, 0, SHADE, 0,  TEXEL0, 0, SHADE, 0),
    gsDPSetCombineLERP(TEXEL0, 0, SCALE, 0,  TEXEL0, 0, ENVIRONMENT, 0,  TEXEL0, 0, SCALE, 0,  TEXEL0, 0, ENVIRONMENT, 0),
    gsDPSetCombineLERP(TEXEL0, 0, SHADE, 0,  0, 0, 0, SHADE,  TEXEL0, 0, SHADE, 0,  0, 0, 0, SHADE),
    gsDPSetCombineLERP(TEXEL0, 0, SCALE, 0,  0, 0, 0, ENVIRONMENT,  TEXEL0, 0, SCALE, 0,  0, 0, 0, ENVIRONMENT),
    gsDPSetCombineLERP(TEXEL0, 0, SHADE, 0,  TEXEL0, 0, SHADE, 0,  TEXEL0, 0, SHADE, 0,  TEXEL0, 0, SHADE, 0),
    gsDPSetCombineLERP(TEXEL0, 0, SCALE, 0,  TEXEL0, 0, ENVIRONMENT, 0,  TEXEL0, 0, SCALE, 0,  TEXEL0, 0, ENVIRONMENT, 0),
    gsDPSetCombineLERP(0, 0, 0, SHADE,  0, 0, 0, SHADE,  0, 0, 0, SHADE,  0, 0, 0, SHADE),
    gsDPSetCombineLERP(CENTER, 0, SCALE, 0,  0, 0, 0, ENVIRONMENT,  CENTER, 0, SCALE, 0,  0, 0, 0, ENVIRONMENT),
    gsDPSetCombineMode(G_CC_TRILERP, G_CC_MODULATEI2),
    gsDPSetCombineLERP(TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0,  TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0,  COMBINED, 0, SCALE, 0,  0, 0, 0, ENVIRONMENT),
    gsDPSetCombineMode(G_CC_TRILERP, G_CC_MODULATEIA2),
    gsDPSetCombineLERP(TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0,  TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0,  COMBINED, 0, SCALE, 0,  COMBINED, 0, ENVIRONMENT, 0),
    gsDPSetCombineMode(G_CC_TRILERP, G_CC_MODULATEI2),
    gsDPSetCombineLERP(TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0,  TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0,  COMBINED, 0, SCALE, 0,  0, 0, 0, ENVIRONMENT),
    gsDPSetCombineMode(G_CC_TRILERP, G_CC_MODULATEIA2),
    gsDPSetCombineLERP(TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0,  TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0,  COMBINED, 0, SCALE, 0,  COMBINED, 0, ENVIRONMENT, 0),
    gsDPSetCombineLERP(TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0,  1, 0, TEXEL1, 0,  COMBINED, 0, SHADE, 0,  0, 0, 0, SHADE),
    gsDPSetCombineLERP(TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0,  1, 0, TEXEL1, 0,  COMBINED, 0, SCALE, 0,  0, 0, 0, ENVIRONMENT),
    gsDPSetCombineLERP(TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0,  1, 0, TEXEL1, 0,  COMBINED, 0, SHADE, 0,  COMBINED, 0, SHADE, 0),
    gsDPSetCombineLERP(TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0,  1, 0, TEXEL1, 0,  COMBINED, 0, SCALE, 0,  COMBINED, 0, ENVIRONMENT, 0),
    gsDPSetCombineLERP(TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0,  1, 0, TEXEL1, 0,  COMBINED, 0, SHADE, 0,  0, 0, 0, SHADE),
    gsDPSetCombineLERP(TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0,  1, 0, TEXEL1, 0,  COMBINED, 0, SCALE, 0,  0, 0, 0, ENVIRONMENT),
    gsDPSetCombineLERP(TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0,  1, 0, TEXEL1, 0,  COMBINED, 0, SHADE, 0,  COMBINED, 0, SHADE, 0),
    gsDPSetCombineLERP(TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0,  1, 0, TEXEL1, 0,  COMBINED, 0, SCALE, 0,  COMBINED, 0, ENVIRONMENT, 0),
    gsDPSetCombineLERP(0, 0, 0, SHADE,  0, 0, 0, SHADE,  0, 0, 0, COMBINED,  0, 0, 0, COMBINED),
    gsDPSetCombineLERP(CENTER, 0, SCALE, 0,  0, 0, 0, ENVIRONMENT,  0, 0, 0, COMBINED,  0, 0, 0, COMBINED),
    0,0
};

//D:80044D88
Gfx *ptrDynamic_CC_RM_LUT[] = {
    &DL_LUT_UNKNOWN, &DL_LUT_PRIMARY_ADDFOG, &DL_LUT_BILLBOARD, &DL_LUT_WATER, &DL_LUT_CLOUD,
    &DL_LUT_SECONDARY_ADDFOG, &DL_LUT_PRIMARY, &DL_LUT_SECONDARY, &DL_LUT_WALLETBOND, &DL_LUT_FIXFOGALPHA3
};


s32 getMaxNumRooms(void) 
{
    return g_MaxNumRooms;
}


/*
 * Return butflags0 (confirmed u8)
 */
u8 getROOMID_isRendered(s32 roomID)
{
    return g_BgRoomInfo[roomID].room_rendered;
}


/*
 * Return butflags1 (confirmed u8)
 */
u8 getROOMID_isNeighborToRendered(s32 roomID)
{
    return g_BgRoomInfo[roomID].room_neighbor_to_rendered;
}


s32 getIndexOfPORTALID(s32 portalID)
{
    s32 i;

    for(i = 0; g_BgPortals[i].offset_portal != NULL; i++)
    {
        if (portalID == (s32)g_BgPortals[i].offset_portal)
        {
            return i;
        }
    }
    #ifdef DEBUG
    osSyncPrintf("bg: bgPortalIndexFromPtr(): No portal found for %08x ",portalID);
    #endif
    return 0;
}


void roomsHandleStateDebugging(void)
{
    char roomstates[MAXROOMCOUNT + 1];
    s32 roomnum;

    if (debugIsRoomStateDebugEnabled())
    {
        for (roomnum = 1; roomnum < g_MaxNumRooms; roomnum++)
        {
            if (g_BgRoomInfo[roomnum].model_bin_loaded)
            {
                roomstates[roomnum] = (roomnum % 10) + '0';
            }
            else
            {
                roomstates[roomnum] = '.';
            }
        }

        roomstates[roomnum] = '\0';
    }
}


u32 bgDecompress(u8* source, u8 *target)
{
    u8 buffer[0x2100];
    return decompressdata(source, target, buffer);
}


/**
 * Address: 7F0B5FAC
 *
 * Load room's compressed vertex table from the bg file, decompress it
 * into dst, and store the resulting Vtx buffer in room.
 */
s32 bgLoadRoomVtxData(s32 roomnum, u8 *dst, s32 len)
{
    s_room_info *room;
    s32 alignedsize;
    s32 offset;
    s32 result;

    room = &g_BgRoomInfo[roomnum];
    alignedsize = (room->csize_point_index_binary + 0xf) & ~0xf;
    if (len < alignedsize + 0x20) {
        return -1;
    }

    /**
    * pPointTableBin is stored as a segment-0x0f bgdata address.
    * The seemingly unncessary " + ptr_bg_data - ptr_bg_data" likely comes from paired bgdata pointer/offset macros.
    * Adding 0xf1000000 strips the 0x0f000000 segment tag, yielding a file offset.
    */
    offset = (((u8 *)ptr_bgdata_room_fileposition_list[roomnum].pPointTableBin + ptr_bg_data) - ptr_bg_data) + 0xf1000000;
    obLoadBGFileBytesAtOffset(levelinfotable[levelentry_index].bg_seg_filename, dst + (len - alignedsize), offset, alignedsize);
    result = bgDecompress(dst + (len - alignedsize), dst);

    room->vertices = (Vtx *)dst;
    room->usize_point_index_binary = result;

    return result;
}


/**
 * Address: 7F0B609C
 *
 * Load and decompress a room's primary display list data.
 *
 * On success, roominfo->ptr_expanded_mapping_info is set to dst,
 * and roominfo->usize_primary_DL_binary is set to the returned size.
 */
s32 bgLoadRoomPrimaryGdl(s32 roomnum, u8 *dst, s32 allocsize)
{
    s_room_info *roominfo;
    s32 size;
    s32 fileoffset;
    u8 *scratch;
    s32 expanded_size;

    roominfo = &g_BgRoomInfo[roomnum];

    size = roominfo->csize_primary_DL_binary;
    size = (size + 0xf) & ~0xf; // Align to 16 bytes

    /**
     * Check if there is enough room to temporarily place the compressed data
     * at the end of the available buffer. Return -1 if there's not.
     */
    if (allocsize < size + 0x20) {
        return -1;
    }

    // Load the compressed data into the end of the buffer, starting at dst.
    scratch = dst + (allocsize - size);

    fileoffset = (s32)((u8 *)ptr_bgdata_room_fileposition_list[roomnum].pPriMappingBin + ptr_bg_data) - ptr_bg_data;
    fileoffset += 0xf1000000;

    obLoadBGFileBytesAtOffset(levelinfotable[levelentry_index].bg_seg_filename, scratch, fileoffset, size);

    // Decompress from the end-of-buffer location at dst.
    expanded_size = bgDecompress(scratch, dst);

    /**
     * Copy the decompressed GDL back to the end of the buffer as scratch.
     * texLoadFromGdl can then read from scratch and write the final
     * texture-processed GDL/data back to dst.
     */
    scratch = dst + (allocsize - expanded_size);

    texCopyGdls((Gfx *)dst, (Gfx *)scratch, expanded_size);

    clear_light_fixturetable_in_room(roomnum);

    size = texLoadFromGdl((Gfx *)scratch, expanded_size, (Gfx *)dst, NULL);

    if (expanded_size < size) {
        expanded_size = size;
    }

    roominfo->ptr_expanded_mapping_info = dst;
    roominfo->usize_primary_DL_binary = expanded_size;

    // Return the uncompressed data size.
    return expanded_size;
}


/**
 * Address: 7F0B61DC
 *
 * Load and decompress a room's secondary display list data.
 *
 * On success, roominfo->ptr_secondary_expanded_mapping_info is set to dst,
 * and roominfo->usize_secondary_DL_binary is set to the returned size.
 */
s32 bgLoadRoomSecondaryGdl(s32 roomnum, u8 *dst, s32 allocsize)
{
    s_room_info *roominfo;
    s32 size;
    s32 fileoffset;
    u8 *scratch;
    s32 expanded_size;

    roominfo = &g_BgRoomInfo[roomnum];

    size = roominfo->csize_secondary_DL_binary;
    size = (size + 0xf) & ~0xf; // Align to 16 bytes

    /**
     * Check if there is enough room to temporarily place the compressed data
     * at the end of the available buffer. Return -1 if there's not.
     */
    if (allocsize < size + 0x20) {
        return -1;
    }

    // Load the compressed data into the end of the buffer, starting at dst.
    scratch = dst + (allocsize - size);

    fileoffset = (s32)((u8 *)ptr_bgdata_room_fileposition_list[roomnum].pSecMappingBin + ptr_bg_data)  - ptr_bg_data;
    fileoffset += 0xf1000000;

    obLoadBGFileBytesAtOffset(levelinfotable[levelentry_index].bg_seg_filename, scratch, fileoffset, size);

    // Decompress from the end-of-buffer location at dst.
    expanded_size = bgDecompress(scratch, dst);

    /**
     * Copy the decompressed GDL back to the end of the buffer as scratch.
     * texLoadFromGdl can then read from scratch and write the final
     * texture-processed GDL/data back to dst.
     */
    scratch = dst + (allocsize - expanded_size);

    texCopyGdls((Gfx *)dst, (Gfx *)scratch, expanded_size);

    size = texLoadFromGdl((Gfx *)scratch, (Gfx *)expanded_size, (Gfx *)dst, NULL);

    if (expanded_size < size) {
        expanded_size = size;
    }

    roominfo->ptr_secondary_expanded_mapping_info = dst;
    roominfo->usize_secondary_DL_binary = expanded_size;

    // Return the uncompressed data size.
    return expanded_size;
}


s32 bgCheckIfRoomModelNeedsLoad(s32 roomID)
{
    g_BgRoomInfo[roomID].field_35 = 1;
    if (g_BgRoomInfo[roomID].model_bin_loaded == 0)
    {
        bgLoadRoomModelData(roomID);
        return 1;
    }
    return 0;
}


/*
* Allocates memory for room and update its display lists
*
* When a room is first allocated, the game will pick the largest block
* available. It doesn't know the size of the decompressed asset as its
* size is not stored as part of the GZIP format. It will then shrink
* the allocated block to the correct size. The size is cached for the
* next time the room is reloaded.
*
* Address: 7F0B6368
*/
void bgLoadRoomModelData(s32 roomID)
{
    /*
     * Keep this prototype visible here for IDO codegen.
     * Without it, the memaRealloc call below schedules a2 too early.
     */
    s32 memaRealloc(s32 addr, u32 oldsize, u32 newsize);

    s32 allocsize;
    s32 used;
    s32 result;
    u8 *data;

    used = 0;

    // Room ID is out of range?
    if (roomID >= g_MaxNumRooms) goto end;

    // Room is already loaded?
    if (g_BgRoomInfo[roomID].model_bin_loaded) goto end;

    // Get the cached file size. Is zero when the size is not yet known.
    allocsize = g_BgRoomInfo[roomID].cur_room_totalsize;

    if (allocsize > 0)
    {
        // Unknown debug code
        if (get_debug_joy2detailedit_flag())
        {
            allocsize += 0x400;
        }
    }
    else
    {
        // On a first allocation, we'll allocate the largest memory block available.
        allocsize = memaGetLongestFree();
    }

    /**
    * Allocate one contiguous block for vertices and display lists.
    */
    data = memaAlloc(allocsize);

    if (data == NULL) goto end;

    if (g_BgRoomInfo[roomID].csize_point_index_binary)
    {
        result = bgLoadRoomVtxData(roomID, data, allocsize);

        if (result >= 0)
        {
            used = result;
            redarken_lights_in_room(roomID);
        }
    }
    else
    {
        g_BgRoomInfo[roomID].vertices = NULL;
        g_BgRoomInfo[roomID].usize_point_index_binary = 0;
    }

    /**
     * Append the primary display list after the vertex data.
     */
    if (g_BgRoomInfo[roomID].csize_primary_DL_binary)
    {
        result = bgLoadRoomPrimaryGdl(roomID, data + used, allocsize - used);

        if (result >= 0)
        {
            used += result;
        }
    }

    /**
     * Append the secondary display list.
     */
    if (g_BgRoomInfo[roomID].csize_secondary_DL_binary)
    {
        result = bgLoadRoomSecondaryGdl(roomID, data + used, allocsize - used);

        if (result > 0)
        {
            used += result;
        }
    }
    else
    {
        g_BgRoomInfo[roomID].ptr_secondary_expanded_mapping_info = NULL;
    }

    g_BgRoomInfo[roomID].cur_room_totalsize = ((used + 0x20) & ~0xf);
    g_BgRoomInfo[roomID].model_bin_loaded = 1;

    // If wasted space is detected, shrink allocated memory block.
    if (allocsize != ((used + 0x20) & ~0xf))
    {
        memaRealloc((s32)data, allocsize, ((used + 0x20) & ~0xf));
    }

    // Same branches, only the LUT parameter changes
    if (g_FogSkyIsEnabled)
    {
        bgApplyDynamicCCRMLUT(
            g_BgRoomInfo[roomID].ptr_expanded_mapping_info,
            (Gfx *)((u8 *)g_BgRoomInfo[roomID].ptr_expanded_mapping_info + g_BgRoomInfo[roomID].usize_primary_DL_binary),
            1);

        if (g_BgRoomInfo[roomID].ptr_secondary_expanded_mapping_info)
        {
            bgApplyDynamicCCRMLUT(
                g_BgRoomInfo[roomID].ptr_secondary_expanded_mapping_info,
                (Gfx *)((u8 *)g_BgRoomInfo[roomID].ptr_secondary_expanded_mapping_info + g_BgRoomInfo[roomID].usize_secondary_DL_binary),
                5);
        }
    }
    else
    {
        bgApplyDynamicCCRMLUT(
            g_BgRoomInfo[roomID].ptr_expanded_mapping_info,
            (Gfx *)((u8 *)g_BgRoomInfo[roomID].ptr_expanded_mapping_info + g_BgRoomInfo[roomID].usize_primary_DL_binary),
            6);

        if (g_BgRoomInfo[roomID].ptr_secondary_expanded_mapping_info)
        {
            bgApplyDynamicCCRMLUT(
                g_BgRoomInfo[roomID].ptr_secondary_expanded_mapping_info,
                (Gfx *)((u8 *)g_BgRoomInfo[roomID].ptr_secondary_expanded_mapping_info + g_BgRoomInfo[roomID].usize_secondary_DL_binary),
                7);
        }
    }

    bgBuildRoomVtxBounds(roomID);
    roomsHandleStateDebugging();

end:;

}


/**
 * Given a room, frees all dynamically allocated data for that room and marks the room as unloaded.
 */
void delete_room_data(s32 roomID)
{
    s_room_info *room = &g_BgRoomInfo[roomID];
    s32 size;
    s32 size2;
    Vtx *pointindex;

    if (room->vtx_batch_bounds != NULL) {
        memaFree(
            room->vtx_batch_bounds,
            ((room->num_vtx_batch_bounds * sizeof(RoomVtxBatchBounds)) + 0xf) & ~0xf);

        room->vtx_batch_bounds = NULL;
    }

    if (room->cur_room_totalsize > 0) {
        size = room->cur_room_totalsize;
        pointindex = room->vertices;

        if (pointindex != NULL)
        {
            size2 = room->cur_room_totalsize;
            memaFree(pointindex, size2);
            room->vertices = NULL;
        }
        else
        {
            memaFree(room->ptr_expanded_mapping_info, size);
            room->vertices = NULL;
        }

        room->ptr_expanded_mapping_info = NULL;
        room->ptr_secondary_expanded_mapping_info = NULL;
    }

    room->model_bin_loaded = 0;
    roomsHandleStateDebugging();
}


/**
 * Immediately unload all loaded rooms.
 * Used for stage cleanup.
 */
void unload_rooms(void)
{
    s32 i;

    for(i = 1; i < g_MaxNumRooms; i++)
    {
        if (g_BgRoomInfo[i].model_bin_loaded)
        {
            delete_room_data(i);
        }
    }
}


/**
 * Address: 7F0B66E8
 *
 * Ages loaded rooms that are no longer marked active, then unloads them
 * once their unload delay expires.
 */
void bgRoomsTickUnload(void)
{
    s32 i;

    for(i = 1; i < g_MaxNumRooms; i++)
    {
        if (g_BgRoomInfo[i].field_35 == 0)
        {
            if (g_BgRoomInfo[i].model_bin_loaded == 4)
            {
                delete_room_data(i);
            }
            else if (g_BgRoomInfo[i].model_bin_loaded != 0)
            {
                g_BgRoomInfo[i].model_bin_loaded = g_BgRoomInfo[i].model_bin_loaded + 1;
            }
        }
    }
}


/**
 * Address 7F0B677C
 *
 * Render a room's primary (solid) geometry.
 * Ensures the room's bg data is loaded if budget allows, then appends its display list.
 * Also resets the age of rendered rooms so bgRoomsTickUnload won't unload it.
*/
Gfx *bgRenderRoomPrimary(Gfx *gdl, s32 room_index)
{
    if (room_index >= g_MaxNumRooms)
    {
        return gdl;
    }

    if ((D_8004485C != 0) || (D_80044858 == (room_index % 10)))
    {
        if (g_BgRoomInfo[room_index].model_bin_loaded == 0)
        {
            if (g_RoomLoadBudget > 0)
            {
                g_RoomLoadBudget--;
                bgLoadRoomModelData(room_index);
            }
        }

        if (g_BgRoomInfo[room_index].model_bin_loaded == 0)
        {
            return gdl;
        }
        else
        {
            gdl = applyRoomMatrixToDisplayList(gdl, room_index);

            gSPSegment(gdl++, SPSEGMENT_BG_VTX, OS_K0_TO_PHYSICAL(g_BgRoomInfo[room_index].vertices));
            gSPDisplayList(gdl++, OS_K0_TO_PHYSICAL(g_BgRoomInfo[room_index].ptr_expanded_mapping_info));

            // Set the room's state to "loaded"
            g_BgRoomInfo[room_index].model_bin_loaded = 1;
        }
    }

    return gdl;
}


/**
 * Address 7F0B6898
 *
 * Render a room's secondary (transparent) geometry.
*/
Gfx *bgRenderRoomSecondary(Gfx *gdl, s32 room_index)
{
    if (room_index >= g_MaxNumRooms)
    {
        return gdl;
    }

    // Return if the room has no secondary geometry.
    if (g_BgRoomInfo[room_index].ptr_secondary_expanded_mapping_info == 0)
    {
        return gdl;
    }
    else if ((D_8004485C != 0) || (D_80044858 == (room_index % 10)))
    {
        if (g_BgRoomInfo[room_index].model_bin_loaded != 0)
        {
            gdl = applyRoomMatrixToDisplayList(gdl, room_index);

            gSPSegment(gdl++, SPSEGMENT_BG_VTX, OS_K0_TO_PHYSICAL(g_BgRoomInfo[room_index].vertices));
            gSPDisplayList(gdl++, OS_K0_TO_PHYSICAL(g_BgRoomInfo[room_index].ptr_secondary_expanded_mapping_info));

            // Set the room's state to "loaded"
            g_BgRoomInfo[room_index].model_bin_loaded = 1;
        }
        else
        {
            bgLoadRoomModelData(room_index);
        }
    }

    return gdl;
}


/*
* Build world space bounds for each vertex batch loaded by the room's display list.
* Address: 7F0B6994
*/
#define ALIGN16(val) (((val) + 0xF) & ~0xF)
#define	SEGMENT_OFFSET(a)	((unsigned int)(a) & 0x00ffffff)

void bgBuildRoomVtxBounds(s32 roomID)
{
    s32 cmdindex;
    Gfx *gdl;
    Vtx *vertices;
    RoomVtxBatchBounds *points;
    s32 numpoints;
    RoomVtxBatchBounds *point;
    s32 i;
    s32 numvertices;
    Vtx *vtx;

    // Check if a cached bounding box is already present for this room
    if (g_BgRoomInfo[roomID].vtx_batch_bounds != NULL)
    {
        return;
    }

    gdl = g_BgRoomInfo[roomID].ptr_expanded_mapping_info;

    vertices = g_BgRoomInfo[roomID].vertices;
    cmdindex = 0;
    numpoints = 0;

    while (gdl[cmdindex].dma.cmd != G_ENDDL)
    {
        if (gdl[cmdindex].dma.cmd == G_VTX) 
        {
            numpoints++;
        }
        cmdindex++;
    }

    points = memaAlloc(ALIGN16(numpoints * sizeof(RoomVtxBatchBounds)));

    if (ALIGN16(numpoints * sizeof(RoomVtxBatchBounds))) {}

    if (points == NULL)
    {
        return;
    }

    g_BgRoomInfo[roomID].vtx_batch_bounds = points;
    g_BgRoomInfo[roomID].num_vtx_batch_bounds = numpoints;

    numpoints = 0;
    cmdindex = 0;

    while (gdl[cmdindex].dma.cmd != G_ENDDL)
    {
        if (gdl[cmdindex].dma.cmd == G_VTX)
        {
            point = &points[numpoints];

            points[numpoints].gdlindex = cmdindex;

            for (i = 0; i < 3; i++) 
            {
                points[numpoints].min[i] = 0x7fff;
                points[numpoints].max[i] = -0x8000;
            }

            numvertices = ((gdl[cmdindex].dma.par >> 4) & 0xf) + 1;

            vtx = (Vtx *)(SEGMENT_OFFSET(gdl[cmdindex].dma.addr) + (u32)vertices);

            for (i = 0; i < numvertices; i++)
            {
                if (points[numpoints].xmin > vtx[i].v.ob[0])
                {
                    points[numpoints].xmin = vtx[i].v.ob[0];
                }

                if (points[numpoints].ymin > vtx[i].v.ob[1])
                {
                    points[numpoints].ymin = vtx[i].v.ob[1];
                }

                if (points[numpoints].zmin > vtx[i].v.ob[2])
                {
                    points[numpoints].zmin = vtx[i].v.ob[2];
                }

                if (points[numpoints].xmax < vtx[i].v.ob[0])
                {
                    points[numpoints].xmax = vtx[i].v.ob[0];
                }

                if (points[numpoints].ymax < vtx[i].v.ob[1])
                {
                    points[numpoints].ymax = vtx[i].v.ob[1];
                }

                if (points[numpoints].zmax < vtx[i].v.ob[2])
                {
                    points[numpoints].zmax = vtx[i].v.ob[2];
                }
            }


            if (points[numpoints].xmin == points[numpoints].xmax)
            {
                points[numpoints].xmax++;
            }

            if (points[numpoints].ymin == points[numpoints].ymax)
            {
                points[numpoints].ymax++;
            }

            if (points[numpoints].zmin == points[numpoints].zmax)
            {
                points[numpoints].zmax++;
            }


            points[numpoints].xmin += (s32)ptr_bgdata_room_fileposition_list[roomID].pos.x;
            points[numpoints].ymin += (s32)ptr_bgdata_room_fileposition_list[roomID].pos.y;
            points[numpoints].zmin += (s32)ptr_bgdata_room_fileposition_list[roomID].pos.z;

            points[numpoints].xmax += (s32)ptr_bgdata_room_fileposition_list[roomID].pos.x;
            points[numpoints].ymax += (s32)ptr_bgdata_room_fileposition_list[roomID].pos.y;
            points[numpoints].zmax += (s32)ptr_bgdata_room_fileposition_list[roomID].pos.z;
            numpoints++;
        }
        cmdindex++;
    }
}

#undef ALIGN16
#undef SEGMENT_OFFSET


/**
 * Slab method ray vs axis aligned bounding box test, written to be division-free.
 */
bool bgTestRayIntersectsBbox(coord3d *origin, coord3d *dir, s32 *bbox_min, s32 *bbox_max)
{
    coord3d bbox_min_f;
    coord3d bbox_max_f;
    u32 stack[4];
    f32 f0;
    f32 f0_2;
    f32 f2;
    f32 f2_2;
    f32 f6;
    f32 f10;
    f32 sp34;
    f32 sp30;
    f32 f16;
    f32 f18;
    f32 f18_2;
    f32 sp20;
    f32 f12;
    f32 f12_2;
    f32 f14;
    f32 f14_2;

    bbox_min_f.f[0] = bbox_min[0];
    bbox_min_f.f[1] = bbox_min[1];
    bbox_min_f.f[2] = bbox_min[2];

    bbox_max_f.f[0] = bbox_max[0];
    bbox_max_f.f[1] = bbox_max[1];
    bbox_max_f.f[2] = bbox_max[2];

    // x
    f18 = dir->x;
    f16 = bbox_max_f.x - origin->x;
    f14 = bbox_min_f.x - origin->x;

    if (f18 < 0.0f)
    {
        f18 = -f18;
        f14 = -f14;
        f16 = -f16;
    }

    if (f14 < 0.0f && f16 < 0.0f)
    {
        return FALSE;
    }

    if (f16 < f14)
    {
        f32 tmp = f14;
        f14 = f16;
        f16 = tmp;
    }

    // y
    f12 = dir->y;
    f2 = bbox_max_f.y - origin->y;
    f0 = bbox_min_f.y - origin->y;

    if (f12 < 0.0f)
    {
        f12 = -f12;
        f0 = -f0;
        f2 = -f2;
    }

    if (f0 < 0.0f && f2 < 0.0f)
    {
        return FALSE;
    }

    if (f2 < f0)
    {
        sp20 = f0;
        f0 = f2;
        f2 = sp20;
    }

    f6 = f14 * f12;
    f10 = f0 * f18;

    if (f10 < f6)
    {
        if (f2 * f18 < f6)
        {
            return FALSE;
        }

        sp34 = f14;
        sp30 = f18;
    }
    else
    {
        if (f16 * f12 < f10)
        {
            return FALSE;
        }

        sp34 = f0;
        sp30 = f12;
    }

    if (f16 * f12 < f2 * f18)
    {
        f0_2 = f16;
        f14_2 = f18;
    }
    else
    {
        f0_2 = f2;
        f14_2 = f12;
    }

    // z
    f2_2 = dir->z;
    f12_2 = bbox_max_f.z - origin->z;
    f18_2 = bbox_min_f.z - origin->z;

    if (f2_2 < 0.0f)
    {
        f2_2 = -f2_2;
        f18_2 = -f18_2;
        f12_2 = -f12_2;
    }

    if (f18_2 < 0.0f && f12_2 < 0.0f)
    {
        return FALSE;
    }

    if (f12_2 < f18_2)
    {
        f32 tmp = f18_2;
        f18_2 = f12_2;
        f12_2 = tmp;
    }

    if (sp34 * f2_2 < f18_2 * sp30)
    {
        if (f0_2 * f2_2 < f18_2 * f14_2)
        {
            return FALSE;
        }
    }
    else
    {
        if (f12_2 * sp30 < sp34 * f2_2)
        {
            return FALSE;
        }
    }

    return TRUE;
}


bool bgTestRayIntersectionInRoom(coord3d *from, coord3d *to, coord3d *dir, RoomVtxBatchBounds *point, s32 roomnum, struct HitThing *hitthing)
{
    Vertex *vtxbase;
    Vertex *v;

    union {
        s32 word;
        Vertex *vertices;
        s_room_info *roominfo;
    } temp;

    s32 op;
    s32 found;
    Gfx *gdl;
    HitThing hitbuf;
    Gfx *tcmd;
    s32 bestScore;
    s32 idx[3];
    s32 vtxoff;
    BoundVec bboxMin;
    BoundVec bboxMax;
    s32 *p;
    s32 i;
    s32 s2;
    s32 texnum;
    s32 dx;
    s32 dy;
    s32 dz;
    s32 dist;
    s32 idx2[3];
    s32 score;
    BoundVec bboxMin2;
    BoundVec bboxMax2;

    struct {
        s32 unused;
        s_room_info *roominfo;
        s32 padding[6];
    } local;

    gdl = (Gfx *) g_BgRoomInfo[roomnum].ptr_expanded_mapping_info;
    gdl = &gdl[point->gdlindex];
    vtxoff = ((u8 *) gdl)[1] & 0xf;
    temp.vertices = (Vertex *)g_BgRoomInfo[roomnum].vertices;
    vtxbase = (Vertex *)((s32)temp.vertices + (((u32 *)gdl)[1] & 0x00ffffff));
    temp.roominfo = &g_BgRoomInfo[roomnum];
    bestScore = 0x7FFFFFFF;
    found = 0;
    gdl++;
    op = *((s8 *) gdl);

    if ((op != G_VTX) && (op != ((s8) G_ENDDL)))
    {
        local.roominfo = temp.roominfo;

        do
        {
            if (op == ((s8) G_TRI1))
            {
                bboxMin = D_80044868;
                bboxMax = D_80044874;
                score = dist;
                idx[0] = (((u8 *) gdl)[5] / 10) - vtxoff;
                idx[1] = (((u8 *) gdl)[6] / 10) - vtxoff;
                idx[2] = (((u8 *) gdl)[7] / 10) - vtxoff;
                i = 0;
                
                do
                {
                    v = vtxbase;
                    v += idx[i];

                    if (v->coord.x < bboxMin.x)
                    {
                        bboxMin.x = v->coord.x;
                    }
                    if (bboxMax.x < v->coord.x)
                    {
                        bboxMax.x = v->coord.x;
                    }
                    if (v->coord.y < bboxMin.y)
                    {
                        bboxMin.y = v->coord.y;
                    }
                    if (bboxMax.y < v->coord.y)
                    {
                        bboxMax.y = v->coord.y;
                    }

                    if (roomnum);

                    if (v->coord.z < bboxMin.z)
                    {
                        bboxMin.z = v->coord.z;
                    }
                    if (bboxMax.z < v->coord.z)
                    {
                        bboxMax.z = v->coord.z;
                    }
                    i++;
                } 
                while (i < 3);

                bboxMin.x += (s32) ptr_bgdata_room_fileposition_list[roomnum].pos.x;
                bboxMin.y += (s32) ptr_bgdata_room_fileposition_list[roomnum].pos.y;
                bboxMin.z += (s32) ptr_bgdata_room_fileposition_list[roomnum].pos.z;
                bboxMax.x += (s32) ptr_bgdata_room_fileposition_list[roomnum].pos.x;
                bboxMax.y += (s32) ptr_bgdata_room_fileposition_list[roomnum].pos.y;
                bboxMax.z += (s32) ptr_bgdata_room_fileposition_list[roomnum].pos.z;

                if (bgTestRayIntersectsBbox(from, dir, (s32 *) (&bboxMin), (s32 *) (&bboxMax)))
                {
                    if (intersectRayTriangle((Vertex *)((s32)vtxbase - (0 - (idx[0] << 4))), (Vertex *)((s32)vtxbase - (0 - (idx[1] << 4))), (Vertex *)((s32)vtxbase - (0 - (idx[2] << 4))), (coord3d *) (((roomnum * 24) + ((s32) ptr_bgdata_room_fileposition_list)) + 12), from, to, dir, &hitbuf))
                    {
                        tcmd = gdl;
                        dx = ((s32) hitbuf.hitpos.x) - ((s32) from->x);
                        dy = ((s32) hitbuf.hitpos.y) - ((s32) from->y);
                        dz = ((s32) hitbuf.hitpos.z) - ((s32) from->z);
                        dist = ((dx * dx) + (dy * dy)) + (dz * dz);
                        score = dist;
                        found = 1;

                        if (((*((u8 *) gdl)) != G_SETTIMG) && (dist || vtxbase || 1) && (((Gfx *) local.roominfo->ptr_expanded_mapping_info) < gdl))
                        {
                            do
                            {
                                tcmd--;
                                if ((*((u8 *) tcmd)) == G_SETTIMG)
                                {
                                    break;
                                }
                            }
                            while (((Gfx *) local.roominfo->ptr_expanded_mapping_info) < tcmd);
                        }

                        if (tcmd == ((Gfx *) local.roominfo->ptr_expanded_mapping_info))
                        {
                            texnum = -1;
                        }
                        else
                        {
                            temp.word = ((u32 *) tcmd)[1] - 8;
                            texnum = *((u16 *) (temp.word | 0x80000000));
                        }

                        if (check_if_imageID_is_light(texnum))
                        {
                            score = dist - 4;
                        }

                        if (dist);

                        // Texture 0x4FD is used for the light shafts that come through windows in Archives.
                        if ((score < bestScore) && (texnum != 0x4FD))
                        {
                            bestScore = score;
                            hitthing->hitpos.x = hitbuf.hitpos.x;
                            hitthing->hitpos.y = hitbuf.hitpos.y;
                            hitthing->hitpos.z = hitbuf.hitpos.z;
                            hitthing->normal.x = hitbuf.normal.x;
                            hitthing->normal.y = hitbuf.normal.y;
                            hitthing->normal.z = hitbuf.normal.z;
                            hitthing->vtx0 = &vtxbase[idx[0]];
                            hitthing->vtx1 = &vtxbase[idx[1]];
                            hitthing->vtx2 = &vtxbase[idx[2]];
                            hitthing->texturenum = texnum;
                            hitthing->tricmd = gdl;
                            hitthing->unk28 = 0;
                        }
                    }
                }
            }
            else
            {
                if (op == ((s8) G_TRI4))
                {
                    // Keep this line as-is for matching.
                    s2 = 0; do
                    {
                        bboxMin2 = D_80044880;
                        bboxMax2 = D_8004488C;

                        if (s2 == 0)
                        {
                            idx2[0] = (((u32 *) gdl)[1] & 0xf) - vtxoff;
                            idx2[1] = (((u32) ((u8 *) gdl)[7]) >> 4) - vtxoff;
                            idx2[2] = (((u32 *) gdl)[0] & 0xf) - vtxoff;
                        }
                        else if (s2 == 1)
                        {
                            idx2[0] = (((u8 *) gdl)[6] & 0xf) - vtxoff;
                            idx2[1] = (((u32) ((u16 *) gdl)[3]) >> 12) - vtxoff;
                            idx2[2] = (((u32) ((u8 *) gdl)[3]) >> 4) - vtxoff;
                        }
                        else if (s2 == 2)
                        {
                            idx2[0] = (((u16 *) gdl)[2] & 0xf) - vtxoff;
                            idx2[1] = (((u32) ((u8 *) gdl)[5]) >> 4) - vtxoff;
                            idx2[2] = (((u8 *) gdl)[2] & 0xf) - vtxoff;
                        }
                        else
                        {
                            idx2[0] = (((u8 *) gdl)[4] & 0xf) - vtxoff;
                            idx2[1] = (((u32 *) gdl)[1] >> 28) - vtxoff;
                            idx2[2] = (((u32) ((u16 *) gdl)[1]) >> 12) - vtxoff;
                        }

                        i = 0;

                        do
                        {
                            v = vtxbase;
                            v += idx2[i];
                            if (v->coord.x < bboxMin2.x)
                            {
                                bboxMin2.x = v->coord.x;
                            }
                            if (bboxMax2.x < v->coord.x)
                            {
                                bboxMax2.x = v->coord.x;
                            }
                            if (v->coord.y < bboxMin2.y)
                            {
                                bboxMin2.y = v->coord.y;
                            }
                            if (bboxMax2.y < v->coord.y)
                            {
                                bboxMax2.y = v->coord.y;
                            }
                            if (v->coord.z < bboxMin2.z)
                            {
                                bboxMin2.z = v->coord.z;
                            }
                            if (bboxMax2.z < v->coord.z)
                            {
                                bboxMax2.z = v->coord.z;
                            }
                            i++;
                        } 
                        while (i < 3);
                        
                        bboxMin2.x += (s32) ptr_bgdata_room_fileposition_list[roomnum].pos.x;
                        bboxMin2.y += (s32) ptr_bgdata_room_fileposition_list[roomnum].pos.y;
                        bboxMin2.z += (s32) ptr_bgdata_room_fileposition_list[roomnum].pos.z;
                        bboxMax2.x += (s32) ptr_bgdata_room_fileposition_list[roomnum].pos.x;
                        bboxMax2.y += (s32) ptr_bgdata_room_fileposition_list[roomnum].pos.y;
                        bboxMax2.z += (s32) ptr_bgdata_room_fileposition_list[roomnum].pos.z;

                        if (bgTestRayIntersectsBbox(from, dir, (s32 *) (&bboxMin2), (s32 *) (&bboxMax2)))
                        {
                            if (intersectRayTriangle((Vertex *)((s32)vtxbase - (0 - (idx2[0] << 4))), (Vertex *)((s32)vtxbase - (0 - (idx2[1] << 4))), (Vertex *)((s32)vtxbase - (0 - (idx2[2] << 4))), (coord3d *) (((roomnum * 24) + ((s32) ptr_bgdata_room_fileposition_list)) + 12), from, to, dir, &hitbuf))
                            {
                                tcmd = gdl;
                                dx = ((s32) hitbuf.hitpos.x) - ((s32) from->x);
                                dy = ((s32) hitbuf.hitpos.y) - ((s32) from->y);
                                dz = ((s32) hitbuf.hitpos.z) - ((s32) from->z);
                                dist = ((dx * dx) + (dy * dy)) + (dz * dz);
                                score = dist;
                                found = 1;

                                if (((*((u8 *) gdl)) != G_SETTIMG) && (dist || vtxbase || 1) && (((Gfx *) local.roominfo->ptr_expanded_mapping_info) < gdl))
                                {
                                    do
                                    {
                                        tcmd--;
                                        if ((*((u8 *) tcmd)) == G_SETTIMG)
                                        {
                                            break;
                                        }
                                    }
                                    while (((Gfx *) local.roominfo->ptr_expanded_mapping_info) < tcmd);
                                }
                                
                                if (tcmd == ((Gfx *) local.roominfo->ptr_expanded_mapping_info))
                                {
                                    texnum = -1;
                                }
                                else
                                {
                                    temp.word = ((u32 *) tcmd)[1] - 8;
                                    texnum = *((u16 *) (temp.word | 0x80000000));
                                }

                                if (check_if_imageID_is_light(texnum))
                                {
                                    score = dist - 4;
                                }

                                if ((score || dist));

                                if ((score < bestScore) && (texnum != 0x4FD))
                                {
                                    bestScore = score;
                                    hitthing->hitpos.x = hitbuf.hitpos.x;
                                    hitthing->hitpos.y = hitbuf.hitpos.y;
                                    hitthing->hitpos.z = hitbuf.hitpos.z;
                                    hitthing->normal.x = hitbuf.normal.x;
                                    hitthing->normal.y = hitbuf.normal.y;
                                    hitthing->normal.z = hitbuf.normal.z;
                                    hitthing->vtx0 = &vtxbase[idx2[0]];
                                    hitthing->vtx1 = &vtxbase[idx2[1]];
                                    hitthing->vtx2 = &vtxbase[idx2[2]];
                                    hitthing->texturenum = texnum;
                                    hitthing->tricmd = gdl;
                                    hitthing->unk28 = s2 + 1;
                                }
                            }
                        }
                        s2++;
                    }
                    while (s2 != 4);
                }
            }

            gdl++;
            op = *((s8 *) gdl);
        }
        while ((op != G_VTX) && (op != ((s8) G_ENDDL)));
    }

    return found;
}


/**
 * In order to make bgTestBulletHitBackground match, 4 bytes had to dropped from the regular HitThing struct.
 * There might be an alternate way of matching this that doesn't require the sub-struct.
 */
struct HitThingSub {
    coord3d hitpos;     // 0x00
    coord3d normal;     // 0x0c

    Vertex *vtx0;       // 0x18
    Vertex *vtx1;       // 0x1c
    Vertex *vtx2;       // 0x20

    Gfx *tricmd;        // 0x24 - display-list command associated with hit triangle

    s16 unk28;          // 0x28
    s16 texturenum;     // 0x2a
};


bool bgTestBulletHitBackground(coord3d *from, coord3d *to, s32 roomnum, struct HitThing *hit)
{
    RoomVtxBatchBounds *point;
    s16 numpoints;
    coord3d fromscaled;
    coord3d toscaled;
    coord3d dir;
    s32 i;
    struct HitThingSub tmp;
    f32 scale;
    s32 bestdist;
    s32 dist;
    bool found;
    s32 score;
    s32 dx;
    s32 dy;
    s32 dz;

    found = FALSE;
    bestdist = 0x7fffffff;

    scale = room_data_float1;
    fromscaled.x = from->x * scale;
    fromscaled.y = from->y * scale;
    fromscaled.z = from->z * scale;
    toscaled.x = to->x * scale;
    toscaled.y = to->y * scale;
    toscaled.z = to->z * scale;
    dir.x = toscaled.x - fromscaled.x;
    dir.y = toscaled.y - fromscaled.y;
    dir.z = toscaled.z - fromscaled.z;
    point = g_BgRoomInfo[roomnum].vtx_batch_bounds;
    point = g_BgRoomInfo[roomnum].vtx_batch_bounds;

    if (point == NULL)
    {
        return FALSE;
    }

    numpoints = g_BgRoomInfo[roomnum].num_vtx_batch_bounds;

    for (i = 0; i < numpoints; i++) 
    {
        if (!bgTestRayIntersectsBbox(&fromscaled, &dir, &point[i].xmin, &point[i].xmax)) 
        {
            continue;
        }

        if (!bgTestRayIntersectionInRoom(&fromscaled, &toscaled, &dir, &point[i], roomnum, (struct HitThing *)&tmp)) 
        {
            continue;
        }

        dx = tmp.hitpos.x - fromscaled.x;
        dy = tmp.hitpos.y - fromscaled.y;
        dz = tmp.hitpos.z - fromscaled.z;
        dist = score = dx * dx + dy * dy + dz * dz;
        found = TRUE;

        if (check_if_imageID_is_light(tmp.texturenum)) 
        {
            score = dist - 4;
        }

        if (score < bestdist) 
        {
            bestdist = score;
            hit->hitpos.x = tmp.hitpos.x;
            hit->hitpos.y = tmp.hitpos.y;
            hit->hitpos.z = tmp.hitpos.z;
            hit->normal.x = tmp.normal.x;
            hit->normal.y = tmp.normal.y;
            hit->normal.z = tmp.normal.z;
            hit->vtx0 = tmp.vtx0;
            hit->vtx1 = tmp.vtx1;
            hit->vtx2 = tmp.vtx2;
            hit->texturenum = tmp.texturenum;
            hit->tricmd = tmp.tricmd;
            hit->unk28 = tmp.unk28;
        }
    }

    if (found) 
    {
        point = (RoomVtxBatchBounds *)hit->tricmd;

        if (((u8 *)((Gfx*)point))[0] != G_SETTILE) 
        {
            while ((Gfx *)g_BgRoomInfo[roomnum].ptr_expanded_mapping_info < ((Gfx*)point)) 
            {
                point = (RoomVtxBatchBounds *)((Gfx *)point - 1);
                if (((u8 *)((Gfx*)point))[0] == G_SETTILE) 
                {
                    break;
                }
            }
        }

        if (((Gfx*)point) == g_BgRoomInfo[roomnum].ptr_expanded_mapping_info) 
        {
            hit->tileformat = -1;
            hit->tilesize = -1;
        } 
        else 
        {
            hit->tileformat = ((u32)((u8 *)((Gfx*)point))[1]) >> 5;
            hit->tilesize = (((Gfx*)point)->words.w0 << 11) >> 30;
        }
    }

    return found;
}


/**
 * Address: 7F0B7D94
 */
void bgResetPortalQueue(void)
{
    g_BgPortalQueueWriteIndex = 0;
    g_BgPortalQueueReadIndex = 0;
}


/**
 * Address: 7F0B7DA8
 */
u8 bgIncrementRoomPortalVisitCount(s32 roomnum)
{
    s_room_info* room_info;
    u8 tmp;
    u8 count;
    u8 out;

    room_info = &g_BgRoomInfo[roomnum];
    count = room_info->portal_visit_count;
    out = count;

    if ((s32) count < 0xFF)
    {
        tmp = count + 1;
        room_info->portal_visit_count = tmp;
        out = tmp & 0xFF;
    }

    return out;
}


/**
 * Address: 7F0B7DE4
 */
#ifdef VERSION_EU
void bgQueuePortalTraversal(s32 arg0, s32 arg1, s32 portalnum, f32 *arg4)
{
    bg_queued_portal_entry *entry;
    entry = &g_BgPortalQueue[g_BgPortalQueueWriteIndex];
    if (portalnum >= 2)
    {
        if (bgIncrementRoomPortalVisitCount((g_BgPortals[arg1].connectedRoom2 ^ g_BgPortals[arg1].connectedRoom1) ^ arg0) >= 9)
        {
            return;
        }
    }
    entry->arg0 = arg0;
    entry->roomnum = arg1;
    entry->portalnum = portalnum;
    entry->sp4[0] = arg4[0];
    entry->sp4[1] = arg4[1];
    entry->sp4[2] = arg4[2];
    entry->sp4[3] = arg4[3];
    g_BgPortalQueueWriteIndex++;
    if (g_BgPortalQueueWriteIndex == BG_PORTAL_QUEUE_LEN)
    {
        g_BgPortalQueueWriteIndex = 0;
    }
    if (g_BgPortalQueueWriteIndex == g_BgPortalQueueReadIndex)
    {
        g_BgPortalQueueWriteIndex--;
    }
}
#else
void bgQueuePortalTraversal(s32 arg0, s32 arg1, s32 portalnum, s32 depth, f32 *arg4)
{
    bg_queued_portal_entry *entry;
    entry = &g_BgPortalQueue[g_BgPortalQueueWriteIndex];
    if (depth >= 2)
    {
        if (bgIncrementRoomPortalVisitCount((g_BgPortals[portalnum].connectedRoom2 ^ g_BgPortals[portalnum].connectedRoom1) ^ arg1) >= 9)
        {
            return;
        }
    }
    entry->arg0 = arg0;
    entry->roomnum = arg1;
    entry->portalnum = portalnum;
    entry->arg3 = depth;
    entry->sp10[0] = arg4[0];
    entry->sp10[1] = arg4[1];
    entry->sp10[2] = arg4[2];
    entry->sp10[3] = arg4[3];
    g_BgPortalQueueWriteIndex++;
    if (g_BgPortalQueueWriteIndex == BG_PORTAL_QUEUE_LEN)
    {
        g_BgPortalQueueWriteIndex = 0;
    }
    if (g_BgPortalQueueWriteIndex == g_BgPortalQueueReadIndex)
    {
            #ifdef DEBUG
            osSyncPrintf("bg: pstackat: Overflow ");
            #endif
        g_BgPortalQueueWriteIndex--;
    }
}
#endif


/**
 * Address: 7F0B7EE4
 */
#if defined(VERSION_EU)
bool bgProcessNextQueuedPortal(void)
{
    bg_queued_portal_entry *entry;

    if (g_BgPortalQueueReadIndex == g_BgPortalQueueWriteIndex) {
        return 0;
    }

    entry = &g_BgPortalQueue[g_BgPortalQueueReadIndex];

    sub_GAME_7F0B7F84(entry->arg0, entry->roomnum, entry->portalnum, entry->sp4);

    g_BgPortalQueueReadIndex++;

    if (g_BgPortalQueueReadIndex == BG_PORTAL_QUEUE_LEN) {
        g_BgPortalQueueReadIndex = 0;
    }

    return 1;
}
#else
bool bgProcessNextQueuedPortal(s32 *arg0)
{
    bg_queued_portal_entry *entry;
    s32 value;

    value = *arg0;

    if (g_BgPortalQueueReadIndex == g_BgPortalQueueWriteIndex) {
        return 0;
    }

    entry = &g_BgPortalQueue[g_BgPortalQueueReadIndex];

    value = sub_GAME_7F0B7F84(value, entry->roomnum, entry->portalnum, entry->arg3, entry->sp10);

    g_BgPortalQueueReadIndex++;

    if (g_BgPortalQueueReadIndex == BG_PORTAL_QUEUE_LEN) {
        g_BgPortalQueueReadIndex = 0;
    }

    *arg0 = value;

    return 1;
}
#endif


/**
 * EU version drops the value parameter and is type void unlike US/JP which returns s32.
 * 
 * bgPortalDescend
 */
#if defined(VERSION_EU)
void sub_GAME_7F0B7F84(s32 roomnum, s32 portalnum /*canonically p*/, s32 depth, bbox2d *parentbox)
{
    bbox2d screenbox;
    coord3d *playerpos;
    s32 otherroom;
    struct PortalMetric metric;
    f32 playermetric;
    f32 portalmetric;
    s32 i;
 
    D_80044898++;
 
    if (depth >= 101)
    {
#ifdef DEBUG
        osSyncPrintf("bg: << Deep\n");
#endif
        return;
    }
 
    if (D_8004489C < depth)
    {
        return;
    }
 
    if (depth >= 16)
    {
        return;
    }
 
    if (depth);
 
#ifdef DEBUG
    assert(portalnum < PORTMAX);
    osSyncPrintf("bg: << bgPortalDescend: Inside out portal '%s' ", bgDebPrintPORTALID(portalnum));
#endif

    if (g_BgPortals[portalnum].controlbytes1 & PORTALFLAG_DISABLED)
    {
        return;
    }
 
    i = (s32) &D_800442FC[portalnum];

    if (i);
 
    playerpos = bondviewGetCurrentPlayersPosition();
    sub_GAME_7F0B96CC(portalnum, &metric);
    playermetric = ((metric.normal.z * playerpos->z) + ((metric.normal.x * playerpos->x) + (metric.normal.y * playerpos->y))) * room_data_float1;
    portalmetric = sub_GAME_7F0B9990(portalnum);
 
    if (roomnum == g_BgPortals[portalnum].connectedRoom1)
    {
        otherroom = g_BgPortals[portalnum].connectedRoom2;
 
        if (metric.max <= (playermetric - portalmetric))
        {
            return;
        }
    }
    else
    {
        otherroom = g_BgPortals[portalnum].connectedRoom1;
 
        if ((playermetric + portalmetric) <= metric.min)
        {
            return;
        }
    }
 
    if (((metric.min - portalmetric) < playermetric) && (playermetric < (metric.max + portalmetric)))
    {
        screenbox.f[0][0] = g_CurrentPlayer->screensize.f[0][0];
        screenbox.f[0][1] = g_CurrentPlayer->screensize.f[0][1];
        screenbox.f[1][0] = g_CurrentPlayer->screensize.f[1][0];
        screenbox.f[1][1] = g_CurrentPlayer->screensize.f[1][1];
    }
    else
    {
        if (g_BgPortals[portalnum].controlbytes1 & PORTALFLAG_SPECIAL)
        {
            if (!sub_GAME_7F0B5864(portalnum, &screenbox))
            {
                return;
            }
 
            otherroom = (g_BgPortals[portalnum].connectedRoom1 ^ g_BgPortals[portalnum].connectedRoom2) ^ roomnum;
 
            if (!bgIsRoomOnScreen(otherroom, (struct rectbbox *) &screenbox))
            {
                return;
            }
 
            screenbox.f[0][0] = g_CurrentPlayer->screensize.f[0][0];
            screenbox.f[0][1] = g_CurrentPlayer->screensize.f[0][1];
            screenbox.f[1][0] = g_CurrentPlayer->screensize.f[1][0];
            screenbox.f[1][1] = g_CurrentPlayer->screensize.f[1][1];
        }
        else
        {
            if (!sub_GAME_7F0B5864(portalnum, &screenbox))
            {
                return;
            }
 
            bgRectIntersect(&screenbox, parentbox);
            bgRectIntersect(&screenbox, &g_CurrentPlayer->screensize);
        }
 
        if ((screenbox.max.x <= screenbox.min.x) || (screenbox.max.y <= screenbox.min.y))
        {
            return;
        }
    }
 
    *((u8 *) i) = depth;
 
    if ((screenbox.min.x < screenbox.max.x) && (screenbox.min.y < screenbox.max.y))
    {
        if (sub_GAME_7F0B39BC(otherroom, depth, &screenbox, g_BgPortals[portalnum].controlbytes1 & PORTALFLAG_SPECIAL))
        {
            return;
        }
    }
    else
    {
        return;
    }
 
    for (i = 0; g_BgPortals[i].offset_portal != NULL; i++)
    {
        if (i != portalnum)
        {
            if ((otherroom == g_BgPortals[i].connectedRoom1) || (otherroom == g_BgPortals[i].connectedRoom2))
            {
                bgQueuePortalTraversal(otherroom, i, depth + 1, &screenbox);
            }
        }
    }
}
#else
s32 sub_GAME_7F0B7F84(s32 value, s32 roomnum, s32 portalnum /*canonically p*/, s32 depth, bbox2d *parentbox)
{
    bbox2d screenbox;
    coord3d *playerpos;
    s32 otherroom;
    struct PortalMetric metric;
    f32 playermetric;
    f32 portalmetric;
    s32 i;
 
    D_80044898++;
 
    if (depth >= 101)
    {
#ifdef DEBUG
        osSyncPrintf("bg: << Deep\n");
#endif
        return value;
    }
 
    if (D_8004489C < depth)
    {
        return value;
    }
 
    if (depth >= 16)
    {
        return value;
    }
 
    if (depth);
 
#ifdef DEBUG
    assert(portalnum < PORTMAX);
    osSyncPrintf("bg: << bgPortalDescend: Inside out portal '%s' ", bgDebPrintPORTALID(portalnum));
#endif
 
    if (g_BgPortals[portalnum].controlbytes1 & PORTALFLAG_DISABLED)
    {
        return value;
    }
 
    i = (s32) &D_800442FC[portalnum];

    if (i);
 
    playerpos = bondviewGetCurrentPlayersPosition();
    sub_GAME_7F0B96CC(portalnum, &metric);
    playermetric = ((metric.normal.z * playerpos->z) + ((metric.normal.x * playerpos->x) + (metric.normal.y * playerpos->y))) * room_data_float1;
    portalmetric = sub_GAME_7F0B9990(portalnum);
 
    if (roomnum == g_BgPortals[portalnum].connectedRoom1)
    {
        otherroom = g_BgPortals[portalnum].connectedRoom2;
 
        if (metric.max <= (playermetric - portalmetric))
        {
            return value;
        }
    }
    else
    {
        otherroom = g_BgPortals[portalnum].connectedRoom1;
 
        if ((playermetric + portalmetric) <= metric.min)
        {
            return value;
        }
    }
 
    if (((metric.min - portalmetric) < playermetric) && (playermetric < (metric.max + portalmetric)))
    {
        screenbox.f[0][0] = g_CurrentPlayer->screensize.f[0][0];
        screenbox.f[0][1] = g_CurrentPlayer->screensize.f[0][1];
        screenbox.f[1][0] = g_CurrentPlayer->screensize.f[1][0];
        screenbox.f[1][1] = g_CurrentPlayer->screensize.f[1][1];
    }
    else
    {
        if (g_BgPortals[portalnum].controlbytes1 & PORTALFLAG_SPECIAL)
        {
            if (!sub_GAME_7F0B5864(portalnum, &screenbox))
            {
                return value;
            }
 
            otherroom = (g_BgPortals[portalnum].connectedRoom1 ^ g_BgPortals[portalnum].connectedRoom2) ^ roomnum;
 
            if (!bgIsRoomOnScreen(otherroom, (struct rectbbox *) &screenbox))
            {
                return value;
            }
 
            screenbox.f[0][0] = g_CurrentPlayer->screensize.f[0][0];
            screenbox.f[0][1] = g_CurrentPlayer->screensize.f[0][1];
            screenbox.f[1][0] = g_CurrentPlayer->screensize.f[1][0];
            screenbox.f[1][1] = g_CurrentPlayer->screensize.f[1][1];
        }
        else
        {
            if (!sub_GAME_7F0B5864(portalnum, &screenbox))
            {
                return value;
            }
 
            bgRectIntersect(&screenbox, parentbox);
            bgRectIntersect(&screenbox, &g_CurrentPlayer->screensize);
        }
 
        if ((screenbox.max.x <= screenbox.min.x) || (screenbox.max.y <= screenbox.min.y))
        {
            return value;
        }
    }
 
    *((u8 *) i) = depth;
 
    if ((screenbox.min.x < screenbox.max.x) && (screenbox.min.y < screenbox.max.y))
    {
        if (sub_GAME_7F0B39BC(otherroom, depth, &screenbox, g_BgPortals[portalnum].controlbytes1 & PORTALFLAG_SPECIAL))
        {
            return value;
        }
    }
    else
    {
        return value;
    }
 
    for (i = 0; g_BgPortals[i].offset_portal != NULL; i++)
    {
        if (i != portalnum)
        {
            if ((otherroom == g_BgPortals[i].connectedRoom1) || (otherroom == g_BgPortals[i].connectedRoom2))
            {
                bgQueuePortalTraversal(value, otherroom, i, depth + 1, &screenbox);
            }
        }
    }
 
    return value;
}
#endif


/**
 * Add to stack. Push and then increment position. Will wrap on overflow.
 *
 * Address 0x7F0B8374.
 */
s32 bgStackPush(s32 arg0)
{
    g_BgStack[g_BgStackCount] = arg0;
    g_BgStackCount = (s32) (g_BgStackCount + 1) % BG_STACK_SIZE;
    return arg0;
}


/**
 * Pop from stack. Decrement position and retrieve from there. Wraps on underflow.
 *
 * Address 0x7F0B83B0.
 */
s32 bgStackPop(void)
{
    s32 val;

    // ok, who thought this was a good idea
    val = g_BgStack[g_BgStackCount = (g_BgStackCount + (BG_STACK_SIZE-1)) % BG_STACK_SIZE];
    return val;
}


s32 bgStackGetNthValueFromEnd(s32 n) 
{
    return g_BgStack[((g_BgStackCount - n) + (BG_STACK_SIZE - 1)) % BG_STACK_SIZE];
}

GlobalVisCommand *parse_global_vis_command_list(GlobalVisCommand *cmd, s32 execute)
{
    static Unk80081600 dword_CODE_bss_80081600;

    GlobalVisCommand  *ret;
    s32                value;
    bbox2d             sp68;
    bbox2d             sp58;
    u8                 preload_ok = TRUE;

    dword_CODE_bss_80081600.unk10 = FALSE;

    if (cmd == NULL)
    {
        return cmd;
    }

    while (TRUE)
    {
        switch (cmd->type)
        {
            case VISOP_END:
                return cmd;

            case VISOP_PUSH:
                if (execute)
                {
                    bgStackPush(cmd->arg);
                }

                cmd += cmd->length;
                break;

            case VISOP_POP:
                if (execute)
                {
                    bgStackPop();
                }

                cmd += cmd->length;
                break;

            case VISOP_AND:
                if (execute)
                {
                    value = bgStackPop();
                    bgStackPush(bgStackPop() & value);
                }

                cmd += cmd->length;
                break;

            case VISOP_OR:
                if (execute)
                {
                    value = bgStackPop();
                    bgStackPush(bgStackPop() | value);
                }

                cmd += cmd->length;
                break;

            case VISOP_NOT:
                if (execute)
                {
                    bgStackPush(bgStackPop() == 0);
                }

                cmd += cmd->length;
                break;

            case VISOP_XOR:
                if (execute)
                {
                    value = bgStackPop();
                    bgStackPush(bgStackPop() ^ value);
                }

                cmd += cmd->length;
                break;

            case VISOP_PUSH_IF_ROOM_IN_RANGE:
                if (execute)
                {
                    s32 curroom;

                    curroom = g_BgCurrentRoom;

                    bgStackPush(cmd[1].arg <= curroom && curroom <= cmd[2].arg);
                }

                cmd += cmd->length;
                break;

            case VISOP_FORCE_VISIBLE:
                if (execute)
                {
                    dword_CODE_bss_80081600.unk0.f[0][0] = g_CurrentPlayer->screensize.f[0][0];
                    dword_CODE_bss_80081600.unk0.f[0][1] = g_CurrentPlayer->screensize.f[0][1];
                    dword_CODE_bss_80081600.unk0.f[1][0] = g_CurrentPlayer->screensize.f[1][0];
                    dword_CODE_bss_80081600.unk0.f[1][1] = g_CurrentPlayer->screensize.f[1][1];

                    current_visibility = FALSE;
                }

                cmd += cmd->length;
                break;

            case VISOP_MATCH_PORTAL_VIS:
                if (execute)
                {
                    if (sub_GAME_7F0B5864(cmd[1].arg, &dword_CODE_bss_80081600.unk0) == 0)
                    {
                        current_visibility = TRUE;
                    }
                    else if (bgRectIntersect(&dword_CODE_bss_80081600.unk0, &g_CurrentPlayer->screensize) == 0)
                    {
                        current_visibility = TRUE;
                    }
                    else
                    {
                        current_visibility = FALSE;
                    }
                }

                cmd += cmd->length;
                break;

            case VISOP_VISIBLE_IF_SEEN_THROUGH_PORTAL:
                if (execute)
                {
                    if (sub_GAME_7F0B5864(cmd[1].arg, &sp68) && bgRectIntersect(&sp68, &g_CurrentPlayer->screensize))
                    {
                        if (current_visibility)
                        {
                            bbox2dCopy(&dword_CODE_bss_80081600.unk0, &sp68);
                            current_visibility = FALSE;
                        }
                        else
                        {
                            bgRectOutersect(&dword_CODE_bss_80081600.unk0, &sp68);
                        }
                    }
                }

                cmd += cmd->length;
                break;

            case VISOP_NOT_VISIBLE_IF_SEEN_THROUGH_PORTAL:
                if (execute && !current_visibility)
                {
                    if (!sub_GAME_7F0B5864(cmd[1].arg, &sp58))
                    {
                        current_visibility = TRUE;
                    }
                    else if (!bgRectIntersect(&sp58, &g_CurrentPlayer->screensize))
                    {
                        current_visibility = TRUE;
                    }
                    else if (!bgRectIntersect(&dword_CODE_bss_80081600.unk0, &sp58))
                    {
                        current_visibility = TRUE;
                    }
                }

                cmd += cmd->length;
                break;

            case VISOP_ADD_VISIBLE_ROOM:
                if (execute && !current_visibility)
                {
                    if (bgIsRoomOnScreen(cmd[1].arg, (struct rectbbox *)&dword_CODE_bss_80081600.unk0))
                    {
                        sub_GAME_7F0B39BC(cmd[1].arg, 0, &dword_CODE_bss_80081600.unk0, 0);

                        list_visible_rooms_in_cur_global_vis_packet[num_visible_rooms_in_cur_global_vis_packet] = cmd[1].arg;

                        num_visible_rooms_in_cur_global_vis_packet++;
                    }
                }

                cmd += cmd->length;
                break;

            case VISOP_DISABLE_ROOM:
                if (execute)
                {
                    g_BgRoomInfo[cmd[1].arg].room_loaded_mask = TRUE;
                }

                cmd += cmd->length;
                break;

            case VISOP_DISABLE_ROOM_RANGE:
                if (execute)
                {
                    s32 room;

                    room = cmd[1].arg;

                    while (room <= cmd[2].arg)
                    {
                        g_BgRoomInfo[room].room_loaded_mask = TRUE;
                        room++;
                    }
                }

                cmd += cmd->length;
                break;

            case VISOP_PRELOAD_ROOM:
                if (execute && preload_ok)
                {
                    preload_ok = !bgCheckIfRoomModelNeedsLoad(cmd[1].arg);
                }

                cmd += cmd->length;
                break;

            case VISOP_PRELOAD_ROOM_RANGE:
                if (execute)
                {
                    s32 room;

                    room = cmd[1].arg;

                    while (room <= cmd[2].arg)
                    {
                        if (preload_ok)
                        {
                            preload_ok = !bgCheckIfRoomModelNeedsLoad(room);
                        }

                        room++;
                    }
                }

                cmd += cmd->length;
                break;

            case VISOP_REMOVE_VIS:
                if (execute)
                {
                    current_visibility = TRUE;
                }

                cmd += cmd->length;
                break;

            case VISOP_IF_STATEMENT:
                ret = parse_global_vis_command_list(cmd + cmd->length, execute);
                ret += ret->length;
                cmd = ret;
                break;

            case VISOP_ENDIF_CONTINUE_EXEC:
                cmd += cmd->length;
                dword_CODE_bss_80081600.unk10 = FALSE;
                return cmd;

            case VISOP_DONT_EXEC_COMMANDS_EVEN_ON_RETURN:
                cmd += cmd->length;

                if (execute)
                {
                    execute                       = FALSE;
                    dword_CODE_bss_80081600.unk10 = TRUE;
                }
                else
                {
                    execute = FALSE;
                }

                break;

            case VISOP_IF_STATEMENT_PULL_FROM_STACK:
                value = bgStackPop();

                ret = parse_global_vis_command_list(cmd + cmd->length, value & execute);
                cmd = ret;

                if (!dword_CODE_bss_80081600.unk10)
                {
                    continue;
                }

                execute = FALSE;
                break;

            case VISOP_TOGGLE_EXEC_VS_READONLY:
                execute ^= TRUE;
                cmd += cmd->length;
                break;

            case VISOP_ENDIF:
                cmd += cmd->length;
                return cmd;

            default:
                return cmd;
        }
    }
}


GLOBAL_ASM(
.bss
.balign 4
glabel table_for_portals
.space 0xfa0
)


#define RS_STOP 0


// Something about portals. Void* are structs.
void *sub_GAME_7F0B8A24(s32 *pc) 
{

    current_visibility = 0;
    if (!pc)
    {
        return pc;
    }

    bgStackGetNthValueFromEnd(0);
    #ifdef DEBUG
    assert( pc->type==RS_STOP)
    #endif

    return parse_global_vis_command_list(pc, 1);
}


/**
 * Address: 7F0B8A6C
 */
void bgDetermineVisibleRooms(void) 
{
    f32 screenbounds[4];
    s32 var_s0;
    s32 temp_a0;
#if defined(LEFTOVERDEBUG)
    s32 sp44;
#endif
    s32 temp_v1;
    s32 i;

    bgUpdateCurrentPlayerScreenMinMax();

    screenbounds[0] = g_CurrentPlayer->screensize.min.x;
    screenbounds[1] = g_CurrentPlayer->screensize.min.y;
    screenbounds[2] = g_CurrentPlayer->screensize.max.x;
    screenbounds[3] = g_CurrentPlayer->screensize.max.y;

    bgResetPortalVisitCounts();

    for (i = 0; i < MAXROOMCOUNT; i++) 
    {
        g_BgRoomInfo[i].room_rendered = 0;
        g_BgRoomInfo[i].room_neighbor_to_rendered = 0;
        g_BgRoomInfo[i].room_loaded_mask = 0;
    }

    for (i = 0; i < PORTMAX; i++) 
    {
        D_800442FC[i] = 0;
    }
    
    D_80044858 = (s32) (D_80044858 + 1) % 10;

#if defined(LEFTOVERDEBUG)
    dword_CODE_bss_8007FF98 = 0;
#else
    *(s32 *) &dword_CODE_bss_8007FFA0[120] = 0;
#endif
    D_80044898 = 0;

    bgResetPortalQueue();
    sub_GAME_7F0B5168();
    sub_GAME_7F0B8A24(dword_CODE_bss_8007FF90);

    if (1);

    /**
     * If the level is Cradle, or has no portals, skip the portal occlusion culling algorithm. Just add every room in the player's
     * screen bounds to the list of rooms to draw.
     */
    if ((levelentry_index == LEVEL_INDEX_CRAD) || (g_BgPortals->offset_portal == NULL)) 
    {
        if (levelentry_index == LEVEL_INDEX_CRAD) 
        {
            sub_GAME_7F0B39BC(9, 0, &g_CurrentPlayer->screensize, 1);
        }

        for (var_s0 = 1; var_s0 < g_MaxNumRooms; var_s0++) 
        {
            if (bgIsRoomOnScreen(var_s0, &g_CurrentPlayer->screensize) != 0) 
            {
                sub_GAME_7F0B39BC(var_s0, 0, &g_CurrentPlayer->screensize, 1);
            }
        }
    } 
    else 
    {
        // This can never execute since the above branch is taken if the level is Cradle.
        if (levelentry_index == LEVEL_INDEX_CRAD) 
        {
            sub_GAME_7F0B39BC(9, 0, &g_CurrentPlayer->screensize, 1);
        }

        sub_GAME_7F0B39BC(g_BgCurrentRoom, 0, &g_CurrentPlayer->screensize, 1);

        for (var_s0 = 0; g_BgPortals[var_s0].offset_portal != NULL; var_s0++) 
        {
            if ((g_BgCurrentRoom == g_BgPortals[var_s0].connectedRoom1) || (g_BgCurrentRoom == g_BgPortals[var_s0].connectedRoom2)) 
            {
#if defined(LEFTOVERDEBUG)
                bgQueuePortalTraversal(0, g_BgCurrentRoom, var_s0, 1, screenbounds);
#else
                bgQueuePortalTraversal(g_BgCurrentRoom, var_s0, 1, screenbounds);
#endif
            }
        }

#if defined(LEFTOVERDEBUG)
        sp44 = 0;

        while (bgProcessNextQueuedPortal(&sp44) != 0) 
        {
            // empty
        }

        if (1);
#else
        while (bgProcessNextQueuedPortal() != 0) 
        {
            // empty
        }
#endif
    }

    for (var_s0 = 0; g_BgPortals[var_s0].offset_portal != NULL; var_s0++) 
    {
        temp_v1 = g_BgPortals[var_s0].connectedRoom1;
        temp_a0 = g_BgPortals[var_s0].connectedRoom2;

        if ((g_BgRoomInfo[temp_v1].room_rendered != 0) && (g_BgRoomInfo[temp_a0].room_rendered == 0)) 
        {
            g_BgRoomInfo[temp_a0].room_neighbor_to_rendered = 1;
        } 
        else if ((g_BgRoomInfo[temp_a0].room_rendered != 0) && (g_BgRoomInfo[temp_v1].room_rendered == 0)) 
        {
            g_BgRoomInfo[temp_v1].room_neighbor_to_rendered = 1;
        }
    }
}


/**
 * Address 0x7F0B8D78.
*/
Gfx *sub_GAME_7F0B8D78(Gfx *arg0)
{
    s32 i;
    if (levelentry_index == LEVEL_INDEX_DAM)
    {
        for (i=0; i<g_BgNumberOfRoomsDrawn; i++)
        {
            // The lake in dam is a single giant room, id 0x23
            if (dword_CODE_bss_8007FFA0[i].roomid == 0x23)
            {
                // speculation in discord: unk1 is probably draw order or similar,
                // this is a hack to draw the lake first.
                dword_CODE_bss_8007FFA0[i].unk1 = 0;
                break;
            }
        }
    }

    return bgScissorCurrentPlayerViewDefault(sub_GAME_7F0B3C8C(arg0));
}


/**
 * Unreferenced.
 */
s32 sub_GAME_7F0B8DF4(s32 room, s32 *portalnums, s32 max)
{
    bg_portal_data_entry *base;
    bg_portal_data_entry *portal;
    s32 count;
    s32 i;
    s32 offset;

    count = 0;
    i = 0;
    base = g_BgPortals;

    if (room);

    if (base->offset_portal != NULL)
    {
        offset = 0;
        portal = base;

        do
        {
            if ((room == portal->connectedRoom1) || (room == portal->connectedRoom2))
            {
                portalnums[count] = i;
                count++;
            }

            if (count >= max)
            {
                return count;
            }

            offset += 8;
            i++;
            portal = (bg_portal_data_entry *) (((u8 *) g_BgPortals) + offset);
        }
        while (portal->offset_portal != NULL);
    }

    return count;
}


// Copies visible rooms to a list
// Address: 0x7F0B8E98
s32 bgCopyVisibleRoomsToList(s32 *rooms, s32 max)
{
    s32 i;

    for (i = 0; (i < num_visible_rooms_in_cur_global_vis_packet) && (i < max); i++) {
        rooms[i] = list_visible_rooms_in_cur_global_vis_packet[i];
    }

    return i;
}


/**
 * Create a list of rooms connected to roomIndex.
 * @param roomIndex    Room to query.
 * @param list         Output buffer for connected room indices.
 * @param max          Max number of entries to write.
 * @return             Number of rooms written to the list.
 */
s32 bgGetConnectedRooms(s32 roomIndex, s32* list, s32 max)
{
    s32 len = 0;
    s32 i;
    s32 p;
    s32 connectedRoom1;
    s32 connectedRoom2;

    for (p = 0; g_BgPortals[p].offset_portal != NULL; p++) {
        connectedRoom1 = g_BgPortals[p].connectedRoom1;
        connectedRoom2 = g_BgPortals[p].connectedRoom2;

        if (connectedRoom1 == roomIndex) {
            connectedRoom1 = connectedRoom2;
            connectedRoom2 = roomIndex;
        }

        if (connectedRoom2 == roomIndex) {
            for (i = 0; i < len; i++) {
                if (list[i] == connectedRoom1) {
                    goto end;
                }
            }

            list[len] = connectedRoom1;
            len++;

            if (len >= max) {
                return len;
            }
end:
            if (1);
        }
    }

    return len;
}


// Scan all portals to see if these rooms are connected
//
// Room data doesn't contain a list of its portals, so it goes through
// the whole list of portals which seems naive and inefficient.
//
// Address: 0x7F0B8FD0
bool bgRoomsSharePortal(s32 room1, s32 room2) 
{
    s32 i;

    for (i = 0; g_BgPortals[i].offset_portal != NULL; i++)
    {
        s32 v0 = g_BgPortals[i].connectedRoom1;
        s32 v1 = g_BgPortals[i].connectedRoom2;

        if (v0 == room1 && v1 == room2)
        {
            return TRUE;
        }

        if (v1 == room1 && v0 == room2)
        {
            return TRUE;
        }
    }
    return FALSE;
}


/**
 * Unreferenced.
 *
 * Adjusts value in bgViewRelated and returns the new value.
 *
 * @param index: index into bgViewRelated.
 * @param times: multiples value by this amount first.
 * @param add: then adds this.
 *
 * Address 0x7F0B9040.
 */
f32 bgTimesAddViewRelatedMaybe(s32 index, f32 times, f32 add)
{
    bgViewRelated[index] = (s32) (((f32) bgViewRelated[index] * times) + add);
    return (f32) bgViewRelated[index];
}





/**
 * Address 0x7F0B908C.
 */
void bgUpdateCurrentPlayerScreenMinMax(void)
{
    f32 fx;
    f32 fwidth;
    f32 fy;
    f32 fheight;

    fx = (f32) bgViewRelated[0];
    fy = (f32) bgViewRelated[1];
    fwidth = (f32) viGetX() + (f32) bgViewRelated[2];
    fheight = (f32) viGetY() + (f32) bgViewRelated[3];

    g_CurrentPlayer->screensize.min.x = (f32) viGetViewLeft();

    if (g_CurrentPlayer->screensize.min.x < fx)
    {
        g_CurrentPlayer->screensize.min.x = fx;
    }

    if (fwidth < g_CurrentPlayer->screensize.min.x)
    {
        g_CurrentPlayer->screensize.min.x = fwidth;
    }

    g_CurrentPlayer->screensize.min.y = (f32) viGetViewTop();

    if (g_CurrentPlayer->screensize.min.y < fy)
    {
        g_CurrentPlayer->screensize.min.y = fy;
    }

    if (fheight < g_CurrentPlayer->screensize.min.y)
    {
        g_CurrentPlayer->screensize.min.y = fheight;
    }

    g_CurrentPlayer->screensize.max.x = (f32) (viGetViewLeft() + viGetViewWidth());

    if (g_CurrentPlayer->screensize.max.x < fx)
    {
        g_CurrentPlayer->screensize.max.x = fx;
    }

    if (fwidth < g_CurrentPlayer->screensize.max.x)
    {
        g_CurrentPlayer->screensize.max.x = fwidth;
    }

    g_CurrentPlayer->screensize.max.y = (f32) (viGetViewTop() + viGetViewHeight());

    if (g_CurrentPlayer->screensize.max.y < fy)
    {
        g_CurrentPlayer->screensize.max.y = fy;
    }

    if (fheight < g_CurrentPlayer->screensize.max.y)
    {
        g_CurrentPlayer->screensize.max.y = fheight;
    }
}


/**
 * Address: 7F0B92B4
 */
void bgGetRoomCenter(s32 roomnum, coord3d *dst)
{
    s32 i;
    s_room_info *room = &g_BgRoomInfo[roomnum];

    for (i = 0; i < 3; i++) {
        dst->f[i] = (room->minbounds.f[i] + room->maxbounds.f[i]) * 0.5f;
    }
}


#ifdef NONMATCHING
//bgRoomCalcBB
/**
 * 99.36% match on decomp.me
 * https://decomp.me/scratch/Sg7UA
 */
void sub_GAME_7F0B9338(s32 room)
{
    bg_room_data *roomdata;
    Vtx *vertices;
    s32 j;
    s16 limits[6];
    u8 wasloaded;
    s16 new_var;
    roomdata = (bg_room_data *) ((s32) ptr_bgdata_room_fileposition_list + room * 24);
    if (roomdata->pPointTableBin == ((void *) 0))
    {
        if ((room < dword_CODE_bss_8007B9DC) && ((j = (&firststaninroom)[room]) != NULL))
        {
            for (j = 0; j < 3; j++)
            {
                g_BgRoomInfo[room].minbounds.f[j] = ((s16 *) (&dword_CODE_bss_8007B358[room * 3]))[j];
                g_BgRoomInfo[room].maxbounds.f[j] = ((s16 *) (&dword_CODE_bss_8007B358[room * 3]))[j + 3];
                ptr_bgdata_room_fileposition_list[room].pos.f[j] = (((s16 *) (&dword_CODE_bss_8007B358[room * 3]))[j] + ((s16 *) (&dword_CODE_bss_8007B358[room * 3]))[j + 3]) / 2;
            }
        }
#ifdef DEBUG
        else
        {
            osSyncPrintf("bg: bgRoomCalcBB: ROOM%d has no gfx, and no stans! Can\'t make bb & roomoffset ", room);
        }
#endif
        
        return;
    }
    
    wasloaded = g_BgRoomInfo[room].model_bin_loaded;
    
    if (!wasloaded)
    {
        bgLoadRoomModelData(room);
    }
    
    vertices = g_BgRoomInfo[room].vertices;
    roomdata = (bg_room_data *) ((s32) ptr_bgdata_room_fileposition_list + room * 24);
    limits[0] = 0x7fff;
    limits[1] = 0x7fff;
    limits[2] = 0x7fff;
    limits[3] = -0x7fff;
    limits[4] = -0x7fff;
    limits[5] = -0x7fff;
    
    for (; vertices < (Vtx *) ((s32) g_BgRoomInfo[room].vertices + g_BgRoomInfo[room].usize_point_index_binary); vertices++)
    {
        for (j = 0; j < 3; j++)
        {
            if (((s16 *) vertices)[j] < limits[j])
            {
                limits[j] = ((s16 *) vertices)[j];
            }
            
            if (limits[j + 3] < ((s16 *) vertices)[j])
            {
                limits[j + 3] = ((s16 *) vertices)[j];
            }
        }
    }
    
    new_var = limits[0];
    if(1);
    g_BgRoomInfo[room].minbounds.x = roomdata->pos.x + new_var;
    new_var = limits[1];
    if(1);
    g_BgRoomInfo[room].minbounds.y = roomdata->pos.y + new_var;
    new_var = limits[2];
    if(1);
    g_BgRoomInfo[room].minbounds.z = roomdata->pos.z + new_var;
    new_var = limits[3];
    if(1);
    g_BgRoomInfo[room].maxbounds.x = roomdata->pos.x + new_var;
    new_var = limits[4];
    if(1);
    g_BgRoomInfo[room].maxbounds.y = roomdata->pos.y + new_var;
    new_var = limits[5];
    if(1);
    g_BgRoomInfo[room].maxbounds.z = roomdata->pos.z + new_var;
    
    if (wasloaded == 0)
    {
        delete_room_data(room);
    }
}
#else
GLOBAL_ASM(
.text
glabel sub_GAME_7F0B9338
/* 0EDE68 7F0B9338 3C088008 */  lui   $t0, %hi(ptr_bgdata_room_fileposition_list)
/* 0EDE6C 7F0B933C 2508FF8C */  addiu $t0, %lo(ptr_bgdata_room_fileposition_list) # addiu $t0, $t0, -0x74
/* 0EDE70 7F0B9340 8D0E0000 */  lw    $t6, ($t0)
/* 0EDE74 7F0B9344 00041080 */  sll   $v0, $a0, 2
/* 0EDE78 7F0B9348 27BDFFB0 */  addiu $sp, $sp, -0x50
/* 0EDE7C 7F0B934C 00441023 */  subu  $v0, $v0, $a0
/* 0EDE80 7F0B9350 000210C0 */  sll   $v0, $v0, 3
/* 0EDE84 7F0B9354 AFBF001C */  sw    $ra, 0x1c($sp)
/* 0EDE88 7F0B9358 AFB00018 */  sw    $s0, 0x18($sp)
/* 0EDE8C 7F0B935C 01C24821 */  addu  $t1, $t6, $v0
/* 0EDE90 7F0B9360 8D2F0000 */  lw    $t7, ($t1)
/* 0EDE94 7F0B9364 3C188004 */  lui   $t8, %hi(g_BgRoomInfo)
/* 0EDE98 7F0B9368 00808025 */  move  $s0, $a0
/* 0EDE9C 7F0B936C 15E00035 */  bnez  $t7, .L7F0B9444
/* 0EDEA0 7F0B9370 27181414 */   addiu $t8, %lo(g_BgRoomInfo) # addiu $t8, $t8, 0x1414
/* 0EDEA4 7F0B9374 3C188008 */  lui   $t8, %hi(dword_CODE_bss_8007B9DC)
/* 0EDEA8 7F0B9378 8F18B9DC */  lw    $t8, %lo(dword_CODE_bss_8007B9DC)($t8)
/* 0EDEAC 7F0B937C 0004C880 */  sll   $t9, $a0, 2
/* 0EDEB0 7F0B9380 3C0B8008 */  lui   $t3, %hi(firststaninroom)
/* 0EDEB4 7F0B9384 0098082A */  slt   $at, $a0, $t8
/* 0EDEB8 7F0B9388 1020008E */  beqz  $at, .L7F0B95C4
/* 0EDEBC 7F0B938C 01795821 */   addu  $t3, $t3, $t9
/* 0EDEC0 7F0B9390 8D6BB128 */  lw    $t3, %lo(firststaninroom)($t3)
/* 0EDEC4 7F0B9394 00106080 */  sll   $t4, $s0, 2
/* 0EDEC8 7F0B9398 01906021 */  addu  $t4, $t4, $s0
/* 0EDECC 7F0B939C 11600089 */  beqz  $t3, .L7F0B95C4
/* 0EDED0 7F0B93A0 00002025 */   move  $a0, $zero
/* 0EDED4 7F0B93A4 00107080 */  sll   $t6, $s0, 2
/* 0EDED8 7F0B93A8 01D07023 */  subu  $t6, $t6, $s0
/* 0EDEDC 7F0B93AC 3C0D8004 */  lui   $t5, %hi(g_BgRoomInfo)
/* 0EDEE0 7F0B93B0 3C0F8008 */  lui   $t7, %hi(dword_CODE_bss_8007B358)
/* 0EDEE4 7F0B93B4 25EFB358 */  addiu $t7, %lo(dword_CODE_bss_8007B358) # addiu $t7, $t7, -0x4ca8
/* 0EDEE8 7F0B93B8 25AD1414 */  addiu $t5, %lo(g_BgRoomInfo) # addiu $t5, $t5, 0x1414
/* 0EDEEC 7F0B93BC 000E7080 */  sll   $t6, $t6, 2
/* 0EDEF0 7F0B93C0 000C6100 */  sll   $t4, $t4, 4
/* 0EDEF4 7F0B93C4 018D2821 */  addu  $a1, $t4, $t5
/* 0EDEF8 7F0B93C8 01CF3021 */  addu  $a2, $t6, $t7
/* 0EDEFC 7F0B93CC 2407000C */  li    $a3, 12
.L7F0B93D0:
/* 0EDF00 7F0B93D0 84C20000 */  lh    $v0, ($a2)
/* 0EDF04 7F0B93D4 84C30006 */  lh    $v1, 6($a2)
/* 0EDF08 7F0B93D8 8D0B0000 */  lw    $t3, ($t0)
/* 0EDF0C 7F0B93DC 44822000 */  mtc1  $v0, $f4
/* 0EDF10 7F0B93E0 44834000 */  mtc1  $v1, $f8
/* 0EDF14 7F0B93E4 0062C021 */  addu  $t8, $v1, $v0
/* 0EDF18 7F0B93E8 468021A0 */  cvt.s.w $f6, $f4
/* 0EDF1C 7F0B93EC 00106080 */  sll   $t4, $s0, 2
/* 0EDF20 7F0B93F0 01906023 */  subu  $t4, $t4, $s0
/* 0EDF24 7F0B93F4 000C60C0 */  sll   $t4, $t4, 3
/* 0EDF28 7F0B93F8 016C6821 */  addu  $t5, $t3, $t4
/* 0EDF2C 7F0B93FC 468042A0 */  cvt.s.w $f10, $f8
/* 0EDF30 7F0B9400 01A47021 */  addu  $t6, $t5, $a0
/* 0EDF34 7F0B9404 24840004 */  addiu $a0, $a0, 4
/* 0EDF38 7F0B9408 E4A60038 */  swc1  $f6, 0x38($a1)
/* 0EDF3C 7F0B940C 24A50004 */  addiu $a1, $a1, 4
/* 0EDF40 7F0B9410 24C60002 */  addiu $a2, $a2, 2
/* 0EDF44 7F0B9414 E4AA0040 */  swc1  $f10, 0x40($a1)
/* 0EDF48 7F0B9418 07010003 */  bgez  $t8, .L7F0B9428
/* 0EDF4C 7F0B941C 0018C843 */   sra   $t9, $t8, 1
/* 0EDF50 7F0B9420 27010001 */  addiu $at, $t8, 1
/* 0EDF54 7F0B9424 0001C843 */  sra   $t9, $at, 1
.L7F0B9428:
/* 0EDF58 7F0B9428 44998000 */  mtc1  $t9, $f16
/* 0EDF5C 7F0B942C 00000000 */  nop
/* 0EDF60 7F0B9430 468084A0 */  cvt.s.w $f18, $f16
/* 0EDF64 7F0B9434 1487FFE6 */  bne   $a0, $a3, .L7F0B93D0
/* 0EDF68 7F0B9438 E5D2000C */   swc1  $f18, 0xc($t6)
/* 0EDF6C 7F0B943C 10000062 */  b     .L7F0B95C8
/* 0EDF70 7F0B9440 8FBF001C */   lw    $ra, 0x1c($sp)
.L7F0B9444:
/* 0EDF74 7F0B9444 00107880 */  sll   $t7, $s0, 2
/* 0EDF78 7F0B9448 01F07821 */  addu  $t7, $t7, $s0
/* 0EDF7C 7F0B944C 000F7900 */  sll   $t7, $t7, 4
/* 0EDF80 7F0B9450 01F84021 */  addu  $t0, $t7, $t8
/* 0EDF84 7F0B9454 910A0002 */  lbu   $t2, 2($t0)
/* 0EDF88 7F0B9458 02002025 */  move  $a0, $s0
/* 0EDF8C 7F0B945C 5540000C */  bnezl $t2, .L7F0B9490
/* 0EDF90 7F0B9460 8D060004 */   lw    $a2, 4($t0)
/* 0EDF94 7F0B9464 AFA20030 */  sw    $v0, 0x30($sp)
/* 0EDF98 7F0B9468 AFA80028 */  sw    $t0, 0x28($sp)
/* 0EDF9C 7F0B946C 0FC2D8DA */  jal   bgLoadRoomModelData
/* 0EDFA0 7F0B9470 A3AA0037 */   sb    $t2, 0x37($sp)
/* 0EDFA4 7F0B9474 3C198008 */  lui   $t9, %hi(ptr_bgdata_room_fileposition_list)
/* 0EDFA8 7F0B9478 8F39FF8C */  lw    $t9, %lo(ptr_bgdata_room_fileposition_list)($t9)
/* 0EDFAC 7F0B947C 8FA20030 */  lw    $v0, 0x30($sp)
/* 0EDFB0 7F0B9480 8FA80028 */  lw    $t0, 0x28($sp)
/* 0EDFB4 7F0B9484 93AA0037 */  lbu   $t2, 0x37($sp)
/* 0EDFB8 7F0B9488 03224821 */  addu  $t1, $t9, $v0
/* 0EDFBC 7F0B948C 8D060004 */  lw    $a2, 4($t0)
.L7F0B9490:
/* 0EDFC0 7F0B9490 8D19001C */  lw    $t9, 0x1c($t0)
/* 0EDFC4 7F0B9494 240B7FFF */  li    $t3, 32767
/* 0EDFC8 7F0B9498 240C7FFF */  li    $t4, 32767
/* 0EDFCC 7F0B949C 03263821 */  addu  $a3, $t9, $a2
/* 0EDFD0 7F0B94A0 240D7FFF */  li    $t5, 32767
/* 0EDFD4 7F0B94A4 240E8001 */  li    $t6, -32767
/* 0EDFD8 7F0B94A8 240F8001 */  li    $t7, -32767
/* 0EDFDC 7F0B94AC 24188001 */  li    $t8, -32767
/* 0EDFE0 7F0B94B0 00C7082B */  sltu  $at, $a2, $a3
/* 0EDFE4 7F0B94B4 A7AB0038 */  sh    $t3, 0x38($sp)
/* 0EDFE8 7F0B94B8 A7AC003A */  sh    $t4, 0x3a($sp)
/* 0EDFEC 7F0B94BC A7AD003C */  sh    $t5, 0x3c($sp)
/* 0EDFF0 7F0B94C0 A7AE003E */  sh    $t6, 0x3e($sp)
/* 0EDFF4 7F0B94C4 A7AF0040 */  sh    $t7, 0x40($sp)
/* 0EDFF8 7F0B94C8 10200017 */  beqz  $at, .L7F0B9528
/* 0EDFFC 7F0B94CC A7B80042 */   sh    $t8, 0x42($sp)
/* 0EE000 7F0B94D0 27A5003E */  addiu $a1, $sp, 0x3e
/* 0EE004 7F0B94D4 00C02025 */  move  $a0, $a2
.L7F0B94D8:
/* 0EE008 7F0B94D8 27A30038 */  addiu $v1, $sp, 0x38
.L7F0B94DC:
/* 0EE00C 7F0B94DC 84820000 */  lh    $v0, ($a0)
/* 0EE010 7F0B94E0 846B0000 */  lh    $t3, ($v1)
/* 0EE014 7F0B94E4 004B082A */  slt   $at, $v0, $t3
/* 0EE018 7F0B94E8 50200004 */  beql  $at, $zero, .L7F0B94FC
/* 0EE01C 7F0B94EC 846C0006 */   lh    $t4, 6($v1)
/* 0EE020 7F0B94F0 A4620000 */  sh    $v0, ($v1)
/* 0EE024 7F0B94F4 84820000 */  lh    $v0, ($a0)
/* 0EE028 7F0B94F8 846C0006 */  lh    $t4, 6($v1)
.L7F0B94FC:
/* 0EE02C 7F0B94FC 0182082A */  slt   $at, $t4, $v0
/* 0EE030 7F0B9500 50200003 */  beql  $at, $zero, .L7F0B9510
/* 0EE034 7F0B9504 24630002 */   addiu $v1, $v1, 2
/* 0EE038 7F0B9508 A4620006 */  sh    $v0, 6($v1)
/* 0EE03C 7F0B950C 24630002 */  addiu $v1, $v1, 2
.L7F0B9510:
/* 0EE040 7F0B9510 1465FFF2 */  bne   $v1, $a1, .L7F0B94DC
/* 0EE044 7F0B9514 24840002 */   addiu $a0, $a0, 2
/* 0EE048 7F0B9518 24C60010 */  addiu $a2, $a2, 0x10
/* 0EE04C 7F0B951C 00C7082B */  sltu  $at, $a2, $a3
/* 0EE050 7F0B9520 5420FFED */  bnezl $at, .L7F0B94D8
/* 0EE054 7F0B9524 00C02025 */   move  $a0, $a2
.L7F0B9528:
/* 0EE058 7F0B9528 87AD0038 */  lh    $t5, 0x38($sp)
/* 0EE05C 7F0B952C 87AE003A */  lh    $t6, 0x3a($sp)
/* 0EE060 7F0B9530 C524000C */  lwc1  $f4, 0xc($t1)
/* 0EE064 7F0B9534 448D3000 */  mtc1  $t5, $f6
/* 0EE068 7F0B9538 448E9000 */  mtc1  $t6, $f18
/* 0EE06C 7F0B953C 87AF003C */  lh    $t7, 0x3c($sp)
/* 0EE070 7F0B9540 46803220 */  cvt.s.w $f8, $f6
/* 0EE074 7F0B9544 87B8003E */  lh    $t8, 0x3e($sp)
/* 0EE078 7F0B9548 87B90040 */  lh    $t9, 0x40($sp)
/* 0EE07C 7F0B954C 87AB0042 */  lh    $t3, 0x42($sp)
/* 0EE080 7F0B9550 468091A0 */  cvt.s.w $f6, $f18
/* 0EE084 7F0B9554 46082280 */  add.s $f10, $f4, $f8
/* 0EE088 7F0B9558 E50A0038 */  swc1  $f10, 0x38($t0)
/* 0EE08C 7F0B955C C5300010 */  lwc1  $f16, 0x10($t1)
/* 0EE090 7F0B9560 448F5000 */  mtc1  $t7, $f10
/* 0EE094 7F0B9564 46068100 */  add.s $f4, $f16, $f6
/* 0EE098 7F0B9568 468054A0 */  cvt.s.w $f18, $f10
/* 0EE09C 7F0B956C E504003C */  swc1  $f4, 0x3c($t0)
/* 0EE0A0 7F0B9570 C5280014 */  lwc1  $f8, 0x14($t1)
/* 0EE0A4 7F0B9574 44982000 */  mtc1  $t8, $f4
/* 0EE0A8 7F0B9578 46124400 */  add.s $f16, $f8, $f18
/* 0EE0AC 7F0B957C 468022A0 */  cvt.s.w $f10, $f4
/* 0EE0B0 7F0B9580 E5100040 */  swc1  $f16, 0x40($t0)
/* 0EE0B4 7F0B9584 C526000C */  lwc1  $f6, 0xc($t1)
/* 0EE0B8 7F0B9588 44998000 */  mtc1  $t9, $f16
/* 0EE0BC 7F0B958C 460A3200 */  add.s $f8, $f6, $f10
/* 0EE0C0 7F0B9590 46808120 */  cvt.s.w $f4, $f16
/* 0EE0C4 7F0B9594 E5080044 */  swc1  $f8, 0x44($t0)
/* 0EE0C8 7F0B9598 C5320010 */  lwc1  $f18, 0x10($t1)
/* 0EE0CC 7F0B959C 448B4000 */  mtc1  $t3, $f8
/* 0EE0D0 7F0B95A0 46049180 */  add.s $f6, $f18, $f4
/* 0EE0D4 7F0B95A4 46804420 */  cvt.s.w $f16, $f8
/* 0EE0D8 7F0B95A8 E5060048 */  swc1  $f6, 0x48($t0)
/* 0EE0DC 7F0B95AC C52A0014 */  lwc1  $f10, 0x14($t1)
/* 0EE0E0 7F0B95B0 46105480 */  add.s $f18, $f10, $f16
/* 0EE0E4 7F0B95B4 15400003 */  bnez  $t2, .L7F0B95C4
/* 0EE0E8 7F0B95B8 E512004C */   swc1  $f18, 0x4c($t0)
/* 0EE0EC 7F0B95BC 0FC2D971 */  jal   delete_room_data
/* 0EE0F0 7F0B95C0 02002025 */   move  $a0, $s0
.L7F0B95C4:
/* 0EE0F4 7F0B95C4 8FBF001C */  lw    $ra, 0x1c($sp)
.L7F0B95C8:
/* 0EE0F8 7F0B95C8 8FB00018 */  lw    $s0, 0x18($sp)
/* 0EE0FC 7F0B95CC 27BD0050 */  addiu $sp, $sp, 0x50
/* 0EE100 7F0B95D0 03E00008 */  jr    $ra
/* 0EE104 7F0B95D4 00000000 */   nop
)
#endif


void sub_GAME_7F0B95D8(s32 roomID)
{
    s32 numupdated = 0;
    s32 i;
    s32 j;
    s32 k;
    f32 value;

    for (i = 0; g_BgPortals[i].offset_portal != NULL; i++)
    {
        if ((roomID == g_BgPortals[i].connectedRoom1) || (roomID == g_BgPortals[i].connectedRoom2))
        {
            for (j = 0; j < g_BgPortals[i].offset_portal->numPoints; j++)
            {
                for (k = 0; k < 3; k++)
                {
                    value = (&g_BgPortals[i].offset_portal->point)[j].f[k];

                    if (value < g_BgRoomInfo[roomID].minbounds.f[k])
                    {
                        g_BgRoomInfo[roomID].minbounds.f[k] = value;
                        numupdated++;
                    }

                    if (g_BgRoomInfo[roomID].maxbounds.f[k] < value)
                    {
                        g_BgRoomInfo[roomID].maxbounds.f[k] = value;
                        numupdated++;
                    }
                }
            }
        }
    }

    if (numupdated);
}


// This assert belongs somewhere in the function
//#ifdef
//assert(levelportals[p].p->n>=3)
//#endif
void sub_GAME_7F0B96CC(s32 portalnum, f32 *out)
{
    f32 sp6c[3];
    f32 sp60[3];
    bg_portal_entry *portal;
    f32 min;
    f32 max;
    f32 dot;
    s32 i;

    for (i = 0; i < 3; i++)
    {
        sp6c[i] = (&g_BgPortals[portalnum].offset_portal->point)[0].f[i] - (&g_BgPortals[portalnum].offset_portal->point)[1].f[i];
    }

    for (i = 0; i < 3; i++)
    {
        sp60[i] = (&g_BgPortals[portalnum].offset_portal->point)[2].f[i] - (&g_BgPortals[portalnum].offset_portal->point)[1].f[i];
    }

    out[0] = (sp6c[1] * sp60[2]) - (sp6c[2] * sp60[1]);
    out[1] = (sp6c[2] * sp60[0]) - (sp6c[0] * sp60[2]);
    out[2] = (sp6c[0] * sp60[1]) - (sp6c[1] * sp60[0]);

    dot = sqrtf(((out[0] * out[0]) + (out[1] * out[1])) + (out[2] * out[2]));

    if (dot != 0.0f)
    {
        dot = 1.0f / dot;
    }

    out[0] *= dot;
    out[1] *= dot;
    out[2] *= dot;

    portal = g_BgPortals[portalnum].offset_portal;

    min = 3.4028235e38f;
    max = -3.4028235e38f;

    for (i = 0; i < portal->numPoints; i++)
    {
        min = (((((&portal->point)[i].f[0] * out[0]) + ((&portal->point)[i].f[1] * out[1])) + ((&portal->point)[i].f[2] * out[2])) < min) ? ((((&portal->point)[i].f[0] * out[0]) + ((&portal->point)[i].f[1] * out[1])) + ((&portal->point)[i].f[2] * out[2])) : (min);
        max = (((((&portal->point)[i].f[0] * out[0]) + ((&portal->point)[i].f[1] * out[1])) + ((&portal->point)[i].f[2] * out[2])) > max) ? ((((&portal->point)[i].f[0] * out[0]) + ((&portal->point)[i].f[1] * out[1])) + ((&portal->point)[i].f[2] * out[2])) : (max);
    }

    out[3] = min;
    out[4] = max;

    if (dot);
}


/**
 * Unknown, makes use of sub_GAME_7F0B96CC.
 *
 * Address 0x7F0B993C.
 */
s32 sub_GAME_7F0B993C(s32 arg0)
{
    struct PortalMetric metric;
    s32 padding;

    sub_GAME_7F0B96CC(arg0, &metric);

    if (((metric.normal.f[0] * metric.normal.f[0]) + (metric.normal.f[2] * metric.normal.f[2])) < 0.999f)
    {
        return 0;
    }

    return 1;
}


f32 sub_GAME_7F0B9990(s32 portalnum)
{
    s32 value;
    s32 shift;
    f32 result;

    value = g_BgPortals[portalnum].controlbytes2;
    shift = (value >> 4) & 0xf;
    result = (value & 0xf) * 0.25f;

    while (shift != 0) {
        result += result;
        shift--;
    }

    return result;
}


/**
 * Unreferenced.
 *
 * Address 0x7F0B9A14.
 */
u8 bgGetDataPortalsControlBytes2(s32 p)
{
    return g_BgPortals[p].controlbytes2;
}


void sub_GAME_7F0B9A2C(s32 portalnum)
{
    u8 value;
    s32 upper;

    value = g_BgPortals[portalnum].controlbytes2;

    if (value >= 0xff)
    {
        value = 0xff;
    }
    else
    {
        value++;
        upper = (value >> 4) & 0xf;

        if (upper > 0)
        {
            value |= 8;
        }
    }

    g_BgPortals[portalnum].controlbytes2 = value;
}


void sub_GAME_7F0B9A7C(s32 portalnum)
{
    u8 value;
    s32 temp;

    value = g_BgPortals[portalnum].controlbytes2;
    temp = value;

    if (((value >> 4) & 0xf) == 0)
    {
        if (temp > 0)
        {
            value--;
        }
    }
    else
    {
        value--;

        if ((value & 0xf) < 8)
        {
            value -= 8;
        }
    }

    g_BgPortals[portalnum].controlbytes2 = value;
}


/**
 * @param index: index into portal array.
 *
 * Address 0x7F0B9AE4.
 */
s32 bgGetDataPortalsControlBytes1Bit1(s32 index)
{
    return g_BgPortals[index].controlbytes1 & 1;
}



/**
 * @param index: index into portal array.
 *
 * Address 0x7F0B9B04.
 */
s32 bgGetDataPortalsControlBytes1Bit2(s32 index)
{
    return g_BgPortals[index].controlbytes1 & 2;
}



/**
 * @param index: index into portal array.
 *
 * Address 0x7F0B9B24.
 */
void bgSetDataPortalsControlBytes1Bit2(s32 index)
{
    g_BgPortals[index].controlbytes1 |= 2;
}



/**
 * @param index: index into portal array.
 *
 * Address 0x7F0B9B44.
 */
void bgClearDataPortalsControlBytes1Low2Bits(s32 index)
{
    g_BgPortals[index].controlbytes1 &= 0xFD;
}


/**
 * Swaps connected rooms.
 *
 * @param index: index into portal array.
 *
 * Address 0x7F0B9B64.
 */
s8 bgSwapConnectedRooms(s32 index)
{
    u8 t;

    t = g_BgPortals[index].connectedRoom1;
    g_BgPortals[index].connectedRoom1 = g_BgPortals[index].connectedRoom2;
    g_BgPortals[index].connectedRoom2 = t;
}


/**
 * Address: 7F0B9B94
 */
void bgOrderPortal(s32 portalnum) // canonical name
{
    coord3d room1centre;
    coord3d room2centre;
    struct PortalMetric metric;
    f32 tmp;
    f32 tmp2;
    s32 roomnum2;
    s32 swapped;
    s32 roomnum1;
    f32 tmp1;

    roomnum1 = g_BgPortals[portalnum].connectedRoom1;
    roomnum2 = g_BgPortals[portalnum].connectedRoom2;

    bgGetRoomCenter(roomnum1, &room1centre);
    bgGetRoomCenter(roomnum2, &room2centre);

    sub_GAME_7F0B96CC(portalnum, &metric);

    if (metric.max - metric.min < 0.1f) {
#ifdef DEBUG
        osSyncPrintf("bg: bgOrderPortal: Portal '%s' not planar by %5.2f\n", bgDebPrintPORTALID(portalnum), metric.max - metric.min);
#endif
    }

    tmp2 = metric.normal.f[2];
    tmp1 = metric.normal.f[0] * room1centre.f[0] + metric.normal.f[1] * room1centre.f[1] + tmp2 * room1centre.f[2];

    swapped = 0;

    if (tmp1 > metric.max) {
        swapped = 1;

        bgSwapConnectedRooms(portalnum);

        metric.normal.x = -metric.normal.x;
        metric.normal.y = -metric.normal.y;
        metric.normal.z = -metric.normal.z;

        tmp = metric.min;
        metric.min = -metric.max;
        metric.max = -tmp;
    }

	tmp2 = metric.normal.f[0] * room2centre.f[0] + metric.normal.f[1] * room2centre.f[1] + metric.normal.f[2] * room2centre.f[2];

	if (tmp2 <= metric.min) {
        if (swapped) {
            bgSwapConnectedRooms(portalnum);
        }
    }

    if (swapped);
}


/**
 * Address: 7F0B9CC8
 *
 */
s32 bgGetPortalBetweenRooms(s32 room1, s32 room2, coord3d *arg2, coord3d *arg3)
{
    s32 bFoundPortal = FALSE;
    s32 i;
    s32 portalIndex = -1;

    #ifndef DEBUG
        #define osSyncPrintf(x)
    #endif

    for (i = 0; g_BgPortals[i].offset_portal != NULL; i++)
    {
        if (((g_BgPortals[i].connectedRoom1 == room1) && (g_BgPortals[i].connectedRoom2 == room2)) ||
            ((g_BgPortals[i].connectedRoom1 == room2) && (g_BgPortals[i].connectedRoom2 == room1)))
        {
            bFoundPortal = TRUE;
            if (sub_GAME_7F0B9F14(i, arg2, arg3) != 0)
            {
                if (portalIndex >= 0) osSyncPrintf("bg: bgGetPortalBetweenRooms(): Multiple portals join room \'%s\' and \'%s\'\ n", bgDebPrintROOMID(room1), bgDebPrintROOMID(room2));
                portalIndex = i;
            }
        }
    }

    if (portalIndex == -1 && !bFoundPortal) osSyncPrintf("bg: bgGetPortalBetweenRooms(): No portal joins room \'%s\' and \'%s\'\n", bgDebPrintROOMID(room1), bgDebPrintROOMID(room2));

    return portalIndex;
    #undef osSyncPrintf
}






/**
 * Toggles control bytes 1 lowest bit, based on toggle parameter.
 *
 * @param index: index into data portals.
 * @param toggle: When zero, sets lowest bit. Otherwise, clears lowest bit.
 *
 * Address 0x7F0B9DBC.
 */
void bgToggleDataPortalsContrlBytes1Bit1(s32 portal, s32 toggle)
{
    #ifdef DEBUG
    assert(portal<PORTMAX);
    #endif
    g_BgPortals[portal].controlbytes1 = (g_BgPortals[portal].controlbytes1 | 1) ^ (toggle != 0);
}




/**
 * Debug method, called from lvl.c.
 * Something to do with portals.
 *
 * Address 0x7F0B9DE4.
 */
s32 bgDebugRemoved7F0B9DE4(s32 arg0, s32 arg1, s32 arg2)
{
#if DEBUG
    // removed
    /*
 if (arg2 == NULL)
 {
     arg2 = {0};
 }
 *arg0          = *arg1 + *arg2;
 arg0[1]        = arg1[1] + arg2[1];
 arg0[2]        = arg1[2] + arg2[2];
 *(arg0 + 3)    = 0;
 arg0[4]        = 0.0;
 arg0[5]        = 0.0;
 *(arg0 + 6)    = (param_4 & 0xcf00cf40) >> 0x18;
 *(arg0 + 0x19) = 0;
 *(arg0 + 0x1a) = (param_4 & 0xcf00cf40) >> 8;
 *(arg0 + 0x1b) = 0x40;

 */

#endif

    return arg0;
}




/**
 * Debug method, called from lvl.c.
 * Something to do with portals.
 *
 * Address 0x7F0B9DF4.
 */
void bgRemoved7F0B9DF4(s32 arg0)
{
#if DEBUG
    // removed
#endif

    return;
}

/**
 * Unreferenced.
 *
 * Address 0x7F0B9DFC.
 */
void bgRemoved7F0B9DFC(s32 p)
{
#if DEBUG
        osSyncPrintf("bg: Error: Multiple portals intersect line; \'%s\' dropped ", bgDebPrintPORTALID(p));
#endif

    return;
}


//bg_consider_window82397B18
s32 sub_GAME_7F0B9E04(coord3d *arg0, coord3d *arg1)
{
    s32 bestportalnum = -1;
    s32 count = 0;
    f32 bestthing = MAXFLOAT;
    f32 thisthing;
    s32 i;

    for (i = 0; g_BgPortals[i].offset_portal; i++)
    {
        if (sub_GAME_7F0B9F14(i, arg0, arg1) != 0)
        {
            thisthing = D_80044900;

            if (thisthing < 0)
            {
                thisthing = -thisthing;
#if DEBUG
                osSyncPrintf("bg: Portal \'%s\' briefly considered for window\n", bgDebPrintPORTALID(i));
#endif
            }

            if (thisthing < bestthing)
            {
                if (count)
                {
#if DEBUG
                    osSyncPrintf("bg: Portal \'%s\' briefly considered for window\n", bgDebPrintPORTALID(i));
#endif
                }
                if (i);
                bestportalnum = i;
                bestthing = thisthing;
                count++;
            }
        }
    }

    return bestportalnum;
}


s32 sub_GAME_7F0B9F14(s32 portalnum, coord3d *pos1, coord3d *pos2)
{
    f32 value1;
    f32 value2;
    coord3d diff;
    s32 padding;
    s32 next;
    f32 tmp;
    coord3d edge;
    struct PortalMetric metric;
    f32 plane_d[1];
    coord3d cross;
    s32 i;
    u8 seenA;
    u8 seenB;

    seenA = 0;
    seenB = 0;

    sub_GAME_7F0B96CC(portalnum, &metric);

    diff.f[0] = pos2->f[0] - pos1->f[0];
    diff.f[1] = pos2->f[1] - pos1->f[1];
    diff.f[2] = pos2->f[2] - pos1->f[2];

    value1 = (pos1->f[0] * metric.normal.f[0] + pos1->f[1] * metric.normal.f[1] + pos1->f[2] * metric.normal.f[2]) * room_data_float1;
    value2 = (pos2->f[0] * metric.normal.f[0] + pos2->f[1] * metric.normal.f[1] + pos2->f[2] * metric.normal.f[2]) * room_data_float1;

    // @bug? Using min everywhere. In PD, the second half of this statement uses max.
    if ((value1 < metric.min && value2 < metric.min) || (value1 > metric.min && value2 > metric.min))
    {
        return 0;
    }

    D_80044900 = (value1 + value2) * 0.5f - metric.min;

    for (i = 0; i < g_BgPortals[portalnum].offset_portal->numPoints; i++)
    {
        next = (i + 1) % g_BgPortals[portalnum].offset_portal->numPoints;

        edge.f[0] = (&g_BgPortals[portalnum].offset_portal->point)[next].f[0] - (&g_BgPortals[portalnum].offset_portal->point)[i].f[0];
        edge.f[1] = (&g_BgPortals[portalnum].offset_portal->point)[next].f[1] - (&g_BgPortals[portalnum].offset_portal->point)[i].f[1];
        edge.f[2] = (&g_BgPortals[portalnum].offset_portal->point)[next].f[2] - (&g_BgPortals[portalnum].offset_portal->point)[i].f[2];

        cross.f[0] = edge.f[1] * diff.f[2] - edge.f[2] * diff.f[1];
        cross.f[1] = edge.f[2] * diff.f[0] - edge.f[0] * diff.f[2];
        cross.f[2] = edge.f[0] * diff.f[1] - edge.f[1] * diff.f[0];

        tmp = cross.f[0] * cross.f[0] + cross.f[1] * cross.f[1] + cross.f[2] * cross.f[2];

        if (tmp == 0.0f)
        {
            return 0;
        }

        plane_d[0] = cross.f[0] * (&g_BgPortals[portalnum].offset_portal->point)[i].f[0]
            + cross.f[1] * (&g_BgPortals[portalnum].offset_portal->point)[i].f[1]
            + cross.f[2] * (&g_BgPortals[portalnum].offset_portal->point)[i].f[2];

        tmp = (cross.f[0] * pos1->f[0] + cross.f[1] * pos1->f[1] + cross.f[2] * pos1->f[2]) * room_data_float1;

        if (tmp < plane_d[0])
        {
            if (seenB)
            {
                return 0;
            }

            seenA = 1;
        }
        else
        {
            if (seenA)
            {
                return 0;
            }

            seenB = 1;
        }
    }

    return value1 < metric.min ? 1 : 2;
}


bool bgIsBboxOverlapping(coord3d *portalbbmin, coord3d *portalbbmax, coord3d *propbbmin, coord3d *propbbmax)
{
    s32 i;

    for (i = 0; i < 3; i++)
    {
        if (propbbmin->f[i] > portalbbmax->f[i] || propbbmax->f[i] < portalbbmin->f[i])
        {
            return FALSE;
        }
    }

    return TRUE;
}


void sub_GAME_7F0BA2D4(coord3d *bbmin, coord3d *bbmax, s32 *room_list, s32 *count, s32 max_count)
{
    bg_portal_entry *portal_pts;
    f32 v;
    s32 cur_room;
    coord3d scaled_bbmin;
    coord3d scaled_bbmax;
    s32 cur_count;
    s32 i;
    s32 j;
    s32 k;
    s32 pad;
    s32 saved_count;
    coord3d portal_min;
    coord3d portal_max;
    s32 portal_idx;
    s32 *p;
    s32 other_room;
    
    cur_count = *count;
    i = 0;
    scaled_bbmin.x = bbmin->x * room_data_float1;
    scaled_bbmin.y = bbmin->y * room_data_float1;
    scaled_bbmin.z = bbmin->z * room_data_float1;
    scaled_bbmax.x = bbmax->x * room_data_float1;
    scaled_bbmax.y = bbmax->y * room_data_float1;
    scaled_bbmax.z = bbmax->z * room_data_float1;
    saved_count = cur_count;
    
    while (1)
    {
        if (i < cur_count)
        {
            p = (s32 *)((u8 *)room_list + (i << 2)); do {
            cur_room = *p;
            portal_idx = 0;
 
            if (g_BgPortals[0].offset_portal != ((void *) 0))
            {
                do
                {
                    if ((g_BgPortals[portal_idx].controlbytes1 & 1) || ((cur_room != g_BgPortals[portal_idx].connectedRoom1) && (cur_room != g_BgPortals[portal_idx].connectedRoom2)))
                    {
                        goto next_portal;
                    }
                    
                    portal_min = *(coord3d *) &D_80044904;
                    portal_max = *(coord3d *) &D_80044910;
                    portal_pts = g_BgPortals[portal_idx].offset_portal;
                    
                    for (j = 0; j < portal_pts->numPoints; j++)
                    {
                        for (k = 0; k < 3; k++)
                        {
                            v = (&portal_pts->point)[j].f[k];
                            
                            if (v < portal_min.f[k])
                            {
                                portal_min.f[k] = v;
                            }
                            
                            if (portal_max.f[k] < v)
                            {
                                portal_max.f[k] = v;
                            }
                            
                            portal_pts = g_BgPortals[portal_idx].offset_portal;
                        }
 
                        if (portal_pts->numPoints);
                    }
                    
                if (bgIsBboxOverlapping(&portal_min, &portal_max, &scaled_bbmin, &scaled_bbmax))
                {
                    if (cur_room == g_BgPortals[portal_idx].connectedRoom1)
                    {
                        other_room = g_BgPortals[portal_idx].connectedRoom2;
                    }
                    else
                    {
                        other_room = g_BgPortals[portal_idx].connectedRoom1;
                    }
                    
                    for (k = 0; k < cur_count; k++)
                    {
                        if (room_list[k] == other_room)
                        {
                            break;
                        }
                    }
                    
                    if (k == cur_count)
                    {
                        if (cur_count < max_count)
                        {
                            room_list[cur_count] = other_room;
                            cur_count++;
                        }
                        
                        if (cur_count >= max_count)
                        {
                            *count = cur_count;
                            return;
                        }
                    }
                }
                    
next_portal:
                portal_idx++;
                }         
                while (g_BgPortals[portal_idx].offset_portal != NULL);
            }

            i++;
            p++;
                    
            } while (i < saved_count);
        }    
 
        if (cur_count == saved_count)
        {
            break;
        }
     
        saved_count = cur_count;
    }
    *count = cur_count;
}
