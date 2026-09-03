#include <ultra64.h>
#include "vi.h"
#include "fr.h"
#include <bondgame.h>
#include <bondconstants.h>
#include "cam.h"
#include "player.h"
#include "sched.h"
#include "rsp.h"
#include <PR/os.h>
#include "game/bondview.h"
#include "game/viewport.h"
#include "game/dyn.h"
// same codegen as (x & 0xffff)
#define TO_U16_A(x) ((u16)(x))
// same codegen as ((u16)(x))
#define TO_U16_B(x) (x & 0xffff)

#define TO_U16_C(x) ((u16)(x & 0xffff))

#define TO_U16_M(x) ((x) % 0xffff)

/**
 * Splits a 32 bit value into upper and lower 16, adds a 16 bit value to each, and combines back to 32 bits.
 *
 * Note: both unsigned 16 bit casts result in the same codegen, but I'm using different
 * macros to give a more "natural" explanation of the asm codegen.
 */
#define ADD_LOW_AND_HI_16_TRUNCATE(x32, add16) ((TO_U16_B(TO_U16_A((x32) >> 16) + (add16)) << 16) | TO_U16_B(TO_U16_A(x32) + (add16)))

#define ADD_LOW_AND_HI_16_MOD(x32, add16) ((TO_U16_M(TO_U16_B((x32) >> 16) + (add16)) << 16) | TO_U16_M(TO_U16_A(x32) + (add16)))

struct VideoSettings_s g_ViDataArray[NUM_VIDEO_SETTINGS] =
{
    {MD_BLACK, 0, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, FOV_Y_F, ASPECT_RATIO, 30.0f, 10000.0f, SCREEN_WIDTH, SCREEN_HEIGHT, SCREEN_WIDTH, SCREEN_HEIGHT, 0, 0, TRUE, NULL},
    {MD_BLACK, 0, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, FOV_Y_F, ASPECT_RATIO, 30.0f, 10000.0f, SCREEN_WIDTH, SCREEN_HEIGHT, SCREEN_WIDTH, SCREEN_HEIGHT, 0, 0, TRUE, NULL}
};

VideoSettings *g_ViFrontData = &g_ViDataArray[0];
VideoSettings *g_ViBackData = &g_ViDataArray[0];
s32 g_viColorOutputMode = COLORMODE_16BIT;

/**
 * g_ViShakeIntensity multiplier sign. This is only ever 1 or -1.
 */
s32 g_viVstartSign = 1;

/**
 * vimode vStart vertical offset in viVsyncRelated.
 */
s32 g_ViShakeIntensity = 0;

/**
 * Some kind of counter for how frequently g_ViShakeIntensity is zero'd in viVsyncRelated.
 */
u32 g_ViShakeTimer = 0;

/**
 * Some kind of counter for how frequently osViBlack is called with "active" parameter.
 */
u32 g_ViUnblackTimer = 3;

/**
 * should correlate to g_schedViCurrentFrameBuffer
 */
s32 D_800232C0 = 0;

f32 g_viProjectionMatrixF[4][4];
Mtx *g_viProjectionMatrix;
u16 g_viPerspNorm;

/**
 * Original vi mode.
 * (name following n64devkit\ultra\usr\src\pr\demos\threadmonkey\block.c)
 */
OSViMode g_viOriginalVideoMode;

/**
 * cfb_16 index for g_ViFrontData.
 */
u8 g_ViFrontIndex;

/**
 * cfb_16 index for g_ViBackData and g_CurrentPlayer->viewports.
 */
u8 g_ViBackIndex;

/**
 * Current vi mode pointer.
 */
OSViMode *viMode;

/**
 * Original viMode->comRegs.hStart.
 */
u32 g_viOriginalHstart;

/**
 * Original viMode->fldRegs[0].vStart.
 */
u32 g_viOriginalVstart0;

/**
 * Original viMode->fldRegs[1].vStart.
 */
u32 g_viOriginalVstart1;

/**
 * Const runtime supported screen widths.
 */
const s16 g_viRuntimeScreenWidths[] = {SCREEN_WIDTH_MIN, SCREEN_WIDTH, SCREEN_WIDTH_MAX};

/**
 * Const runtime supported screen heights.
 */
const s16 g_viRuntimeScreenHeights[] = {SCREEN_HEIGHT_MIN, SCREEN_HEIGHT, SCREEN_HEIGHT_MAX};


