#include <ultra64.h>
#include "lv.h"
#include "unk_092E50.h"

// bss
//CODE.bss:80079E80
f32 flt_CODE_bss_80079E80;
//CODE.bss:80079E84
f32 flt_CODE_bss_80079E84;
//CODE.bss:80079E88
f32 flt_CODE_bss_80079E88;


// data
//D:8003FCC0
Gfx MipMap2C_Something_Setup[] = {
    gsDPSetTile(G_IM_FMT_I, G_IM_SIZ_4b, 4, 0, 0, 0, G_TX_WRAP, 6, 0, G_TX_WRAP, 6, 0),
    gsDPSetTile(G_IM_FMT_I, G_IM_SIZ_4b, 4, 0, 1, 0, G_TX_WRAP, 6, 0, G_TX_WRAP, 6, 0),
    gsDPSetTileSize(0, 2, 2, 0, 0),
    gsDPSetTileSize(1, 2, 2, 0, 0),
    gsDPSetPrimColor(0, 15, 255, 255, 255, 255),
    gsDPSetTextureDetail(G_TD_CLAMP),
    gsDPSetTextureFilter(G_TF_BILERP),
    gsDPSetCombineLERP(TEXEL1, TEXEL0, PRIM_LOD_FRAC, TEXEL0,  TEXEL1, TEXEL0, PRIM_LOD_FRAC, TEXEL0,  COMBINED, 0, SHADE, 0,  COMBINED, 0, SHADE, 0),
    gsDPSetRenderMode(G_RM_PASS, G_RM_AA_ZB_OPA_SURF2),
    gsDPSetTextureLOD(G_TL_TILE),
    gsDPSetCycleType(G_CYC_2CYCLE),
    gsSPSetGeometryMode(G_CULL_BACK ),
    gsSPEndDisplayList()
};

//D:8003FD28
Gfx MipMap2C_Something2_Setup[] = {
    gsDPSetTile(G_IM_FMT_CI, G_IM_SIZ_8b, 2, 0, 0, 0, G_TX_WRAP, 5, 0, G_TX_WRAP, 5, 0),
    gsDPSetTile(G_IM_FMT_CI, G_IM_SIZ_8b, 2, 0, 1, 0, G_TX_WRAP, 5, 0, G_TX_WRAP, 5, 0),
    gsDPSetTileSize(0, 2, 2, 0, 0),
    gsDPSetTileSize(1, 2, 2, 0, 0),
    gsDPSetPrimColor(0, 15, 255, 255, 255, 255),
    gsDPSetTextureDetail(G_TD_CLAMP),
    gsDPSetTextureFilter(G_TF_BILERP),
    gsDPSetCombineLERP(TEXEL1, TEXEL0, PRIM_LOD_FRAC, TEXEL0,  TEXEL1, TEXEL0, PRIM_LOD_FRAC, TEXEL0,  COMBINED, 0, SHADE, 0,  COMBINED, 0, SHADE, 0),
    gsDPSetRenderMode(G_RM_PASS, G_RM_AA_ZB_OPA_SURF2),
    gsDPSetTextureLOD(G_TL_TILE),
    gsDPSetCycleType(G_CYC_2CYCLE),
    gsSPSetGeometryMode(G_CULL_BACK ),
    gsSPEndDisplayList()
};

//End Dl means this gfx list cannot go any further. perhaps below is a vtx array?

u32 D_8003FD90 = 0;
f32 g_SkyCloudOffset = 0;
f32 D_8003FD98[] = { 0, 0 };

