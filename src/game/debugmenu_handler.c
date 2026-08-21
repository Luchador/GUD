#include <ultra64.h>
#include "debugmenu_handler.h"
#include "initgamedata.h"
#include <boss.h>
#include <fr.h>

#ifndef DEBUG
    #define osSyncPrintf()
#endif
#if defined(LEFTOVERDEBUG)

// data
//D:80036BA0
u32 D_80036BA0 = 0;
//D:80036BA4
s32 g_DebugMenuOffsets[] =
{
       8, 0x13, 0x1E, 0x2B,
    0x32, 0x39, 0x45, 0x4D,
    -1
};

//D:80036BC8
struct mcm_layout g_DebugMenuPositions[] = {
    {8, 5},
    {8, 6},
    {8, 7},
    {8, 8},
    {8, 9},
    {8, 0xA},
    {8, 0xB},
    {8, 0xC},

    {0x19, 5},
    {0x19, 6},
    {0x19, 7},
    {0x19, 8},
    {0x19, 9},
    {0x19, 0xA},
    {0x19, 0xB},
    {0x19, 0xC},
    {0x19, 0xD},
    {0x19, 0xE},
    {0x19, 0xF},

    {0x28, 5},
    {0x28, 6},
    {0x28, 7},
    {0x28, 8},
    {0x28, 9},
    {0x28, 0xA},
    {0x28, 0xB},
    {0x28, 0xC},
    {0x28, 0xD},
    {0x28, 0xE},
    {0x28, 0xF},

    {0x39, 5},
    {0x39, 6},
    {0x39, 7},
    {0x39, 8},
    {0x39, 9},
    {0x39, 0xA},
    {0x39, 0xB},
    {0x39, 0xC},
    {0x39, 0xD},
    {0x39, 0xE},
    {0x39, 0xF},
    {0x39, 0x10},
    {0x39, 0x11},

    {8, 0x14},
    {8, 0x15},
    {8, 0x16},
    {8, 0x17},
    {8, 0x18},
    {8, 0x19},
    {8, 0x1A},

    {0x19, 0x14},
    {0x19, 0x15},
    {0x19, 0x16},
    {0x19, 0x17},
    {0x19, 0x18},
    {0x19, 0x19},
    {0x19, 0x1A},

    {0x28, 0x14},
    {0x28, 0x15},
    {0x28, 0x16},
    {0x28, 0x17},
    {0x28, 0x18},
    {0x28, 0x19},
    {0x28, 0x1A},
    {0x28, 0x1B},
    {0x28, 0x1C},
    {0x28, 0x1D},
    {0x28, 0x1E},
    {0x28, 0x1F},

    {0x39, 0x14},
    {0x39, 0x15},
    {0x39, 0x16},
    {0x39, 0x17},
    {0x39, 0x18},
    {0x39, 0x19},
    {0x39, 0x1A},
    {0x39, 0x1B}
};

//D:80036E30
char *mcm_strings[] = {
     "move view",
     "stan view",
     "bond view",
     "level",
     "region",
     "scale",
     "play title",
     "bond die",

     "select anim",
     "gun pos",
     "flash colour",
     "hit colour",
     "music",
     "sfx",
     "invincible",
     "visible",
     "collisions",
     "all guns",
     "max ammo",

     "display speed",
     "background",
     "props",
     "stan hit",
     "stan region",
     "stan problems",
     "print man pos",
     "port close",
     "port inf",
     "port approx",
     "pr room loads",

     "show mem use",
     "show mem bars",
     "grab rgb",
     "grab jpeg",
     "grab task",
     "rnd walk",
     "record ramrom",
     "record 1",
     "record 2",
     "record 3",
     "replay ramrom",
     "save ramrom",
     "load ramrom",

     "auto y aim",
     "auto x aim",
     "007",
     "agent",
     "all",
     "fast",
     "objectives",

     "marg top",
     "marg bot",
     "marg left",
     "marg right",
     "marg reset",
     "screen size",
     "screen pos",

     "show patrols",
     "intro",
     "intro edit",
     "intro pos",
     "world pos",
     "gun key pos",
     "vis cvg",
     "chr num",
     "room blocks",
     "profile",
     "obj load",
     "weapon load",

     "joy2 sky edit",
     "joy2 hits edit",
     "joy2 detail edit",
     "explosion info",
     "magic fog",
     "gun watch pos",
     "testing man pos",
     "fog"
};