void viInitVideoSettings(void)
{
    g_ViFrontIndex = 0;
    g_ViBackIndex = 1;

    g_ViFrontData = (VideoSettings*)((u8*)&g_ViDataArray + (g_ViFrontIndex * sizeof(VideoSettings))); //AKA front = viDataArray[0]
    g_ViFrontData->framebuf = cfb_16[g_ViFrontIndex];

    g_ViBackData = (VideoSettings*)((u8*)&g_ViDataArray + (g_ViBackIndex * sizeof(VideoSettings))); //AKA back = viDataArray[1] - was Rare mental?
    g_ViBackData->framebuf = cfb_16[g_ViBackIndex];
}


void viInitBuffers(void)
{
    s32 i;
    u8* p1 = cfb_16[0];
    u8* p2 = cfb_16[1];

    zbufDeallocate();

    for (i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT * 2; i++)
    {
        p1[i] = 0;
        p2[i] = 0;
    }
}


void frBlack(u32 black)
{
    black += 2;
    g_ViUnblackTimer = black;
}


/**
 * Calls osViSetMode with current viMode.
 * Also calls osViBlack.
 * Applies verticalOffset to vstart.
 *
 * Looks related to n64devkit\ultra\usr\src\pr\demos\blockmonkey\block.c
 * in particular, ModifyVStart
 */
void viVsyncRelated(void)
{
    s32 verticalOffset;
    s32 registerValue;

    if (g_ViShakeTimer > 0)
    {
        g_ViShakeTimer--;

        if (g_ViShakeTimer == 0)
        {
            g_ViShakeIntensity = 0;
        }
    }

    verticalOffset = g_viVstartSign * g_ViShakeIntensity;

    // note the implicit cast from unsigned to signed.
    registerValue = g_viOriginalVstart0;
    (*viMode).fldRegs[0].vStart = ADD_LOW_AND_HI_16_TRUNCATE(registerValue, verticalOffset);

    registerValue = g_viOriginalVstart1;
    (*viMode).fldRegs[1].vStart = ADD_LOW_AND_HI_16_TRUNCATE(registerValue, verticalOffset);

    osViSetMode(viMode);
    osViBlack(g_ViUnblackTimer);

    // g_ViUnblackTimer might be declared signed which means the if statement is
    //
    //    if (g_ViUnblackTimer != 0 && g_ViUnblackTimer < 3)
    //
    // but this way feels "safer", though the cast to (s32) now seems unnatural.
    // The "right" answer depends on what's supposed to happen for values like -1 or 0xffffffff ...
    if (g_ViUnblackTimer > 0 && (s32)g_ViUnblackTimer < 3)
    {
        g_ViUnblackTimer--;
    }

    //smooth dither pattern and disable weird gamma
    //(N64 was supposed to compensate for CRT's 2.2 gamma, but by this time most games were made with sRGB in mind)
    osViSetSpecialFeatures(OS_VI_DITHER_FILTER_ON | OS_VI_GAMMA_OFF);

    g_viVstartSign = -g_viVstartSign;
}


