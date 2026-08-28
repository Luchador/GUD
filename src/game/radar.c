#include <ultra64.h>
#include "math_atan2f.h"
#include "textrelated.h"
#include "front.h"
#include "fr.h"
#include "player.h"
#include "image_bank.h"


Gfx *radarRender(Gfx *gdl)
{
    s32 player_count;
    s32 cur_playernum;
    enum MPSCENARIOS current_scenario;
    
    s32 start_left;
    s32 start_top;
    s32 dl_color_1;
    s32 i;
    PropRecord *other_player_prop;
    PropRecord *player_prop;

    f32 temp_f20;
    f32 temp_f22;
    s32 temp_f24;
    f32 temp_f28;
    f32 temp_f2;
    s32 dl_color_2;
    s32 loop_start_left;
    s32 loop_start_top;
    f32 temp_f16;

    #define RADAR_TOP_OFFSET 26
    #define RADAR_RECT1_OFFSET 16
    #define RADAR_RECT1_D 1024
    #define RADAR_VERT_SCALE 1.0f
    
    current_scenario = get_scenario();
    cur_playernum = get_cur_playernum();
    player_count = getPlayerCount();
    
    if ((g_CurrentPlayer->mpmenuon) || (g_CurrentPlayer->bondstate == BONDSTATE_JUST_DIED || g_CurrentPlayer->bondstate == BONDSTATE_DEAD))
    {
        return gdl;
    }
    
    if (cheatIsActive(CHEAT_NO_RADAR_MP))
    {
        return gdl;
    }

    start_left = (viGetViewLeft() + viGetViewWidth()) - 41;
    start_top = viGetViewTop() + RADAR_TOP_OFFSET;
    
    if ((player_count >= 3) && !(cur_playernum & 1))
    {
        start_left += 15;
    }
    
    texSelect(&gdl, mpradarimages, 2, 0, 2);

    gdl = gfxSetup2DTextureMode(gdl);

    gDPSetCombineLERP(gdl++, 0, 0, 0, PRIMITIVE, PRIMITIVE, 0, TEXEL0, 0, 0, 0, 0, PRIMITIVE, PRIMITIVE, 0, TEXEL0, 0);
    gDPSetPrimColor(gdl++, 0, 0, 0x00, 0x00, 0x00, 0xA0);

    gSPTextureRectangle(
        gdl++,
        (start_left - 16) << 2,
        (start_top - RADAR_RECT1_OFFSET) << 2,
        (start_left + 16) << 2,
        (start_top + RADAR_RECT1_OFFSET) << 2,
        G_TX_RENDERTILE,
        16,
        16,
        1024,
        RADAR_RECT1_D);
    
    gdl = gfxDrawTranslucentRect(gdl, start_left - 2, start_top - 2, start_left + 2, start_top + 2, 64);
    
    if ((current_scenario == SCENARIO_2v2)
        || (current_scenario == SCENARIO_3v1)
        || (current_scenario == SCENARIO_2v1)
        || (current_scenario == SCENARIO_TLD)
        || (current_scenario == SCENARIO_MWTGG))
    {
        if (!g_playerPlayerData[cur_playernum].have_token_or_goldengun)
        {
            dl_color_1 = 0xFF7777FF;
        }
        else
        {
            dl_color_1 = 0x8888FFFF;
        }
        
        gdl = gfxDrawTranslucentRect(gdl, start_left - 1, start_top - 1, start_left + 1, start_top + 1, dl_color_1);
    }
    else
    {
        gdl = gfxDrawTranslucentRect(gdl, start_left - 1, start_top - 1, start_left + 1, start_top + 1, -96);
    }

    for (i = 0; i < player_count; i++)
    {
        if (i != cur_playernum)
        {
            if (g_playerPointers[i]->bondstate == BONDSTATE_ALIVE)
            {
                f32 tt1;
                other_player_prop = g_playerPointers[i]->prop;
                player_prop = g_CurrentPlayer->prop;

                temp_f20 = other_player_prop->pos.x - player_prop->pos.x;
                temp_f22 = other_player_prop->pos.z - player_prop->pos.z;
                
                temp_f28 = ((atan2f(temp_f20, temp_f22) * 180.0f) / M_PI_F) + g_CurrentPlayer->vv_theta + 180.0f;
                
                temp_f24 = 16;
                temp_f16 = 4000;

                tt1 = (temp_f24 / temp_f16);
                temp_f2 = sqrtf((temp_f20 * temp_f20) + (temp_f22 * temp_f22)) * tt1;
            
                if ((current_scenario == SCENARIO_2v2)
                    || (current_scenario == SCENARIO_3v1)
                    || (current_scenario == SCENARIO_2v1)
                    || (current_scenario == SCENARIO_TLD)
                    || (current_scenario == SCENARIO_MWTGG))
                {
                    if (temp_f2 < temp_f24)
                    {
                        if (!g_playerPlayerData[i].have_token_or_goldengun)
                        {
                            dl_color_2 = 0xFF0000A0;
                        }
                        else
                        {
                            dl_color_2 = 0x2828FFFF;
                        }
                    }
                    else
                    {
                        temp_f2 = temp_f24;

                        if (!g_playerPlayerData[i].have_token_or_goldengun)
                        {
                            dl_color_2 = 0xFF000060;
                        }
                        else
                        {
                            dl_color_2 = 0x2828FFB0;
                        }
                    }
                }
                else
                {
                    dl_color_2 = 0xFFFF0060;
    
                    if (temp_f2 < temp_f24)
                    {
                        dl_color_2 = 0xFFFF00A0;
                    }
                    else
                    {
                        temp_f2 = temp_f24;
                    }
                }

                loop_start_left = (s32) (sinf(temp_f28 * DegToRad(1)) * temp_f2) + start_left;
                loop_start_top = (s32) (cosf(temp_f28 * DegToRad(1)) * temp_f2 * RADAR_VERT_SCALE) + start_top;
                
                gdl = gfxDrawTranslucentRect(gdl, loop_start_left - 2, loop_start_top - 2, loop_start_left + 2, loop_start_top + 2, 64);
                gdl = gfxDrawTranslucentRect(gdl, loop_start_left - 1, loop_start_top - 1, loop_start_left + 1, loop_start_top + 1, dl_color_2);
            }
        }
    }

    return gfxRestore3DRenderMode(gdl);

    #undef RADAR_TOP_OFFSET
    #undef RADAR_RECT1_OFFSET
    #undef RADAR_RECT1_D
    #undef RADAR_VERT_SCALE
}
