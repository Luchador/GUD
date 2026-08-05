#include <ultra64.h>
#include "vi.h"
#include "fr.h"
#include <bondgame.h>
#include <bondconstants.h>
#include "player.h" // g_CurrentPlayer
#include "sched.h"
#include "rsp.h"
#include <PR/os.h>
#include "indy_comms.h"
#include "game/bondview.h"
#include "game/viewport.h"
#include "game/dyn.h"
#ifdef ENABLE_USB
#include "usb.h"
#endif

/**
 * EU .data, offset from start of data_seg : 0x2484
*/

/**
 * @file video.c
 * This file contains video handling code.
 */

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

/**
 * Address 80023240.
 */
u32 g_unused80023240 = 0;

#if defined(VERSION_EU)
struct VideoSettings_s g_ViDataArray[NUM_VIDEO_SETTINGS] =
{
    {0, 0, 0, 0, 320, 272, 60.0f, 1.17647063732f, 30.0f, 10000.0f, SCREEN_WIDTH, SCREEN_HEIGHT, 320, 272, 0, 0, TRUE, NULL},
    {0, 0, 0, 0, 320, 272, 60.0f, 1.17647063732f, 30.0f, 10000.0f, SCREEN_WIDTH, SCREEN_HEIGHT, 320, 272, 0, 0, TRUE, NULL}
};
#else
struct VideoSettings_s g_ViDataArray[NUM_VIDEO_SETTINGS] =
{
    {MD_BLACK, 0, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, FOV_Y_F, ASPECT_RATIO, 30.0f, 10000.0f, SCREEN_WIDTH, SCREEN_HEIGHT, SCREEN_WIDTH, SCREEN_HEIGHT, 0, 0, TRUE, NULL},
    {MD_BLACK, 0, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, FOV_Y_F, ASPECT_RATIO, 30.0f, 10000.0f, SCREEN_WIDTH, SCREEN_HEIGHT, SCREEN_WIDTH, SCREEN_HEIGHT, 0, 0, TRUE, NULL}};
#endif

/**
 * Address 8002329C.
 * vimode hStart horizontal offset in video_related_8.
 */
s32 g_viHorizontalOffset = 0;

/**
 * Address 800232A0.
 * vimode vStart vertical offset in video_related_8.
 * (see g_ViShakeIntensity before changing the name)
 */
s32 D_800232A0 = 0;

VideoSettings *g_ViFrontData = &g_ViDataArray[0];
VideoSettings *g_ViBackData = &g_ViDataArray[0];
s32 g_viColorOutputMode = COLORMODE_16BIT;

/**
 * Address 800232B0.
 * g_ViShakeIntensity multiplier sign. This is only ever 1 or -1.
 */
s32 g_viVstartSign = 1;

/**
 * Address 800232B4.
 * vimode vStart vertical offset in viVsyncRelated.
 * (see D_800232A0 before changing the name)
 */
s32 g_ViShakeIntensity = 0;

/**
 * Address 800232B8.
 * Some kind of counter for how frequently g_ViShakeIntensity is zero'd in viVsyncRelated.
 */
u32 g_ViShakeTimer = 0;

/**
 * Address 800232BC.
 * Some kind of counter for how frequently osViBlack is called with "active" parameter.
 */
u32 g_ViUnblackTimer = 3;

/**
 * Address 800232C0.
 * should correlate to g_schedViCurrentFrameBuffer
 */
s32 D_800232C0 = 0;

#if defined(LEFTOVERDEBUG)
s32 g_indyJpg16BitGrabnum = 1;
s32 g_indyJpg32BitGrabnum = 1;
s32 g_indyRgb16BitGrabnum = 1;
s32 g_indyRgb32BitGrabnum = 1;
#endif

f32 g_viProjectionMatrixF[4][4];
Mtx *g_viProjectionMatrix;
u16 g_viPerspNorm;

/**
 * Address 80060828.
 * Original vi mode.
 * (name following n64devkit\ultra\usr\src\pr\demos\threadmonkey\block.c)
 */
OSViMode g_viOriginalVideoMode;

/**
 * Address 80060878.
 * cfb_16 index for g_ViFrontData.
 */
u8 g_ViFrontIndex;

/**
 * Address 80060879.
 * cfb_16 index for g_ViBackData and g_CurrentPlayer->viewports.
 */
u8 g_ViBackIndex;

/**
 * Current vi mode pointer.
 * EU .bss 0x80053E4C
 */
OSViMode *viMode;

/**
 * Address 80060880.
 * Original viMode->comRegs.hStart.
 * EU .bss 0x80053E50
 */
u32 g_viOriginalHstart;

/**
 * Address 80060884.
 * Original viMode->fldRegs[0].vStart.
 * EU .bss 0x80053E54
 */
u32 g_viOriginalVstart0;

/**
 * Address 80060888.
 * Original viMode->fldRegs[1].vStart.
 * EU .bss 0x80053E58
 */
u32 g_viOriginalVstart1;

s32 g_unused8006088C;

/**
 * Address 80028480.
 * Const runtime supported screen widths.
 */
const s16 g_viRuntimeScreenWidths[] = {SCREEN_WIDTH_MIN, SCREEN_WIDTH, SCREEN_WIDTH_MAX};

/**
 * Address 80028488.
 * Const runtime supported screen heights.
 */
#ifndef VERSION_EU
const s16 g_viRuntimeScreenHeights[] = {SCREEN_HEIGHT_MIN, SCREEN_HEIGHT, SCREEN_HEIGHT_MAX};
#else
const s16 g_viRuntimeScreenHeights[] = {SCREEN_HEIGHT_272, SCREEN_HEIGHT_272, SCREEN_HEIGHT_MAX_EU};
#endif

/**
 * 3C60	70003060
 */
