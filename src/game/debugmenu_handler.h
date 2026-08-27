#ifndef _debugmenu_handler_H_
#define _debugmenu_handler_H_
#include <ultra64.h>

s32 get_debug_enable_agent_levels_flag(void);
s32 get_debug_enable_all_levels_flag(void);
s32 get_debug_fast_bond_flag(void);
void set_debug_fast_bond_flag(s32 flag);
s32 get_debug_all_obj_complete_flag(void);
s32 get_debug_portal_flag(void);
s32 get_debug_gunwatchpos_flag(void);

#endif