//D:80036F64
s32 debug_render_raster = DEB_BOND_VIEW;
//D:80036F68
s32 debug_freeze_processing = DEB_BOND_VIEW;
//D:80036F6C
s32 g_DebugMode = DEB_BOND_VIEW;
//D:80036F70
s32 g_DebugHighlightedOption = DEB_BOND_VIEW;
//D:80036F78
s32 debug_do_draw_bg = TRUE;
//D:80036F7C
s32 debug_do_draw_obj = TRUE;
//D:80036F84
s32 debug_stanhit_flag = FALSE;
//D:80036F88
s32 debug_stanregion_flag = FALSE;
//D:80036F8C
s32 debug_stan_problems_flag = FALSE;
//D:80036F90
s32 debug_man_pos_flag = 0;
//D:80036F94
s32 debug_prroomloads_flag = 0;
//D:80036F98
s32 debug_joy2skyedit_flag = 0;
//D:80036F9C
s32 debug_joy2hitsedit_flag = 0;
//D:80036FA0
s32 debug_joy2detailedit_flag = 0;
//D:80036FA4
s32 debug_explosioninfo_flag = 0;
#endif


#if defined(LEFTOVERDEBUG)
//D:80036FAC
s32 debug_007_unlock_flag = 0;
//D:80036FB0
s32 debug_enable_agent_levels_flag = 0;
//D:80036FB4
s32 debug_enable_all_levels_flag = 0;
//D:80036FB8
s32 debug_chrnum_flag = 0;

//D:80036FBC
s32 debug_gunwatchpos_flags = 0;
//D:80036FC0
s32 debug_profile_flag = 0;
//D:80036FC4
s32 debug_enable_taskgrab_flag = 0;
#endif

s32 g_DebugManPos = 0;

//D:80036FCC
s32 debug_fast_bond_flag = 0;

#if defined(LEFTOVERDEBUG)
//D:80036FD0
s32 debug_all_obj_complete_flag = 0 ;
//D:80036FD4
s32 debug_portal_flag = 0;

//D:80036FD8
u32 dword_D_80036FD8[] = {
    0x80500000,
    0x40000,
    0x7F000000,
    0x7F100000
};

u32 dword_D_80036FE8[] = {
    0x80600000,
    0x40000,
    0x70000000,
    0x70100000
};

//D:80036FF8
s32 show_debug_menu_flag = 0;


struct ScreenshotFlag {
    s32 flag;
};

//D:80036FFC
struct ScreenshotFlag g_DebugScreenshotFlagRgb = { 0 };
//D:80037000
struct ScreenshotFlag g_DebugScreenshotFlagJpg = { 0 };

//D:80037004
coord3d player_pos_x = {0};

#endif


s32 get_debug_freeze_processing(void) {
#if defined(LEFTOVERDEBUG)
    return debug_freeze_processing;
#else
    return 2;
#endif
}

s32 getDebugMode(void) 
{
#if defined(LEFTOVERDEBUG)
    return g_DebugMode;
#else
    return 2;
#endif
}


s32 get_debug_do_draw_bg(void) {
#if defined(LEFTOVERDEBUG)
    return debug_do_draw_bg;
#else
    return 1;
#endif
}

s32 get_debug_do_draw_obj(void) {
#if defined(LEFTOVERDEBUG)
    return debug_do_draw_obj;
#else
    return 1;
#endif
}

s32 get_debug_stanhit_flag(void) {
#if defined(LEFTOVERDEBUG)
    return debug_stanhit_flag;
#else
    return 0;
#endif
}

s32 get_debug_stanregion_flag(void) {
#if defined(LEFTOVERDEBUG)
    return debug_stanregion_flag;
#else
    return 0;
#endif
}