void viInitVideoSettings(void)
{
    g_ViFrontIndex = 0;
    g_ViBackIndex = 1;

    g_ViFrontData = (VideoSettings*)((u8*)&g_ViDataArray + (g_ViFrontIndex * sizeof(VideoSettings))); //AKA front = viDataArray[0]
    g_ViFrontData->framebuf = cfb_16[g_ViFrontIndex];

    g_ViBackData = (VideoSettings*)((u8*)&g_ViDataArray + (g_ViBackIndex * sizeof(VideoSettings))); //AKA back = viDataArray[1] - was Rare mental?
    g_ViBackData->framebuf = cfb_16[g_ViBackIndex];

    g_viHorizontalOffset = 0;
    #ifndef VERSION_EU
    D_800232A0 = 0;
    #else
    D_800232A0 = 0xffffffe0;
    #endif
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
 * 3DA0	700031A0
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


#ifndef VERSION_EU
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
                if (D_800232C0);

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

            case MD_MAXIMUM:
                if (osTvType == TV_TYPE_MPAL)
                {
                    viMode = &osViModeTable[OS_VI_MPAL_HAF1];
                }
                else
                {
                    viMode = &osViModeTable[OS_VI_NTSC_HAF1];
                }

                g_viOriginalHstart = viMode->comRegs.hStart;
                g_viOriginalVstart0 = viMode->fldRegs[0].vStart;
                g_viOriginalVstart1 = viMode->fldRegs[1].vStart;

                osViBlack(FALSE);
                break;
        }

        osViSetSpecialFeatures(OS_VI_DITHER_FILTER_ON | OS_VI_GAMMA_OFF);
    }

    calculatedXScale = (f32)g_ViBackData->x / (f32)g_ViBackData->bufx;
    calculatedYScale = (f32)g_ViBackData->y / (f32)g_ViBackData->bufy;

    /* Keep these statements on the same logical source line for IDO scheduling. */
    if (g_ViBackData->mode == MD_BLACK) \
    { \
        calculatedYScale = 1.0f; \
    } nextMode = D_800232C0;

    yScaleMax = YSCALE_MAX;

    if (1);
    if (1);
    if (1);
    if (1);

    g_ViXScales[nextMode] = calculatedXScale;
    g_ViYScales[nextMode] = calculatedYScale;

    if (g_ViBackData->mode == VIDEOMODE_320x240)
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

        if (1)
        {
            g_ViModes[nextMode].fldRegs[0].yScale = (g_ViBackData->bufy * yScaleMax) / SCREEN_HEIGHT_MAX;
        }

        packedStart = g_ViModes[nextMode].comRegs.hStart;

        g_ViModes[nextMode].fldRegs[1].yScale = (g_ViBackData->bufy * yScaleMax) / SCREEN_HEIGHT_MAX;
        g_ViModes[nextMode].fldRegs[0].origin = g_ViBackData->bufx * 2;
        g_ViModes[nextMode].fldRegs[1].origin = g_ViBackData->bufx * 2;

        registerValue = packedStart;
        registerValue = ADD_LOW_AND_HI_16_MOD(registerValue, g_viHorizontalOffset);

        g_ViModes[nextMode].comRegs.hStart = registerValue;
        g_viOriginalHstart = registerValue;

        packedStart = g_ViModes[nextMode].fldRegs[0].vStart;
        registerValue = packedStart;
        registerValue = ADD_LOW_AND_HI_16_MOD(registerValue, D_800232A0);

        g_ViModes[nextMode].fldRegs[0].vStart = registerValue;
        g_viOriginalVstart0 = registerValue;

        packedStart = g_ViModes[nextMode].fldRegs[1].vStart;
        registerValue = packedStart;
        registerValue = ADD_LOW_AND_HI_16_MOD(registerValue, D_800232A0);

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
#endif
#ifdef VERSION_EU
GLOBAL_ASM(
.text
glabel video_related_8
/* 003A0C 70002E0C 3C078002 */  lui   $a3, %hi(g_ViBackData) # $a3, 0x8002
/* 003A10 70002E10 3C0E8002 */  lui   $t6, %hi(g_ViFrontData) # $t6, 0x8002
/* 003A14 70002E14 8DCE1144 */  lw    $t6, %lo(g_ViFrontData)($t6)
/* 003A18 70002E18 8CE71148 */  lw    $a3, %lo(g_ViBackData)($a3)
/* 003A1C 70002E1C 27BDFFE8 */  addiu $sp, $sp, -0x18
/* 003A20 70002E20 AFBF0014 */  sw    $ra, 0x14($sp)
/* 003A24 70002E24 91CF0000 */  lbu   $t7, ($t6)
/* 003A28 70002E28 90E30000 */  lbu   $v1, ($a3)
/* 003A2C 70002E2C 506F0060 */  beql  $v1, $t7, .L70002FB0
/* 003A30 70002E30 84EE0004 */   lh    $t6, 4($a3)
/* 003A34 70002E34 10600009 */  beqz  $v1, .L70002E5C
/* 003A38 70002E38 3C013F80 */   li    $at, 0x3F800000 # 1.000000
/* 003A3C 70002E3C 24010001 */  li    $at, 1
/* 003A40 70002E40 1061000D */  beq   $v1, $at, .L70002E78
/* 003A44 70002E44 3C188002 */   lui   $t8, %hi(g_viColorOutputMode) # $t8, 0x8002
/* 003A48 70002E48 24010002 */  li    $at, 2
/* 003A4C 70002E4C 10610043 */  beq   $v1, $at, .L70002F5C
/* 003A50 70002E50 3C0E8002 */   lui   $t6, 0x8002 # fixme?
/* 003A54 70002E54 10000050 */  b     .L70002F98
/* 003A58 70002E58 00000000 */   nop
.L70002E5C:
/* 003A5C 70002E5C 44816000 */  mtc1  $at, $f12
/* 003A60 70002E60 0C003300 */  jal   osViSetYScale
/* 003A64 70002E64 00000000 */   nop
/* 003A68 70002E68 0C003334 */  jal   osViBlack
/* 003A6C 70002E6C 24040001 */   li    $a0, 1
/* 003A70 70002E70 10000049 */  b     .L70002F98
/* 003A74 70002E74 00000000 */   nop
.L70002E78:
/* 003A78 70002E78 8F18114C */  lw    $t8, %lo(g_viColorOutputMode)($t8)
/* 003A7C 70002E7C 3C038005 */  lui   $v1, %hi(viMode) # $v1, 0x8005
/* 003A80 70002E80 24633E4C */  addiu $v1, %lo(viMode) # addiu $v1, $v1, 0x3e4c
/* 003A84 70002E84 13000014 */  beqz  $t8, .L70002ED8
/* 003A88 70002E88 3C0F8005 */   lui   $t7, %hi(g_viOriginalVideoMode) # $t7, 0x8005
/* 003A8C 70002E8C 3C0E8002 */  lui   $t6, %hi(osViModeTable + 1280) # $t6, 0x8002
/* 003A90 70002E90 25CE2930 */  addiu $t6, %lo(osViModeTable + 1280) # addiu $t6, $t6, 0x2930
/* 003A94 70002E94 3C198005 */  lui   $t9, %hi(g_viOriginalVideoMode) # $t9, 0x8005
/* 003A98 70002E98 27393DF8 */  addiu $t9, %lo(g_viOriginalVideoMode) # addiu $t9, $t9, 0x3df8
/* 003A9C 70002E9C 25D80048 */  addiu $t8, $t6, 0x48
.L70002EA0:
/* 003AA0 70002EA0 8DC10000 */  lw    $at, ($t6)
/* 003AA4 70002EA4 25CE000C */  addiu $t6, $t6, 0xc
/* 003AA8 70002EA8 2739000C */  addiu $t9, $t9, 0xc
/* 003AAC 70002EAC AF21FFF4 */  sw    $at, -0xc($t9)
/* 003AB0 70002EB0 8DC1FFF8 */  lw    $at, -8($t6)
/* 003AB4 70002EB4 AF21FFF8 */  sw    $at, -8($t9)
/* 003AB8 70002EB8 8DC1FFFC */  lw    $at, -4($t6)
/* 003ABC 70002EBC 15D8FFF8 */  bne   $t6, $t8, .L70002EA0
/* 003AC0 70002EC0 AF21FFFC */   sw    $at, -4($t9)
/* 003AC4 70002EC4 8DC10000 */  lw    $at, ($t6)
/* 003AC8 70002EC8 8DD80004 */  lw    $t8, 4($t6)
/* 003ACC 70002ECC AF210000 */  sw    $at, ($t9)
/* 003AD0 70002ED0 10000012 */  b     .L70002F1C
/* 003AD4 70002ED4 AF380004 */   sw    $t8, 4($t9)
.L70002ED8:
/* 003AD8 70002ED8 3C188002 */  lui   $t8, %hi(osViModeTable + 1600) # $t8, 0x8002
/* 003ADC 70002EDC 27182A70 */  addiu $t8, %lo(osViModeTable + 1600) # addiu $t8, $t8, 0x2a70
/* 003AE0 70002EE0 27190048 */  addiu $t9, $t8, 0x48
/* 003AE4 70002EE4 25EF3DF8 */  addiu $t7, %lo(g_viOriginalVideoMode) # addiu $t7, $t7, 0x3df8
.L70002EE8:
/* 003AE8 70002EE8 8F010000 */  lw    $at, ($t8)
/* 003AEC 70002EEC 2718000C */  addiu $t8, $t8, 0xc
/* 003AF0 70002EF0 25EF000C */  addiu $t7, $t7, 0xc
/* 003AF4 70002EF4 ADE1FFF4 */  sw    $at, -0xc($t7)
/* 003AF8 70002EF8 8F01FFF8 */  lw    $at, -8($t8)
/* 003AFC 70002EFC ADE1FFF8 */  sw    $at, -8($t7)
/* 003B00 70002F00 8F01FFFC */  lw    $at, -4($t8)
/* 003B04 70002F04 1719FFF8 */  bne   $t8, $t9, .L70002EE8
/* 003B08 70002F08 ADE1FFFC */   sw    $at, -4($t7)
/* 003B0C 70002F0C 8F010000 */  lw    $at, ($t8)
/* 003B10 70002F10 8F190004 */  lw    $t9, 4($t8)
/* 003B14 70002F14 ADE10000 */  sw    $at, ($t7)
/* 003B18 70002F18 ADF90004 */  sw    $t9, 4($t7)
.L70002F1C:
/* 003B1C 70002F1C 3C0E8005 */  lui   $t6, %hi(g_viOriginalVideoMode) # $t6, 0x8005
/* 003B20 70002F20 25CE3DF8 */  addiu $t6, %lo(g_viOriginalVideoMode) # addiu $t6, $t6, 0x3df8
/* 003B24 70002F24 AC6E0000 */  sw    $t6, ($v1)
/* 003B28 70002F28 8DD9001C */  lw    $t9, 0x1c($t6)
/* 003B2C 70002F2C 3C018005 */  lui   $at, %hi(viMode+0x4) # $at, 0x8005
/* 003B30 70002F30 00002025 */  move  $a0, $zero
/* 003B34 70002F34 AC393E50 */  sw    $t9, %lo(viMode+0x4)($at)
/* 003B38 70002F38 8DD80030 */  lw    $t8, 0x30($t6)
/* 003B3C 70002F3C 3C018005 */  lui   $at, %hi(viMode+0x8) # $at, 0x8005
/* 003B40 70002F40 AC383E54 */  sw    $t8, %lo(viMode+0x8)($at)
/* 003B44 70002F44 8DCF0044 */  lw    $t7, 0x44($t6)
/* 003B48 70002F48 3C018005 */  lui   $at, %hi(viMode+0xC) # $at, 0x8005
/* 003B4C 70002F4C 0C000B41 */  jal   frBlack
/* 003B50 70002F50 AC2F3E58 */   sw    $t7, %lo(viMode+0xC)($at)
/* 003B54 70002F54 10000010 */  b     .L70002F98
/* 003B58 70002F58 00000000 */   nop
.L70002F5C:
/* 003B5C 70002F5C 3C038005 */  lui   $v1, %hi(viMode) # $v1, 0x8005
/* 003B60 70002F60 24633E4C */  addiu $v1, %lo(viMode) # addiu $v1, $v1, 0x3e4c
/* 003B64 70002F64 25CE2C00 */  addiu $t6, $t6, 0x2c00
/* 003B68 70002F68 AC6E0000 */  sw    $t6, ($v1)
/* 003B6C 70002F6C 8DD9001C */  lw    $t9, 0x1c($t6)
/* 003B70 70002F70 3C018005 */  lui   $at, %hi(viMode+0x4) # $at, 0x8005
/* 003B74 70002F74 00002025 */  move  $a0, $zero
/* 003B78 70002F78 AC393E50 */  sw    $t9, %lo(viMode+0x4)($at)
/* 003B7C 70002F7C 8DD80030 */  lw    $t8, 0x30($t6)
/* 003B80 70002F80 3C018005 */  lui   $at, %hi(viMode+0x8) # $at, 0x8005
/* 003B84 70002F84 AC383E54 */  sw    $t8, %lo(viMode+0x8)($at)
/* 003B88 70002F88 8DCF0044 */  lw    $t7, 0x44($t6)
/* 003B8C 70002F8C 3C018005 */  lui   $at, %hi(viMode+0xC) # $at, 0x8005
/* 003B90 70002F90 0C003334 */  jal   osViBlack
/* 003B94 70002F94 AC2F3E58 */   sw    $t7, %lo(viMode+0xC)($at)
.L70002F98:
/* 003B98 70002F98 0C003BF8 */  jal   osViSetSpecialFeatures
/* 003B9C 70002F9C 24040042 */   li    $a0, 66
/* 003BA0 70002FA0 3C078002 */  lui   $a3, %hi(g_ViBackData) # $a3, 0x8002
/* 003BA4 70002FA4 8CE71148 */  lw    $a3, %lo(g_ViBackData)($a3)
/* 003BA8 70002FA8 90E30000 */  lbu   $v1, ($a3)
/* 003BAC 70002FAC 84EE0004 */  lh    $t6, 4($a3)
.L70002FB0:
/* 003BB0 70002FB0 84F90018 */  lh    $t9, 0x18($a3)
/* 003BB4 70002FB4 84F80006 */  lh    $t8, 6($a3)
/* 003BB8 70002FB8 448E2000 */  mtc1  $t6, $f4
/* 003BBC 70002FBC 84EF001A */  lh    $t7, 0x1a($a3)
/* 003BC0 70002FC0 44994000 */  mtc1  $t9, $f8
/* 003BC4 70002FC4 468021A0 */  cvt.s.w $f6, $f4
/* 003BC8 70002FC8 44988000 */  mtc1  $t8, $f16
/* 003BCC 70002FCC 448F2000 */  mtc1  $t7, $f4
/* 003BD0 70002FD0 3C198005 */  lui   $t9, %hi(g_viOriginalVideoMode) # $t9, 0x8005
/* 003BD4 70002FD4 3C1F8002 */  lui   $ra, %hi(D_800232C0) # $ra, 0x8002
/* 003BD8 70002FD8 468042A0 */  cvt.s.w $f10, $f8
/* 003BDC 70002FDC 3C013F80 */  li    $at, 0x3F800000 # 1.000000
/* 003BE0 70002FE0 27393DF8 */  addiu $t9, %lo(g_viOriginalVideoMode) # addiu $t9, $t9, 0x3df8
/* 003BE4 70002FE4 3C0D8002 */  lui   $t5, %hi(g_viColorOutputMode) # $t5, 0x8002
/* 003BE8 70002FE8 340AFFFF */  li    $t2, 65535
/* 003BEC 70002FEC 468084A0 */  cvt.s.w $f18, $f16
/* 003BF0 70002FF0 3C088002 */  lui   $t0, %hi(D_800232A0)
/* 003BF4 70002FF4 46802220 */  cvt.s.w $f8, $f4
/* 003BF8 70002FF8 460A3083 */  div.s $f2, $f6, $f10
/* 003BFC 70002FFC 14600003 */  bnez  $v1, .L7000300C
/* 003C00 70003000 46089003 */   div.s $f0, $f18, $f8
/* 003C04 70003004 44810000 */  mtc1  $at, $f0
/* 003C08 70003008 00000000 */  nop
.L7000300C:
/* 003C0C 7000300C 8FFF1160 */  lw    $ra, %lo(D_800232C0)($ra)
/* 003C10 70003010 3C018002 */  lui   $at, %hi(g_ViXScales)
/* 003C14 70003014 001F6080 */  sll   $t4, $ra, 2
/* 003C18 70003018 002C0821 */  addu  $at, $at, $t4
/* 003C1C 7000301C E4220F74 */  swc1  $f2, %lo(g_ViXScales)($at)
/* 003C20 70003020 3C018002 */  lui   $at, %hi(g_ViYScales)
/* 003C24 70003024 002C0821 */  addu  $at, $at, $t4
/* 003C28 70003028 E4200F7C */  swc1  $f0, %lo(g_ViYScales)($at)
/* 003C2C 7000302C 90EE0000 */  lbu   $t6, ($a3)
/* 003C30 70003030 24010001 */  li    $at, 1
/* 003C34 70003034 15C100F2 */  bne   $t6, $at, .L70003400
/* 003C38 70003038 00000000 */   nop
/* 003C3C 7000303C 8DAD114C */  lw    $t5, %lo(g_viColorOutputMode)($t5)
/* 003C40 70003040 3C018005 */  lui   $at, %hi(g_ViModePtrs)
/* 003C44 70003044 002C0821 */  addu  $at, $at, $t4
/* 003C48 70003048 11A00019 */  beqz  $t5, .L700030B0
/* 003C4C 7000304C AC391FA0 */   sw    $t9, %lo(g_ViModePtrs)($at)
/* 003C50 70003050 001FC080 */  sll   $t8, $ra, 2
/* 003C54 70003054 031FC021 */  addu  $t8, $t8, $ra
/* 003C58 70003058 3C0F8005 */  lui   $t7, %hi(g_ViModes) # $t7, 0x8005
/* 003C5C 7000305C 25EF1F00 */  addiu $t7, %lo(g_ViModes) # addiu $t7, $t7, 0x1f00
/* 003C60 70003060 0018C100 */  sll   $t8, $t8, 4
/* 003C64 70003064 030F2021 */  addu  $a0, $t8, $t7
/* 003C68 70003068 3C0E8002 */  lui   $t6, %hi(osViModeTable + 1280) # $t6, 0x8002
/* 003C6C 7000306C 25CE2930 */  addiu $t6, %lo(osViModeTable + 1280) # addiu $t6, $t6, 0x2930
/* 003C70 70003070 25D80048 */  addiu $t8, $t6, 0x48
/* 003C74 70003074 00807825 */  move  $t7, $a0
.L70003078:
/* 003C78 70003078 8DC10000 */  lw    $at, ($t6)
/* 003C7C 7000307C 25CE000C */  addiu $t6, $t6, 0xc
/* 003C80 70003080 25EF000C */  addiu $t7, $t7, 0xc
/* 003C84 70003084 ADE1FFF4 */  sw    $at, -0xc($t7)
/* 003C88 70003088 8DC1FFF8 */  lw    $at, -8($t6)
/* 003C8C 7000308C ADE1FFF8 */  sw    $at, -8($t7)
/* 003C90 70003090 8DC1FFFC */  lw    $at, -4($t6)
/* 003C94 70003094 15D8FFF8 */  bne   $t6, $t8, .L70003078
/* 003C98 70003098 ADE1FFFC */   sw    $at, -4($t7)
/* 003C9C 7000309C 8DC10000 */  lw    $at, ($t6)
/* 003CA0 700030A0 ADE10000 */  sw    $at, ($t7)
/* 003CA4 700030A4 8DD80004 */  lw    $t8, 4($t6)
/* 003CA8 700030A8 10000018 */  b     .L7000310C
/* 003CAC 700030AC ADF80004 */   sw    $t8, 4($t7)
.L700030B0:
/* 003CB0 700030B0 001FC880 */  sll   $t9, $ra, 2
/* 003CB4 700030B4 033FC821 */  addu  $t9, $t9, $ra
/* 003CB8 700030B8 3C188005 */  lui   $t8, %hi(g_ViModes) # $t8, 0x8005
/* 003CBC 700030BC 27181F00 */  addiu $t8, %lo(g_ViModes) # addiu $t8, $t8, 0x1f00
/* 003CC0 700030C0 0019C900 */  sll   $t9, $t9, 4
/* 003CC4 700030C4 03382021 */  addu  $a0, $t9, $t8
/* 003CC8 700030C8 3C0E8002 */  lui   $t6, %hi(osViModeTable + 1600) # $t6, 0x8002
/* 003CCC 700030CC 25CE2A70 */  addiu $t6, %lo(osViModeTable + 1600) # addiu $t6, $t6, 0x2a70
/* 003CD0 700030D0 25D90048 */  addiu $t9, $t6, 0x48
/* 003CD4 700030D4 0080C025 */  move  $t8, $a0
.L700030D8:
/* 003CD8 700030D8 8DC10000 */  lw    $at, ($t6)
/* 003CDC 700030DC 25CE000C */  addiu $t6, $t6, 0xc
/* 003CE0 700030E0 2718000C */  addiu $t8, $t8, 0xc
/* 003CE4 700030E4 AF01FFF4 */  sw    $at, -0xc($t8)
/* 003CE8 700030E8 8DC1FFF8 */  lw    $at, -8($t6)
/* 003CEC 700030EC AF01FFF8 */  sw    $at, -8($t8)
/* 003CF0 700030F0 8DC1FFFC */  lw    $at, -4($t6)
/* 003CF4 700030F4 15D9FFF8 */  bne   $t6, $t9, .L700030D8
/* 003CF8 700030F8 AF01FFFC */   sw    $at, -4($t8)
/* 003CFC 700030FC 8DC10000 */  lw    $at, ($t6)
/* 003D00 70003100 AF010000 */  sw    $at, ($t8)
/* 003D04 70003104 8DD90004 */  lw    $t9, 4($t6)
/* 003D08 70003108 AF190004 */  sw    $t9, 4($t8)
.L7000310C:
/* 003D0C 7000310C 84EF0018 */  lh    $t7, 0x18($a3)
/* 003D10 70003110 24010280 */  li    $at, 640
/* 003D14 70003114 240B014A */  li    $t3, 330
/* 003D18 70003118 AC8F0008 */  sw    $t7, 8($a0)
/* 003D1C 7000311C 84F90018 */  lh    $t9, 0x18($a3)
/* 003D20 70003120 00001025 */  move  $v0, $zero
/* 003D24 70003124 00004825 */  move  $t1, $zero
/* 003D28 70003128 00197280 */  sll   $t6, $t9, 0xa
/* 003D2C 7000312C 01C1001A */  div   $zero, $t6, $at
/* 003D30 70003130 0000C012 */  mflo  $t8
/* 003D34 70003134 AC980020 */  sw    $t8, 0x20($a0)
/* 003D38 70003138 84E5001A */  lh    $a1, 0x1a($a3)
/* 003D3C 7000313C 15650003 */  bne   $t3, $a1, .L7000314C
/* 003D40 70003140 00057AC0 */   sll   $t7, $a1, 0xb
/* 003D44 70003144 10000001 */  b     .L7000314C
/* 003D48 70003148 2402001C */   li    $v0, 28
.L7000314C:
/* 003D4C 7000314C 24590220 */  addiu $t9, $v0, 0x220
/* 003D50 70003150 01F9001A */  div   $zero, $t7, $t9
/* 003D54 70003154 00007012 */  mflo  $t6
/* 003D58 70003158 AC8E002C */  sw    $t6, 0x2c($a0)
/* 003D5C 7000315C 84E5001A */  lh    $a1, 0x1a($a3)
/* 003D60 70003160 17200002 */  bnez  $t9, .L7000316C
/* 003D64 70003164 00000000 */   nop
/* 003D68 70003168 0007000D */  break 7
.L7000316C:
/* 003D6C 7000316C 2401FFFF */  li    $at, -1
/* 003D70 70003170 17210004 */  bne   $t9, $at, .L70003184
/* 003D74 70003174 3C018000 */   lui   $at, 0x8000
/* 003D78 70003178 15E10002 */  bne   $t7, $at, .L70003184
/* 003D7C 7000317C 00000000 */   nop
/* 003D80 70003180 0006000D */  break 6
.L70003184:
/* 003D84 70003184 00001025 */  move  $v0, $zero
/* 003D88 70003188 15650003 */  bne   $t3, $a1, .L70003198
/* 003D8C 7000318C 0005C2C0 */   sll   $t8, $a1, 0xb
/* 003D90 70003190 10000001 */  b     .L70003198
/* 003D94 70003194 2402001C */   li    $v0, 28
.L70003198:
/* 003D98 70003198 244F0220 */  addiu $t7, $v0, 0x220
/* 003D9C 7000319C 030F001A */  div   $zero, $t8, $t7
/* 003DA0 700031A0 0000C812 */  mflo  $t9
/* 003DA4 700031A4 AC990040 */  sw    $t9, 0x40($a0)
/* 003DA8 700031A8 84EE0018 */  lh    $t6, 0x18($a3)
/* 003DAC 700031AC 15E00002 */  bnez  $t7, .L700031B8
/* 003DB0 700031B0 00000000 */   nop
/* 003DB4 700031B4 0007000D */  break 7
.L700031B8:
/* 003DB8 700031B8 2401FFFF */  li    $at, -1
/* 003DBC 700031BC 15E10004 */  bne   $t7, $at, .L700031D0
/* 003DC0 700031C0 3C018000 */   lui   $at, 0x8000
/* 003DC4 700031C4 17010002 */  bne   $t8, $at, .L700031D0
/* 003DC8 700031C8 00000000 */   nop
/* 003DCC 700031CC 0006000D */  break 6
.L700031D0:
/* 003DD0 700031D0 000EC040 */  sll   $t8, $t6, 1
/* 003DD4 700031D4 AC980028 */  sw    $t8, 0x28($a0)
/* 003DD8 700031D8 84EF0018 */  lh    $t7, 0x18($a3)
/* 003DDC 700031DC 8C86001C */  lw    $a2, 0x1c($a0)
/* 003DE0 700031E0 3C038002 */  lui   $v1, %hi(g_viHorizontalOffset) # $v1, 0x8002
/* 003DE4 700031E4 000FC840 */  sll   $t9, $t7, 1
/* 003DE8 700031E8 AC99003C */  sw    $t9, 0x3c($a0)
/* 003DEC 700031EC 8C63113C */  lw    $v1, %lo(g_viHorizontalOffset)($v1)
/* 003DF0 700031F0 00067403 */  sra   $t6, $a2, 0x10
/* 003DF4 700031F4 31D8FFFF */  andi  $t8, $t6, 0xffff
/* 003DF8 700031F8 03037821 */  addu  $t7, $t8, $v1
/* 003DFC 700031FC 01EA001A */  div   $zero, $t7, $t2
/* 003E00 70003200 15400002 */  bnez  $t2, .L7000320C
/* 003E04 70003204 00000000 */   nop
/* 003E08 70003208 0007000D */  break 7
.L7000320C:
/* 003E0C 7000320C 2401FFFF */  li    $at, -1
/* 003E10 70003210 15410004 */  bne   $t2, $at, .L70003224
/* 003E14 70003214 3C018000 */   lui   $at, 0x8000
/* 003E18 70003218 15E10002 */  bne   $t7, $at, .L70003224
/* 003E1C 7000321C 00000000 */   nop
/* 003E20 70003220 0006000D */  break 6
.L70003224:
/* 003E24 70003224 30D8FFFF */  andi  $t8, $a2, 0xffff
/* 003E28 70003228 0000C810 */  mfhi  $t9
/* 003E2C 7000322C 03037821 */  addu  $t7, $t8, $v1
/* 003E30 70003230 00197400 */  sll   $t6, $t9, 0x10
/* 003E34 70003234 01EA001A */  div   $zero, $t7, $t2
/* 003E38 70003238 15400002 */  bnez  $t2, .L70003244
/* 003E3C 7000323C 00000000 */   nop
/* 003E40 70003240 0007000D */  break 7
.L70003244:
/* 003E44 70003244 2401FFFF */  li    $at, -1
/* 003E48 70003248 15410004 */  bne   $t2, $at, .L7000325C
/* 003E4C 7000324C 3C018000 */   lui   $at, 0x8000
/* 003E50 70003250 15E10002 */  bne   $t7, $at, .L7000325C
/* 003E54 70003254 00000000 */   nop
/* 003E58 70003258 0006000D */  break 6
.L7000325C:
/* 003E5C 7000325C 0000C810 */  mfhi  $t9
/* 003E60 70003260 01D91025 */  or    $v0, $t6, $t9
/* 003E64 70003264 AC82001C */  sw    $v0, 0x1c($a0)
/* 003E68 70003268 3C018005 */  lui   $at, %hi(viMode+0x4) # $at, 0x8005
/* 003E6C 7000326C AC223E50 */  sw    $v0, %lo(viMode+0x4)($at)
/* 003E70 70003270 84E5001A */  lh    $a1, 0x1a($a3)
/* 003E74 70003274 8C860030 */  lw    $a2, 0x30($a0)
/* 003E78 70003278 00001825 */  move  $v1, $zero
/* 003E7C 7000327C 15650003 */  bne   $t3, $a1, .L7000328C
/* 003E80 70003280 00000000 */   nop
/* 003E84 70003284 10000001 */  b     .L7000328C
/* 003E88 70003288 2409FFF2 */   li    $t1, -14
.L7000328C:
/* 003E8C 7000328C 15650003 */  bne   $t3, $a1, .L7000329C
/* 003E90 70003290 00000000 */   nop
/* 003E94 70003294 10000001 */  b     .L7000329C
/* 003E98 70003298 2403000E */   li    $v1, 14
.L7000329C:
/* 003E9C 7000329C 8D081140 */  lw    $t0, %lo(D_800232A0)($t0)
/* 003EA0 700032A0 30D8FFFF */  andi  $t8, $a2, 0xffff
/* 003EA4 700032A4 00787821 */  addu  $t7, $v1, $t8
/* 003EA8 700032A8 01E87021 */  addu  $t6, $t7, $t0
/* 003EAC 700032AC 25D90040 */  addiu $t9, $t6, 0x40
/* 003EB0 700032B0 032A001A */  div   $zero, $t9, $t2
/* 003EB4 700032B4 00067C03 */  sra   $t7, $a2, 0x10
/* 003EB8 700032B8 15400002 */  bnez  $t2, .L700032C4
/* 003EBC 700032BC 00000000 */   nop
/* 003EC0 700032C0 0007000D */  break 7
.L700032C4:
/* 003EC4 700032C4 2401FFFF */  li    $at, -1
/* 003EC8 700032C8 15410004 */  bne   $t2, $at, .L700032DC
/* 003ECC 700032CC 3C018000 */   lui   $at, 0x8000
/* 003ED0 700032D0 17210002 */  bne   $t9, $at, .L700032DC
/* 003ED4 700032D4 00000000 */   nop
/* 003ED8 700032D8 0006000D */  break 6
.L700032DC:
/* 003EDC 700032DC 31EEFFFF */  andi  $t6, $t7, 0xffff
/* 003EE0 700032E0 01C8C821 */  addu  $t9, $t6, $t0
/* 003EE4 700032E4 03297821 */  addu  $t7, $t9, $t1
/* 003EE8 700032E8 0000C010 */  mfhi  $t8
/* 003EEC 700032EC 00004825 */  move  $t1, $zero
/* 003EF0 700032F0 00001825 */  move  $v1, $zero
/* 003EF4 700032F4 01EA001A */  div   $zero, $t7, $t2
/* 003EF8 700032F8 15400002 */  bnez  $t2, .L70003304
/* 003EFC 700032FC 00000000 */   nop
/* 003F00 70003300 0007000D */  break 7
.L70003304:
/* 003F04 70003304 2401FFFF */  li    $at, -1
/* 003F08 70003308 15410004 */  bne   $t2, $at, .L7000331C
/* 003F0C 7000330C 3C018000 */   lui   $at, 0x8000
/* 003F10 70003310 15E10002 */  bne   $t7, $at, .L7000331C
/* 003F14 70003314 00000000 */   nop
/* 003F18 70003318 0006000D */  break 6
.L7000331C:
/* 003F1C 7000331C 00007010 */  mfhi  $t6
/* 003F20 70003320 000ECC00 */  sll   $t9, $t6, 0x10
/* 003F24 70003324 03191025 */  or    $v0, $t8, $t9
/* 003F28 70003328 AC820030 */  sw    $v0, 0x30($a0)
/* 003F2C 7000332C 3C018005 */  lui   $at, %hi(viMode+0x8) # $at, 0x8005
/* 003F30 70003330 AC223E54 */  sw    $v0, %lo(viMode+0x8)($at)
/* 003F34 70003334 84E5001A */  lh    $a1, 0x1a($a3)
/* 003F38 70003338 8C860044 */  lw    $a2, 0x44($a0)
/* 003F3C 7000333C 15650003 */  bne   $t3, $a1, .L7000334C
/* 003F40 70003340 30CFFFFF */   andi  $t7, $a2, 0xffff
/* 003F44 70003344 10000001 */  b     .L7000334C
/* 003F48 70003348 2409FFF2 */   li    $t1, -14
.L7000334C:
/* 003F4C 7000334C 15650003 */  bne   $t3, $a1, .L7000335C
/* 003F50 70003350 00000000 */   nop
/* 003F54 70003354 10000001 */  b     .L7000335C
/* 003F58 70003358 2403000E */   li    $v1, 14
.L7000335C:
/* 003F5C 7000335C 006F7021 */  addu  $t6, $v1, $t7
/* 003F60 70003360 01C8C021 */  addu  $t8, $t6, $t0
/* 003F64 70003364 27190040 */  addiu $t9, $t8, 0x40
/* 003F68 70003368 032A001A */  div   $zero, $t9, $t2
/* 003F6C 7000336C 00067403 */  sra   $t6, $a2, 0x10
/* 003F70 70003370 15400002 */  bnez  $t2, .L7000337C
/* 003F74 70003374 00000000 */   nop
/* 003F78 70003378 0007000D */  break 7
.L7000337C:
/* 003F7C 7000337C 2401FFFF */  li    $at, -1
/* 003F80 70003380 15410004 */  bne   $t2, $at, .L70003394
/* 003F84 70003384 3C018000 */   lui   $at, 0x8000
/* 003F88 70003388 17210002 */  bne   $t9, $at, .L70003394
/* 003F8C 7000338C 00000000 */   nop
/* 003F90 70003390 0006000D */  break 6
.L70003394:
/* 003F94 70003394 31D8FFFF */  andi  $t8, $t6, 0xffff
/* 003F98 70003398 0308C821 */  addu  $t9, $t8, $t0
/* 003F9C 7000339C 03297021 */  addu  $t6, $t9, $t1
/* 003FA0 700033A0 00007810 */  mfhi  $t7
/* 003FA4 700033A4 00000000 */  nop
/* 003FA8 700033A8 00000000 */  nop
/* 003FAC 700033AC 01CA001A */  div   $zero, $t6, $t2
/* 003FB0 700033B0 15400002 */  bnez  $t2, .L700033BC
/* 003FB4 700033B4 00000000 */   nop
/* 003FB8 700033B8 0007000D */  break 7
.L700033BC:
/* 003FBC 700033BC 2401FFFF */  li    $at, -1
/* 003FC0 700033C0 15410004 */  bne   $t2, $at, .L700033D4
/* 003FC4 700033C4 3C018000 */   lui   $at, 0x8000
/* 003FC8 700033C8 15C10002 */  bne   $t6, $at, .L700033D4
/* 003FCC 700033CC 00000000 */   nop
/* 003FD0 700033D0 0006000D */  break 6
.L700033D4:
/* 003FD4 700033D4 0000C010 */  mfhi  $t8
/* 003FD8 700033D8 0018CC00 */  sll   $t9, $t8, 0x10
/* 003FDC 700033DC 01F91025 */  or    $v0, $t7, $t9
/* 003FE0 700033E0 AC820044 */  sw    $v0, 0x44($a0)
/* 003FE4 700033E4 3C018005 */  lui   $at, %hi(viMode+0xC) # $at, 0x8005
/* 003FE8 700033E8 AC223E58 */  sw    $v0, %lo(viMode+0xC)($at)
/* 003FEC 700033EC 3C018002 */  lui   $at, %hi(g_ViChangeVideoModes)
/* 003FF0 700033F0 002C0821 */  addu  $at, $at, $t4
/* 003FF4 700033F4 240E0001 */  li    $t6, 1
/* 003FF8 700033F8 10000006 */  b     .L70003414
/* 003FFC 700033FC AC2E0F84 */   sw    $t6, %lo(g_ViChangeVideoModes)($at)
.L70003400:
/* 004000 70003400 3C018002 */  lui   $at, %hi(g_ViChangeVideoModes)
/* 004004 70003404 002C0821 */  addu  $at, $at, $t4
/* 004008 70003408 3C0D8002 */  lui   $t5, %hi(g_viColorOutputMode) # $t5, 0x8002
/* 00400C 7000340C 8DAD114C */  lw    $t5, %lo(g_viColorOutputMode)($t5)
/* 004010 70003410 AC200F84 */  sw    $zero, %lo(g_ViChangeVideoModes)($at)
.L70003414:
/* 004014 70003414 27FF0001 */  addiu $ra, $ra, 1
/* 004018 70003418 07E10004 */  bgez  $ra, .L7000342C
/* 00401C 7000341C 33F80001 */   andi  $t8, $ra, 1
/* 004020 70003420 13000002 */  beqz  $t8, .L7000342C
/* 004024 70003424 00000000 */   nop
/* 004028 70003428 2718FFFE */  addiu $t8, $t8, -2
.L7000342C:
/* 00402C 7000342C 3C018002 */  lui   $at, %hi(D_800232C0) # $at, 0x8002
/* 004030 70003430 11A00006 */  beqz  $t5, .L7000344C
/* 004034 70003434 AC381160 */   sw    $t8, %lo(D_800232C0)($at)
/* 004038 70003438 3C198004 */  lui   $t9, %hi(g_gfxTaskSettingsList) # $t9, 0x8004
/* 00403C 7000343C 8F397030 */  lw    $t9, %lo(g_gfxTaskSettingsList)($t9)
/* 004040 70003440 8CEF0028 */  lw    $t7, 0x28($a3)
/* 004044 70003444 10000006 */  b     .L70003460
/* 004048 70003448 AF2F0058 */   sw    $t7, 0x58($t9)
.L7000344C:
/* 00404C 7000344C 3C188004 */  lui   $t8, %hi(g_gfxTaskSettingsList) # $t8, 0x8004
/* 004050 70003450 8F187030 */  lw    $t8, %lo(g_gfxTaskSettingsList)($t8)
/* 004054 70003454 3C0E803B */  lui   $t6, %hi(cfb_16) # $t6, 0x803b
/* 004058 70003458 25CEB000 */  addiu $t6, %lo(cfb_16) # addiu $t6, $t6, -0x5000
/* 00405C 7000345C AF0E0058 */  sw    $t6, 0x58($t8)
.L70003460:
/* 004060 70003460 3C028005 */  lui   $v0, %hi(g_ViFrontIndex) # $v0, 0x8005
/* 004064 70003464 24423E48 */  addiu $v0, %lo(g_ViFrontIndex) # addiu $v0, $v0, 0x3e48
/* 004068 70003468 904F0000 */  lbu   $t7, ($v0)
/* 00406C 7000346C 3C048002 */  lui   $a0, %hi(g_ViBackData) # $a0, 0x8002
/* 004070 70003470 8C841148 */  lw    $a0, %lo(g_ViBackData)($a0)
/* 004074 70003474 25F90001 */  addiu $t9, $t7, 1
/* 004078 70003478 3C038005 */  lui   $v1, %hi(g_ViBackIndex) # $v1, 0x8005
/* 00407C 7000347C 2408002C */  li    $t0, 44
/* 004080 70003480 24633E49 */  addiu $v1, %lo(g_ViBackIndex) # addiu $v1, $v1, 0x3e49
/* 004084 70003484 90780000 */  lbu   $t8, ($v1)
/* 004088 70003488 3C078002 */  lui   $a3, %hi(g_ViDataArray) # $a3, 0x8002
/* 00408C 7000348C 24E710E4 */  addiu $a3, %lo(g_ViDataArray) # addiu $a3, $a3, 0x10e4
/* 004090 70003490 07210004 */  bgez  $t9, .L700034A4
/* 004094 70003494 332E0001 */   andi  $t6, $t9, 1
/* 004098 70003498 11C00002 */  beqz  $t6, .L700034A4
/* 00409C 7000349C 00000000 */   nop
/* 0040A0 700034A0 25CEFFFE */  addiu $t6, $t6, -2
.L700034A4:
/* 0040A4 700034A4 A04E0000 */  sb    $t6, ($v0)
/* 0040A8 700034A8 904E0000 */  lbu   $t6, ($v0)
/* 0040AC 700034AC 270F0001 */  addiu $t7, $t8, 1
/* 0040B0 700034B0 05E10004 */  bgez  $t7, .L700034C4
/* 0040B4 700034B4 31F90001 */   andi  $t9, $t7, 1
/* 0040B8 700034B8 13200002 */  beqz  $t9, .L700034C4
/* 0040BC 700034BC 00000000 */   nop
/* 0040C0 700034C0 2739FFFE */  addiu $t9, $t9, -2
.L700034C4:
/* 0040C4 700034C4 01C80019 */  multu $t6, $t0
/* 0040C8 700034C8 A0790000 */  sb    $t9, ($v1)
/* 0040CC 700034CC 3C018002 */  lui   $at, %hi(g_ViFrontData) # $at, 0x8002
/* 0040D0 700034D0 3C058002 */  lui   $a1, %hi(g_ViBackData) # $a1, 0x8002
/* 0040D4 700034D4 2406002C */  li    $a2, 44
/* 0040D8 700034D8 0000C012 */  mflo  $t8
/* 0040DC 700034DC 00F87821 */  addu  $t7, $a3, $t8
/* 0040E0 700034E0 AC2F1144 */  sw    $t7, %lo(g_ViFrontData)($at)
/* 0040E4 700034E4 90790000 */  lbu   $t9, ($v1)
/* 0040E8 700034E8 3C018002 */  lui   $at, %hi(g_ViBackData) # $at, 0x8002
/* 0040EC 700034EC 03280019 */  multu $t9, $t0
/* 0040F0 700034F0 00007012 */  mflo  $t6
/* 0040F4 700034F4 00EEC021 */  addu  $t8, $a3, $t6
/* 0040F8 700034F8 AC381148 */  sw    $t8, %lo(g_ViBackData)($at)
/* 0040FC 700034FC 0C003C68 */  jal   bcopy
/* 004100 70003500 8CA51148 */   lw    $a1, %lo(g_ViBackData)($a1)
/* 004104 70003504 3C038005 */  lui   $v1, %hi(g_ViBackIndex) # $v1, 0x8005
/* 004108 70003508 24633E49 */  addiu $v1, %lo(g_ViBackIndex) # addiu $v1, $v1, 0x3e49
/* 00410C 7000350C 906F0000 */  lbu   $t7, ($v1)
/* 004110 70003510 3C0E803B */  lui   $t6, %hi(cfb_16) # $t6, 0x803b
/* 004114 70003514 25CEB000 */  addiu $t6, %lo(cfb_16) # addiu $t6, $t6, -0x5000
/* 004118 70003518 000FC880 */  sll   $t9, $t7, 2
/* 00411C 7000351C 032FC821 */  addu  $t9, $t9, $t7
/* 004120 70003520 0019C880 */  sll   $t9, $t9, 2
/* 004124 70003524 032FC821 */  addu  $t9, $t9, $t7
/* 004128 70003528 0019C880 */  sll   $t9, $t9, 2
/* 00412C 7000352C 032FC821 */  addu  $t9, $t9, $t7
/* 004130 70003530 3C0F8002 */  lui   $t7, %hi(g_ViBackData) # $t7, 0x8002
/* 004134 70003534 8DEF1148 */  lw    $t7, %lo(g_ViBackData)($t7)
/* 004138 70003538 0019CAC0 */  sll   $t9, $t9, 0xb
/* 00413C 7000353C 032EC021 */  addu  $t8, $t9, $t6
/* 004140 70003540 ADF80028 */  sw    $t8, 0x28($t7)
/* 004144 70003544 8FBF0014 */  lw    $ra, 0x14($sp)
/* 004148 70003548 27BD0018 */  addiu $sp, $sp, 0x18
/* 00414C 7000354C 03E00008 */  jr    $ra
/* 004150 70003550 00000000 */   nop
)
#endif


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

