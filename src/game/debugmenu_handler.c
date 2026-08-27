#include <ultra64.h>
#include "debugmenu_handler.h"
#include "initgamedata.h"
#include <boss.h>
#include <fr.h>

#ifndef DEBUG
    #define osSyncPrintf()
#endif
#if defined(LEFTOVERDEBUG)

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

s32 get_debug_gunwatchpos_flag(void) {
    return 0;
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
s32 get_debug_gunwatchpos_flag(void) {
    return debug_gunwatchpos_flags;
}
#endif