void video_related_8(void)
{
    u32 packedStart;
    s32 registerValue;
    f32 calculatedYScale;
    s32 yScaleMax;
    f32 calculatedXScale;
    VideoSettings *settings;
    s32 nextMode;

    if (g_ViFrontData->mode != g_ViBackData->mode)
    {
        registerValue = g_ViBackData->mode;

        switch (registerValue)
        {
            case MD_BLACK:
            {
                osViSetYScale(1.0f);
                osViBlack(TRUE);
                break;
            }

            case MD_NORMAL:
            {
                if (g_viColorOutputMode != COLORMODE_32BIT)
                {
                    if (osTvType == TV_TYPE_MPAL)
                    {
                        g_viOriginalVideoMode = osViModeTable[OS_VI_MPAL_LAN1];
                    }
                    else
                    {
                        g_viOriginalVideoMode = osViModeTable[OS_VI_NTSC_LAN1];
                    }
                }
                else
                {
                    if (osTvType == TV_TYPE_MPAL)
                    {
                        g_viOriginalVideoMode = osViModeTable[OS_VI_MPAL_LAN2];
                    }
                    else
                    {
                        g_viOriginalVideoMode = osViModeTable[OS_VI_NTSC_LAN2];
                    }
                }

                viMode = &g_viOriginalVideoMode;
                g_viOriginalHstart = viMode->comRegs.hStart;
                g_viOriginalVstart0 = viMode->fldRegs[0].vStart;
                g_viOriginalVstart1 = viMode->fldRegs[1].vStart;

                frBlack(0);
                break;
            }
        }

        osViSetSpecialFeatures(OS_VI_DITHER_FILTER_ON | OS_VI_GAMMA_OFF);
    }

    calculatedXScale = (f32)g_ViBackData->x / (f32)g_ViBackData->bufx;
    calculatedYScale = (f32)g_ViBackData->y / (f32)g_ViBackData->bufy;

    if (g_ViBackData->mode == MD_BLACK)
    {
        calculatedYScale = 1.0f;
    } 

    nextMode = D_800232C0;
    yScaleMax = YSCALE_MAX;

    g_ViXScales[nextMode] = calculatedXScale;
    g_ViYScales[nextMode] = calculatedYScale;

    if (g_ViBackData->mode == MD_NORMAL)
    {
        g_ViModePtrs[nextMode] = &g_viOriginalVideoMode;

        if (g_viColorOutputMode != COLORMODE_32BIT)
        {
            if (osTvType == TV_TYPE_MPAL)
            {
                g_ViModes[nextMode] = osViModeTable[OS_VI_MPAL_LAN1];
            }
            else
            {
                g_ViModes[nextMode] = osViModeTable[OS_VI_NTSC_LAN1];
            }
        }
        else
        {
            if (osTvType == TV_TYPE_MPAL)
            {
                g_ViModes[nextMode] = osViModeTable[OS_VI_MPAL_LAN2];
            }
            else
            {
                g_ViModes[nextMode] = osViModeTable[OS_VI_NTSC_LAN2];
            }
        }

        g_ViModes[nextMode].comRegs.width = g_ViBackData->bufx;
        g_ViModes[nextMode].comRegs.xScale = (g_ViBackData->bufx * XSCALE_MAX) / SCREEN_WIDTH_MAX;
        g_ViModes[nextMode].fldRegs[0].yScale = (g_ViBackData->bufy * yScaleMax) / SCREEN_HEIGHT_MAX;
        
        packedStart = g_ViModes[nextMode].comRegs.hStart;

        g_ViModes[nextMode].fldRegs[1].yScale = (g_ViBackData->bufy * yScaleMax) / SCREEN_HEIGHT_MAX;
        g_ViModes[nextMode].fldRegs[0].origin = g_ViBackData->bufx * 2;
        g_ViModes[nextMode].fldRegs[1].origin = g_ViBackData->bufx * 2;

        registerValue = packedStart;
        registerValue = ADD_LOW_AND_HI_16_MOD(registerValue, 0);

        g_ViModes[nextMode].comRegs.hStart = registerValue;
        g_viOriginalHstart = registerValue;

        packedStart = g_ViModes[nextMode].fldRegs[0].vStart;
        registerValue = packedStart;
        registerValue = ADD_LOW_AND_HI_16_MOD(registerValue, 0);

        g_ViModes[nextMode].fldRegs[0].vStart = registerValue;
        g_viOriginalVstart0 = registerValue;

        packedStart = g_ViModes[nextMode].fldRegs[1].vStart;
        registerValue = packedStart;
        registerValue = ADD_LOW_AND_HI_16_MOD(registerValue, 0);

        g_ViModes[nextMode].fldRegs[1].vStart = registerValue;
        g_viOriginalVstart1 = registerValue;

        g_ViChangeVideoModes[nextMode] = 1;
    }
    else
    {
        g_ViChangeVideoModes[nextMode] = 0;
    }

    nextMode = (nextMode + 1) % NUM_VIDEO_FRAME_BUFFERS;
    D_800232C0 = nextMode;

    if (g_viColorOutputMode != COLORMODE_32BIT)
    {
        ((struct GfxInfo_s *)g_gfxTaskSettingsList)->cfb = g_ViBackData->framebuf;
    }
    else
    {
        ((struct GfxInfo_s *)g_gfxTaskSettingsList)->cfb = cfb_16[0];
    }

    settings = g_ViBackData;

    g_ViFrontIndex = (g_ViFrontIndex + 1) % NUM_VIDEO_SETTINGS;
    g_ViBackIndex = (g_ViBackIndex + 1) % NUM_VIDEO_SETTINGS;

    g_ViFrontData = g_ViDataArray + g_ViFrontIndex;
    g_ViBackData = g_ViDataArray + g_ViBackIndex;

    bcopy(settings, g_ViBackData, sizeof(VideoSettings));

    g_ViBackData->framebuf = cfb_16[g_ViBackIndex];
}


void viShake(f32 intensity)
{
    if (intensity > 14.0f)
    {
        intensity = 14.0f;
    }

    if (intensity < 0.0f)
    {
        intensity = 0.0f;
    }

    g_ViShakeIntensity = (s32)intensity;
    g_ViShakeTimer = 10;
}