// TODO: D_8003FDA0 is actually a `struct hand`
u32 D_8003FDA0[] = {
    0x00000000, 0xFFFFFFFF, 0x00000000, 0x00000100,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 
    0x00000000, 0x00000000, 0x00000000, 0x3F800000, 
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 
    0x3F800000, 0x00000000, 0x00000000, 0x00000000, 
    0x00000000, 0x3F800000, 0x00000000, 0x00000000, 
    0x00000000, 0x00000000, 0x3F800000, 0x00000000, 
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 
    0x00000000, 0xBF800000, 0x00000000, 0x3F800000, 
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 
#if defined(BUGFIX_R2)
    0x00000000, 0x00000000, 0XC18600DA, 0x00000000, 
    0X418600DA, 0x00000000, 0x00000000, 0x00000000, 
#else
    0x00000000, 0x00000000, 0xC19FFFFE, 0x00000000, 
    0x419FFFFE, 0x00000000, 0x00000000, 0x00000000, 
#endif
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 
    0xBF800000, 0x00000000, 0x00000000, 0xBF800000, 
    0x00000000, 0x00000000, 0xBF800000, 0x00000000, 
    0x00000000, 0xBF800000, 0x00000000, 0x3F800000, 
    0x00000000, 0x00000000, 0x3F800000, 0x00000000, 
    0x00000000, 0x3F800000, 0x00000000, 0x00000000, 
    0x3F800000, 0x00000000, 0x00000000, 0x00000000, 
    0x3F800000, 0x3F800000, 0x00000000, 0x00000000, 
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 
    0x447A0000, 0x00000000, 0x00000000, 0x00000000, 
    0x00000000, 0xFF000000, 0x00000000, 0x00000000, 
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 
    0x00000000, 0x00000000
};


u64 D_80040148[] = { 0, 0, 0 }; // Unused.


void sub_GAME_7F092E50(void)
{
#ifdef VERSION_EU
    f32 delta = g_GlobalTimerDelta;
#else
    f32 delta = g_ClockTimer;
#endif

    flt_CODE_bss_80079E80 += delta * 0.25f;

    if (flt_CODE_bss_80079E80 >= 256.0f)
    {
        flt_CODE_bss_80079E80 -= 256.0f;
    }

    if (flt_CODE_bss_80079E80 < 0.0f)
    {
        flt_CODE_bss_80079E80 += 256.0f;
    }

    flt_CODE_bss_80079E84 += delta * 0.1f;

    if (flt_CODE_bss_80079E84 >= 256.0f)
    {
        flt_CODE_bss_80079E84 -= 256.0f;
    }

    if (flt_CODE_bss_80079E84 < 0.0f)
    {
        flt_CODE_bss_80079E84 += 256.0f;
    }

    flt_CODE_bss_80079E88 += delta * 0.04f;

    // 6.2831802f is not quite equal to M_TAU_F. Leave as literal value here.
    if (flt_CODE_bss_80079E88 >= 6.2831802f)
    {
        flt_CODE_bss_80079E88 -= 6.2831802f;
    }

    if (flt_CODE_bss_80079E88 < 0.0f)
    {
        flt_CODE_bss_80079E88 += 6.2831802f;
    }
    
    MipMap2C_Something_Setup[2].loadtile.sl = flt_CODE_bss_80079E80;
    MipMap2C_Something_Setup[2].loadtile.tl = flt_CODE_bss_80079E84;
    MipMap2C_Something_Setup[3].loadtile.sl = ((s32)flt_CODE_bss_80079E80 + 90) & 0xFF;
    MipMap2C_Something_Setup[3].loadtile.tl = ((s32)flt_CODE_bss_80079E84 + 150) & 0xFF;
    ((u32 *) MipMap2C_Something_Setup)[8] = (((u32 *) MipMap2C_Something_Setup)[8] & ~0xFF) | (u32) ((sinf(flt_CODE_bss_80079E88) * 127.0f) + 128.0f);

    MipMap2C_Something2_Setup[2].loadtile.sl = flt_CODE_bss_80079E80;
    MipMap2C_Something2_Setup[2].loadtile.tl = flt_CODE_bss_80079E84;
    MipMap2C_Something2_Setup[3].loadtile.sl = ((s32)flt_CODE_bss_80079E80 + 90) & 0xFF;
    MipMap2C_Something2_Setup[3].loadtile.tl = ((s32)flt_CODE_bss_80079E84 + 150) & 0xFF;
    ((u32 *) MipMap2C_Something2_Setup)[8] = (((u32 *) MipMap2C_Something_Setup)[8] & ~0xFF) | (u32) ((sinf(flt_CODE_bss_80079E88) * 127.0f) + 128.0f);
}