void viRecvMesg(int count)
{
    do
    {
        osRecvMesg(&vi_c_debug_MQ, NULL, 1);
        count--;
    }
    while (count > 0);
}

void viSetVideoMode(s32 m)
{
    //if (m < 0) {
    //    assertPrint_8291E690(".\\ported\\fr.cpp",0x22a,"Assertion failed: m>=MD_BLACK");
    //}
    //if (2 < iStack00000014) {
    //    assertPrint_8291E690(".\\ported\\fr.cpp",0x22b,"Assertion failed: m<MD_MAXIMUM");
    //}
    g_ViBackData->mode = m;
    g_ViBackData->x = g_ViBackData->bufx = g_viRuntimeScreenWidths[m];
    g_ViBackData->y = g_ViBackData->bufy = g_viRuntimeScreenHeights[m];
}

void viSetColorMode16Bit(void)
{
    g_viColorOutputMode = COLORMODE_16BIT;
}

void viSetColorMode32Bit(void)
{
    g_viColorOutputMode = COLORMODE_32BIT;
}

u8 *viGetFrameBuf2(void)
{
    return g_ViBackData->framebuf;
}

u8 *viGetFrameBuf1(void)
{
    return g_ViFrontData->framebuf;
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
    currentPlayerSetProjectionMatrix(g_viProjectionMatrix);
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

void viSet800232A0(s32 arg0)
{
    D_800232A0 = arg0;
}

s32 viGet800232A0(void)
{
    return D_800232A0;
}

void viSetHorizontalOffset(s32 arg0)
{
    g_viHorizontalOffset = arg0;
}

s32 viGetHorizontalOffset(void)
{
    return g_viHorizontalOffset;
}

void viSetBuf(s16 x, s16 y)
{
    g_ViBackData->bufx = x;
    g_ViBackData->bufy = y;
}

s16 viGetBufX(void)
{
    return g_ViBackData->bufx;
}

s16 viGetBufY(void)
{
    return g_ViBackData->bufy;
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
    currentPlayerSetScreenSize(g_ViBackData->viewx, g_ViBackData->viewy);
    currentPlayerSetCameraScale();
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
    currentPlayerSetScreenPosition(g_ViBackData->viewleft, g_ViBackData->viewtop);
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
    currentPlayerSetPerspective(g_ViBackData->znear, g_ViBackData->fovy, g_ViBackData->aspect);
    currentPlayerSetCameraScale();
}

void viSetAspect(f32 aspect)
{
    g_ViBackData->aspect = aspect;
    currentPlayerSetPerspective(g_ViBackData->znear, g_ViBackData->fovy, g_ViBackData->aspect);
    currentPlayerSetCameraScale();
}

f32 viGetFovY(void)
{
    return g_ViBackData->fovy;
}

void viSetFov(f32 fovx, f32 fovy)
{
    g_ViBackData->fovy = fovy;
    g_ViBackData->aspect = (f32) (fovx / fovy);
    currentPlayerSetPerspective(g_ViBackData->znear, g_ViBackData->fovy, g_ViBackData->aspect);
    currentPlayerSetCameraScale();
}

void viSetZRange(f32 near, f32 far)
{
    g_ViBackData->znear = near;
    g_ViBackData->zfar = far;
    currentPlayerSetPerspective(g_ViBackData->znear, g_ViBackData->fovy, g_ViBackData->aspect);
    currentPlayerSetCameraScale();
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

/**
 * 5358	70004758
 *     image capture routine, jpeg 16bit colour
 */
void indyGrabJpg16bit(void)
{
#ifdef LEFTOVERDEBUG
    s32 *pgrabnum = &g_indyJpg16BitGrabnum;
    char buffer[250];
    s32 filesize;

    while (1)
    {
        sprintf(buffer, "grab.%d.jpeg", *pgrabnum);
        if (indycommHostCheckFileExists((u8*)&buffer, &filesize) == 0)
        {
            break;
        }

        *pgrabnum = *pgrabnum + 1;
    }

    sprintf(buffer, "grab.%d.temp.uix", *pgrabnum);
    indycommHostSendDump((u8*)&buffer, (u8*)g_ViBackData->framebuf, (viGetX() * viGetY() * 2));

    sprintf(buffer, "uix2pix grab.%d.temp.uix", *pgrabnum);
    indycommHostSendCmd((u8*)&buffer);

    sprintf(buffer, "fromalias grab.%d.temp.pix grab.%d.temp.rgb", *pgrabnum, *pgrabnum);
    indycommHostSendCmd((u8*)&buffer);

    sprintf(buffer, "imgcopy -fjfif grab.%d.temp.rgb grab.%d.jpeg", *pgrabnum, *pgrabnum);
    indycommHostSendCmd((u8*)&buffer);

    sprintf(buffer, "rm grab.%d.temp.uix grab.%d.temp.pix grab.%d.temp.rgb", *pgrabnum, *pgrabnum, *pgrabnum);
    indycommHostSendCmd((u8*)&buffer);

    sprintf(buffer, "imgview grab.%d.jpeg", *pgrabnum);
    indycommHostSendCmd((u8*)&buffer);
#endif
}

/**
 * 54D4	700048D4
 *     image capture routine, jpeg 32bit colour
 */
void indyGrabJpg32bit(void)
{
    #if defined(LEFTOVERDEBUG)
    s32 *pgrabnum = &g_indyJpg32BitGrabnum;
    char buffer[250];
    s32 filesize;

    while (1)
    {
        sprintf(buffer, "grab.%d.jpeg", *pgrabnum);
        if (indycommHostCheckFileExists((u8*)&buffer, &filesize) == 0)
        {
            break;
        }

        *pgrabnum = *pgrabnum + 1;
    }

    sprintf(buffer, "grab.%d.temp.Uix", *pgrabnum);
    indycommHostSendDump((u8*)&buffer, (u8*)&cfb_16, (viGetX() * viGetY() * 4));

    sprintf(buffer, "Uix2pix -xs%d grab.%d.temp.Uix", viGetX(), *pgrabnum);
    indycommHostSendCmd((u8*)&buffer);

    sprintf(buffer, "fromalias grab.%d.temp.pix grab.%d.temp.rgb", *pgrabnum, *pgrabnum);
    indycommHostSendCmd((u8*)&buffer);

    sprintf(buffer, "imgcopy -fjfif grab.%d.temp.rgb grab.%d.jpeg", *pgrabnum, *pgrabnum);
    indycommHostSendCmd((u8*)&buffer);

    sprintf(buffer, "rm grab.%d.temp.Uix grab.%d.temp.pix grab.%d.temp.rgb", *pgrabnum, *pgrabnum, *pgrabnum);
    indycommHostSendCmd((u8*)&buffer);

    sprintf(buffer, "imgview grab.%d.jpeg", *pgrabnum);
    indycommHostSendCmd((u8*)&buffer);
    #endif
}

/**
 * 5660	70004A60
 *     image capture routine, rgb 16bit colour
 */
void indyGrabRgb16bit(void)
{
    #if defined(LEFTOVERDEBUG)
    s32 *pgrabnum = &g_indyRgb16BitGrabnum;
    char buffer[250];
    s32 filesize;

    while (1)
    {
        sprintf(buffer, "grab.%d.rgb", *pgrabnum);
        if (indycommHostCheckFileExists((u8*)&buffer, &filesize) == 0)
        {
            break;
        }

        *pgrabnum = *pgrabnum + 1;
    }

    sprintf(buffer, "grab.%d.temp.uix", *pgrabnum);
    indycommHostSendDump((u8*)&buffer, (u8*)g_ViBackData->framebuf, (viGetX() * viGetY() * 2));

    sprintf(buffer, "uix2pix grab.%d.temp.uix", *pgrabnum);
    indycommHostSendCmd((u8*)&buffer);

    sprintf(buffer, "fromalias grab.%d.temp.pix grab.%d.rgb", *pgrabnum, *pgrabnum);
    indycommHostSendCmd((u8*)&buffer);

    sprintf(buffer, "rm grab.%d.temp.uix grab.%d.temp.pix", *pgrabnum, *pgrabnum);
    indycommHostSendCmd((u8*)&buffer);

    sprintf(buffer, "imgview grab.%d.rgb", *pgrabnum);
    indycommHostSendCmd((u8*)&buffer);
#endif
}

/**
 * 57B4	70004BB4
 *     image capture routine, rgb 32bit colour
 */
void indyGrabRgb32bit(void)
{
    #if defined(LEFTOVERDEBUG)
    s32 *pgrabnum = &g_indyRgb32BitGrabnum;
    char buffer[250];
    s32 filesize;

    while (1)
    {
        sprintf(buffer, "grab.%d.rgb", *pgrabnum);
        if (indycommHostCheckFileExists((u8*)&buffer, &filesize) == 0)
        {
            break;
        }

        *pgrabnum = *pgrabnum + 1;
    }

    sprintf(buffer, "grab.%d.temp.Uix", *pgrabnum);
    indycommHostSendDump((u8*)&buffer, (u8*)&cfb_16, (viGetX() * viGetY() * 4));

    sprintf(buffer, "Uix2pix -xs%d grab.%d.temp.Uix", viGetX(), *pgrabnum);
    indycommHostSendCmd((u8*)&buffer);

    sprintf(buffer, "fromalias grab.%d.temp.pix grab.%d.rgb", *pgrabnum, *pgrabnum);
    indycommHostSendCmd((u8*)&buffer);

    sprintf(buffer, "rm grab.%d.temp.Uix grab.%d.temp.pix", *pgrabnum, *pgrabnum);
    indycommHostSendCmd((u8*)&buffer);

    sprintf(buffer, "imgview grab.%d.rgb", *pgrabnum);
    indycommHostSendCmd((u8*)&buffer);
    #endif
}