void viSetVideoMode(s32 m)
{
    g_ViBackData->mode = m;
    g_ViBackData->x = g_ViBackData->bufx = g_viRuntimeScreenWidths[m];
    g_ViBackData->y = g_ViBackData->bufy = g_viRuntimeScreenHeights[m];
}


u8 *viGetFrameBuf2(void)
{
    return g_ViBackData->framebuf;
}


void viSetFrameBuf2(u8 *buf)
{
    g_ViBackData->framebuf = buf;
}


u16 viGetPerspNorm(void)
{
    return g_viPerspNorm;
}


Gfx *viSetupCurrentPlayerView(Gfx *gdl)
{
    if (g_CurrentPlayer != NULL)
    {
        g_CurrentPlayer->viewports[g_ViBackIndex].vp.vscale[0] = (g_ViBackData->viewx * 2);
        g_CurrentPlayer->viewports[g_ViBackIndex].vp.vtrans[0] = (g_ViBackData->viewx * 2) + (g_ViBackData->viewleft * 4);
        g_CurrentPlayer->viewports[g_ViBackIndex].vp.vscale[1] = (g_ViBackData->viewy * 2);
        g_CurrentPlayer->viewports[g_ViBackIndex].vp.vtrans[1] = (g_ViBackData->viewy * 2) + (g_ViBackData->viewtop * 4);
    }

    // Make the RSP's viewport transform the current player's screen rectangle.
    gSPViewport(gdl++, OS_K0_TO_PHYSICAL(&g_CurrentPlayer->viewports[g_ViBackIndex]));

    // Create both a floating-point matrix for the CPU side and a fixed-point matrix for the RSP.
    g_viProjectionMatrix = dynAllocateMatrix();
    guPerspectiveF(g_viProjectionMatrixF, &g_viPerspNorm, g_ViBackData->fovy, g_ViBackData->aspect, g_ViBackData->znear, g_ViBackData->zfar, 1.0f);
    guMtxF2L(g_viProjectionMatrixF, g_viProjectionMatrix);

    /** 
     * Load g_viProjectionMatrix as the current projection matrix.
     * All subsequent rendering uses this projection until a another projection matrix is loaded.
     */
    gSPMatrix(gdl++, OS_K0_TO_PHYSICAL(g_viProjectionMatrix), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION);
    gSPPerspNormalize(gdl++, g_viPerspNorm);

    // Store the float and non-float projection matrices so we can recall them later instead of having to rebuild them.
    camSetPlayerProjMtx(g_viProjectionMatrix);
    currentPlayerSetProjectionMatrixF(g_viProjectionMatrixF);

    // Normal rendering mode is a 16-bit RGBA image.
    if (g_viColorOutputMode != COLORMODE_32BIT)
    {
        gDPSetColorImage(gdl++, G_IM_FMT_RGBA, G_IM_SIZ_16b, g_ViBackData->bufx, OS_K0_TO_PHYSICAL(g_ViBackData->framebuf));
    }
    // 32-bit mode is only used for the 32-bit screenshot routines.
    else
    {
        gDPSetColorImage(gdl++, G_IM_FMT_RGBA, G_IM_SIZ_32b, g_ViBackData->bufx, OS_K0_TO_PHYSICAL(cfb_16[0]));
    }

    return gdl;
}


Gfx *viClearZBufCurrentPlayer(Gfx *gdl)
{
    if (g_ViBackData->usezbuf != 0)
    {
        gdl = zbufClearCurrentPlayer(zbufInit(gdl));
    }

    return gdl;
}


Gfx *viFillScreen(Gfx *gdl)
{
	gDPSetCycleType(gdl++, G_CYC_FILL);
	gDPFillRectangle(gdl++, 0, 0, (g_ViBackData->bufx - 1), (g_ViBackData->bufy - 1));
	gDPPipeSync(gdl++);

    return gdl;
}