Gfx* sub_GAME_7F09343C(Gfx *gdl, s32 arg1)
{
    if (arg1 != 0)
    {
        gSPDisplayList(gdl++, MipMap2C_Something_Setup);
    }
    else
    {
        gDPSetTile(gdl++, G_IM_FMT_RGBA, G_IM_SIZ_16b, 4, 0, 0, 0, 0, 5, 0, 0, 5, 0);
        gDPSetTile(gdl++, G_IM_FMT_RGBA, G_IM_SIZ_16b, 4, 0, 1, 0, 0, 5, 0, 0, 5, 0);
        gDPSetTileSize(gdl++, 0, 0, 0, 0, 0);
        gDPSetTileSize(gdl++, 1, 90, 150, 0, 0);
        gDPSetPrimColor(gdl++, 0, (sinf(flt_CODE_bss_80079E88) * 127.0f + 128.0f), 0xFF, 0xFF, 0xFF, 0xFF);
        gDPSetTextureDetail(gdl++, G_TD_CLAMP);
        gDPSetTextureFilter(gdl++, G_TF_BILERP);
        gDPSetCombineLERP(gdl++, TEXEL1, TEXEL0, PRIM_LOD_FRAC, TEXEL0, TEXEL1, TEXEL0, PRIM_LOD_FRAC, TEXEL0, COMBINED, 0, SHADE, 0, COMBINED, 0, SHADE, 0);
        gDPSetRenderMode(gdl++, G_RM_PASS, G_RM_AA_ZB_OPA_SURF2);
        gDPSetTextureLOD(gdl++, G_TL_TILE);
        gDPSetCycleType(gdl++, G_CYC_2CYCLE);
        gSPSetGeometryMode(gdl++, G_CULL_BACK);
    }
    return gdl;
}


Gfx* sub_GAME_7F09365C(Gfx *gdl, s32 arg1)
{
    if (arg1 != 0)
    {
        gSPDisplayList(gdl++, MipMap2C_Something2_Setup);
    }
    else
    {
        gDPSetTile(gdl++, G_IM_FMT_CI, G_IM_SIZ_8b, 2, 0, 0, 0, 0, 5, 0, 0, 5, 0);
        gDPSetTile(gdl++, G_IM_FMT_CI, G_IM_SIZ_8b, 2, 0, 1, 0, 0, 5, 0, 0, 5, 0);
        gDPSetTileSize(gdl++, 0, 0, 0, 0, 0);
        gDPSetTileSize(gdl++, 1, 90, 150, 0, 0);
        gDPSetPrimColor(gdl++, 0, (sinf(flt_CODE_bss_80079E88) * 127.0f + 128.0f), 0xFF, 0xFF, 0xFF, 0xFF);
        gDPSetTextureDetail(gdl++, G_TD_CLAMP);
        gDPSetTextureFilter(gdl++, G_TF_BILERP);
        gDPSetCombineLERP(gdl++, TEXEL1, TEXEL0, PRIM_LOD_FRAC, TEXEL0, TEXEL1, TEXEL0, PRIM_LOD_FRAC, TEXEL0, COMBINED, 0, SHADE, 0, COMBINED, 0, SHADE, 0); /* expands to FC272C04 1F1093FF */
        gDPSetRenderMode(gdl++, G_RM_PASS, G_RM_AA_ZB_OPA_SURF2);
        gDPSetTextureLOD(gdl++, G_TL_TILE);
        gDPSetCycleType(gdl++, G_CYC_2CYCLE);
        gSPSetGeometryMode(gdl++, G_CULL_BACK);
    }

    return gdl;
}