s32 get_debug_stan_problems_flag(void) {
#if defined(LEFTOVERDEBUG)
    return debug_stan_problems_flag;
#else
    return 0;
#endif
}

s32 get_debug_man_pos_flag(void) {
#if defined(LEFTOVERDEBUG)
    return debug_man_pos_flag;
#else
    return 0;
#endif
}

s32 get_debug_testingmanpos_flag(void)
{
    return g_DebugManPos;
}

void set_debug_testingmanpos_flag(s32 flag)
{
    g_DebugManPos = flag;
}

s32 get_debug_joy2skyedit_flag(void) {
#if defined(LEFTOVERDEBUG)
    return debug_joy2skyedit_flag;
#else
    return 0;
#endif
}

s32 get_debug_joy2hitsedit_flag(void) {
#if defined(LEFTOVERDEBUG)
    return debug_joy2hitsedit_flag;
#else
    return 0;
#endif
}

s32 get_debug_joy2detailedit_flag(void) {
#if defined(LEFTOVERDEBUG)
    return debug_joy2detailedit_flag;
#else
    return 0;
#endif
}

s32 get_debug_explosioninfo_flag(void) {
#if defined(LEFTOVERDEBUG)
    return debug_explosioninfo_flag;
#else
    return 0;
#endif
}

s32 debugIsRoomStateDebugEnabled(void) {
#if defined(LEFTOVERDEBUG)
    return debug_prroomloads_flag;
#else
    return 0;
#endif
}


s32 get_debug_007_unlock_flag(void) {
#if defined(LEFTOVERDEBUG)
    return debug_007_unlock_flag;
#else
    return 0;
#endif
}

s32 get_debug_enable_agent_levels_flag(void) {
#if defined(LEFTOVERDEBUG)
    return debug_enable_agent_levels_flag;
#else
    return 0;
#endif
}

s32 get_debug_enable_all_levels_flag(void) {
#if defined(LEFTOVERDEBUG)
    return debug_enable_all_levels_flag;
#else
    return 0;
#endif
}





#if !defined(LEFTOVERDEBUG)
s32 get_debug_chrnum_flag(void) {
    return 0;
}
s32 get_debug_gunwatchpos_flag(void) {
    return 0;
}
s32  get_debug_profile_flag(void) {
    return 0;
}
s32 get_debug_taskgrab_val(void) {
    return 0;
}
void func_7F0904C0(void)
{
    // removed
}
void func_7F0904C8(void)
{
    // removed
}
s32 get_debug_fast_bond_flag(void) {
    return debug_fast_bond_flag;
}
void set_debug_fast_bond_flag(s32 flag) {
    debug_fast_bond_flag = flag;
}
s32 get_debug_all_obj_complete_flag(void) {
    return 0;
}
s32 get_debug_portal_flag(void) {
    return 0;
}
void func_7F0904F8(s32 arg0)
{
    // removed
}
#endif


#if defined(LEFTOVERDEBUG)
s32 get_debug_fast_bond_flag(void) {
    return debug_fast_bond_flag;
}
#endif

#if defined(LEFTOVERDEBUG)
void set_debug_fast_bond_flag(s32 flag) {
    debug_fast_bond_flag = flag;
}
#endif

#if defined(LEFTOVERDEBUG)
s32 get_debug_all_obj_complete_flag(void) {
    return debug_all_obj_complete_flag;
}
#endif

#if defined(LEFTOVERDEBUG)
s32 get_debug_portal_flag(void) {
    return debug_portal_flag;
}
#endif

#if defined(LEFTOVERDEBUG)
s32 get_debug_chrnum_flag(void) {
    return debug_chrnum_flag;
}
#endif

#if defined(LEFTOVERDEBUG)
s32 get_debug_gunwatchpos_flag(void) {
    return debug_gunwatchpos_flags;
}
#endif

#if defined(LEFTOVERDEBUG)
s32  get_debug_profile_flag(void) {
    return debug_profile_flag;
}
#endif

#if defined(LEFTOVERDEBUG)
s32 get_debug_taskgrab_val(void) {
    return debug_enable_taskgrab_flag;
}
#endif