Gfx *viSetupScreensForNumPlayers(Gfx *gdl)
{
    gDPSetCycleType(gdl++, G_CYC_FILL);
    gDPSetScissor(gdl++, G_SC_NON_INTERLACE, 0, 0, viGetX(), viGetY());
    gDPSetFillColor(gdl++, ((GPACK_RGBA5551(0,0,0,1) << 16) | GPACK_RGBA5551(0,0,0,1)));

    if (viGetViewTop() > 0)
    {
        if (get_player_position_in_shuffled(get_cur_playernum()) == 0)
        {
            gDPFillRectangle(gdl++, 0, 0, (viGetX() - 1), (viGetViewTop() - 1));
            gDPPipeSync(gdl++);
        }
    }

    if (getPlayerCount() == 1)
    {
        if ((viGetViewTop() + viGetViewHeight()) < viGetY())
        {
            gDPFillRectangle(gdl++, 0, (viGetViewTop() + viGetViewHeight()), (viGetX() - 1), (viGetY() - 1));
            gDPPipeSync(gdl++);
        }
    }
    else
    {
        if (get_player_position_in_shuffled(get_cur_playernum()) == 0)
        {
            if ((viGetViewTop() + viGetViewHeight() + viGetViewHeight()) < viGetY())
            {
                gDPFillRectangle(gdl++, 0, (viGetViewTop() + viGetViewHeight() + viGetViewHeight()), (viGetX() - 1), (viGetY() - 1));
                gDPPipeSync(gdl++);
            }

            gDPFillRectangle(gdl++, 0, ((SCREEN_HEIGHT / 2) - 1), (viGetX() - 1), (SCREEN_HEIGHT / 2));
            gDPPipeSync(gdl++);

            if (getPlayerCount() >= 3)
            {
                gDPFillRectangle(gdl++, ((SCREEN_WIDTH / 2) - 1), 0, (SCREEN_WIDTH / 2), (viGetY() - 1));
                gDPPipeSync(gdl++);
            }

            if (getPlayerCount() == 3)
            {
                gDPFillRectangle(gdl++, ((SCREEN_WIDTH / 2) + 1), ((SCREEN_HEIGHT / 2) + 1), (viGetX() - 1), (viGetY() - 1));
                gDPPipeSync(gdl++);
            }
        }
    }

    return gdl;
}


void viSetBuf(s16 x, s16 y)
{
    g_ViBackData->bufx = x;
    g_ViBackData->bufy = y;
}


void viSetXY(s16 x, s16 y)
{
    g_ViBackData->x = x;
    g_ViBackData->y = y;
}


s16 viGetX(void)
{
    return g_ViBackData->x;
}


s16 viGetY(void)
{
    return g_ViBackData->y;
}


void viSetViewSize(s16 x, s16 y)
{
    g_ViBackData->viewx = x;
    g_ViBackData->viewy = y;
    camSetPlayerScreenSize(g_ViBackData->viewx, g_ViBackData->viewy);
    camSetPlayerCameraScale();
}


s16 viGetViewWidth(void)
{
    return g_ViBackData->viewx;
}


s16 viGetViewHeight(void)
{
    return g_ViBackData->viewy;
}


void viSetViewPosition(s16 left, s16 top)
{
    g_ViBackData->viewleft = left;
    g_ViBackData->viewtop = top;
    camSetPlayerScreenPosition(g_ViBackData->viewleft, g_ViBackData->viewtop);
}


s16 viGetViewLeft(void)
{
    return g_ViBackData->viewleft;
}


s16 viGetViewTop(void)
{
    return g_ViBackData->viewtop;
}


void viSetUseZBuf(s32 usezbuf)
{
  g_ViBackData->usezbuf = usezbuf;
}


void viSetFovY(f32 fovy)
{
    g_ViBackData->fovy = fovy;
    camSetPlayerPerspective(g_ViBackData->znear, g_ViBackData->fovy, g_ViBackData->aspect);
    camSetPlayerCameraScale();
}


void viSetAspect(f32 aspect)
{
    g_ViBackData->aspect = aspect;
    camSetPlayerPerspective(g_ViBackData->znear, g_ViBackData->fovy, g_ViBackData->aspect);
    camSetPlayerCameraScale();
}


f32 viGetFovY(void)
{
    return g_ViBackData->fovy;
}


void viSetZRange(f32 near, f32 far)
{
    g_ViBackData->znear = near;
    g_ViBackData->zfar = far;
    camSetPlayerPerspective(g_ViBackData->znear, g_ViBackData->fovy, g_ViBackData->aspect);
    camSetPlayerCameraScale();
}


void viGetZRange(f32 *zrange)
{
    zrange[0] = g_ViBackData->znear;
    zrange[1] = g_ViBackData->zfar;
}


Gfx *viSetFillColor(Gfx *gdl, s32 r, s32 g, s32 b)
{
    if (g_viColorOutputMode != COLORMODE_32BIT)
    {
        gDPSetFillColor(gdl++, ((GPACK_RGBA5551(r, g, b, 1) << 16) | GPACK_RGBA5551(r, g, b, 1)));
    }
    else
    {
        gDPSetFillColor(gdl++, ((r << 24) | (g << 16) | (b << 8) | 0xFF));
    }

    return gdl;
}
