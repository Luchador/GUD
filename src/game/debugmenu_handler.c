#include <ultra64.h>
#include "debugmenu_handler.h"
#include "initgamedata.h"
#include <boss.h>
#include <fr.h>


//D:80036FBC
s32 debug_gunwatchpos_flags = 0;
//D:80036FC0
s32 debug_profile_flag = 0;
//D:80036FC4
s32 debug_enable_taskgrab_flag = 0;

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