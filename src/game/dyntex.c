#include <ultra64.h>
#include "bondview.h"
#include "lv.h"
#include "dyntex.h"

f32 g_DyntexWaterScrollS;
f32 g_DyntexWaterScrollT;
f32 g_DyntexWaterBlendPhase;

Gfx g_DyntexWaterI4SetupGdl[] = {
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

Gfx g_DyntexWaterCi8SetupGdl[] = {
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


// Water animation controller.
void dyntexWaterController(void)
{
    f32 delta = g_ClockTimer;

    g_DyntexWaterScrollS += delta * 0.25f;

    if (g_DyntexWaterScrollS >= 256.0f)
    {
        g_DyntexWaterScrollS -= 256.0f;
    }

    if (g_DyntexWaterScrollS < 0.0f)
    {
        g_DyntexWaterScrollS += 256.0f;
    }

    g_DyntexWaterScrollT += delta * 0.1f;

    if (g_DyntexWaterScrollT >= 256.0f)
    {
        g_DyntexWaterScrollT -= 256.0f;
    }

    if (g_DyntexWaterScrollT < 0.0f)
    {
        g_DyntexWaterScrollT += 256.0f;
    }

    g_DyntexWaterBlendPhase += delta * 0.04f;

    // 6.2831802f is not quite equal to M_TAU_F. Leave as literal value here.
    if (g_DyntexWaterBlendPhase >= 6.2831802f)
    {
        g_DyntexWaterBlendPhase -= 6.2831802f;
    }

    if (g_DyntexWaterBlendPhase < 0.0f)
    {
        g_DyntexWaterBlendPhase += 6.2831802f;
    }
    
    g_DyntexWaterI4SetupGdl[2].loadtile.sl = g_DyntexWaterScrollS;
    g_DyntexWaterI4SetupGdl[2].loadtile.tl = g_DyntexWaterScrollT;
    g_DyntexWaterI4SetupGdl[3].loadtile.sl = ((s32)g_DyntexWaterScrollS + 90) & 0xFF;
    g_DyntexWaterI4SetupGdl[3].loadtile.tl = ((s32)g_DyntexWaterScrollT + 150) & 0xFF;
    ((u32 *) g_DyntexWaterI4SetupGdl)[8] = (((u32 *) g_DyntexWaterI4SetupGdl)[8] & ~0xFF) | (u32) ((sinf(g_DyntexWaterBlendPhase) * 127.0f) + 128.0f);

    g_DyntexWaterCi8SetupGdl[2].loadtile.sl = g_DyntexWaterScrollS;
    g_DyntexWaterCi8SetupGdl[2].loadtile.tl = g_DyntexWaterScrollT;
    g_DyntexWaterCi8SetupGdl[3].loadtile.sl = ((s32)g_DyntexWaterScrollS + 90) & 0xFF;
    g_DyntexWaterCi8SetupGdl[3].loadtile.tl = ((s32)g_DyntexWaterScrollT + 150) & 0xFF;
    ((u32 *) g_DyntexWaterCi8SetupGdl)[8] = (((u32 *) g_DyntexWaterI4SetupGdl)[8] & ~0xFF) | (u32) ((sinf(g_DyntexWaterBlendPhase) * 127.0f) + 128.0f);
}


Gfx* dyntexConfigureTwoLayerWater(Gfx *gdl, s32 useIntensityTexture)
{
    // Use I4 texture, 64-texel wrapping.
    if (useIntensityTexture)
    {
        gSPDisplayList(gdl++, g_DyntexWaterI4SetupGdl);
    }
    // Use RGBA 16 texture with fixed tile offsets.
    else
    {
        gDPSetTile(gdl++, G_IM_FMT_RGBA, G_IM_SIZ_16b, 4, 0, 0, 0, 0, 5, 0, 0, 5, 0);
        gDPSetTile(gdl++, G_IM_FMT_RGBA, G_IM_SIZ_16b, 4, 0, 1, 0, 0, 5, 0, 0, 5, 0);
        gDPSetTileSize(gdl++, 0, 0, 0, 0, 0);
        gDPSetTileSize(gdl++, 1, 90, 150, 0, 0);
        gDPSetPrimColor(gdl++, 0, (sinf(g_DyntexWaterBlendPhase) * 127.0f + 128.0f), 0xFF, 0xFF, 0xFF, 0xFF);
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


// Use CI8 texture, 32-texel wrapping.
Gfx* dyntexConfigureTwoLayerCiWater(Gfx *gdl)
{
    gSPDisplayList(gdl++, g_DyntexWaterCi8SetupGdl);

    return gdl;
}
