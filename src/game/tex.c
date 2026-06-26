#include <ultra64.h>
#include "gbi_extension.h"
#include "lightfixture.h"
#include "lv.h"
#include "image.h"
#include "tex.h"

// bss
//8008D140
s32 g_TexLutMode;
//8008D144
s32 dword_CODE_bss_8008D144;
//8008D148
s_bss_8008D148 g_TexTileStates[8]; //[0x160]; //8 entries of 0x2c ea
//8008D2A8
s_bss_8008D2A8 g_TexTileSizes[8]; //[0xA0]; //8 entries of 0x14 ea
//8008D348
s32 dword_CODE_bss_8008D348;


// data
//D:8004E760
u16 D_8004E760[] = {HIT_BULLET_STONE1_SFX, HIT_BULLET_STONE2_SFX};
//D:8004E764
u8 D_8004E764[] = {0x7, 0x0, 0x0, 0x0};
//D:8004E768
struct image_sound isnd_default = {&D_8004E760, &D_8004E764, 0x0, 0x1};

//D:8004E774
u16 D_8004E774[] = {HIT_BULLET_STONE1_SFX, HIT_BULLET_STONE2_SFX};
//D:8004E778
u8 D_8004E778[] = {0x1, 0x0, 0x0, 0x0};
//D:8004E77C
struct image_sound isnd_stone = {&D_8004E774, &D_8004E778, 0x2, 0x1};

//D:8004E788
u16 D_8004E788[] = {HIT_BULLET_WOOD_SFX, HIT_BULLET_WOOD2_SFX};
//D:8004E78C
u8 D_8004E78C[] = {0x1, 0x0, 0x0, 0x0};
//D:8004E790
struct image_sound isnd_wood = {&D_8004E788, &D_8004E78C, 0x2, 0x1};

//D:8004E79C
u16 D_8004E79C[] = {HIT_BULLET_METAL_A_SFX, HIT_BULLET_METAL_A4_SFX, HIT_BULLET_METAL_A3_SFX, 0x00};
//D:8004E7A4
u8 D_8004E7A4[] = {0x7, 0x0, 0x0, 0x0};
//D:8004E7A8
struct image_sound isnd_metal = {&D_8004E79C, &D_8004E7A4, 0x3, 0x1};

//D:8004E7B4
u16 D_8004E7B4[] = {HIT_BULLET_GLASS_SFX, 0x00};
//D:8004E7B8
u8 D_8004E7B8[] = {0x4, 0x5, 0x6, 0x0};
//D:8004E7BC
struct image_sound isnd_glass = {&D_8004E7B4, &D_8004E7B8, 0x1, 0x3};

//D:8004E7C8
u16 D_8004E7C8[] = {HIT_BULLET_WATER_SFX, 0x00};
//D:8004E7CC
u8 D_8004E7CC[] = {0, 0, 0, 0};
//D:8004E7D0
struct image_sound isnd_water = {&D_8004E7C8, &D_8004E7CC, 0x1, 0x0};

//D:8004E7DC
u16 D_8004E7DC[] = {HIT_BULLET_SNOW_SFX, 0x00};
//D:8004E7E0
u8 D_8004E7E0[] = {0x1, 0x0, 0x0, 0x0};
//D:8004E7E4
struct image_sound isnd_snow = {&D_8004E7DC, &D_8004E7E0, 0x1, 0x1};

//D:8004E7F0
u16 D_8004E7F0[] = {HIT_BULLET_DIRT1_SFX, HIT_BULLET_DIRT2_SFX};
//D:8004E7F4
u8 D_8004E7F4[] = {0x2, 0x0, 0x0, 0x0};
//D:8004E7F8
struct image_sound isnd_dirt = {&D_8004E7F0, &D_8004E7F4, 0x2, 0x1};

//D:8004E804
u16 D_8004E804[] = {HIT_BULLET_MUD1_SFX, HIT_BULLET_MUD2_SFX, HIT_BULLET_MUD3_SFX, 0x00};
//D:8004E80C
u8 D_8004E80C[] = {0x2, 0x0, 0x0, 0x0};
//D:8004E810
struct image_sound isnd_mud = {&D_8004E804, &D_8004E80C, 0x3, 0x1};

//D:8004E81C
u16 D_8004E81C[] = {HIT_BULLET_TILE_SFX, 0x00};
//D:8004E820
u8 D_8004E820[] = {0x1, 0x0, 0x0, 0x0};
//D:8004E824
struct image_sound isnd_tile = {&D_8004E81C, &D_8004E820, 0x1, 0x1};

//D:8004E830
u16 D_8004E830[] = {HIT_METAL_OBJECT1_SFX, HIT_METAL_OBJECT2_SFX};
//D:8004E834
u8 D_8004E834[] = {0x1, 0x7, 0x0, 0x0};
//D:8004E838
struct image_sound isnd_metalobj = {&D_8004E830, &D_8004E834, 0x2, 0x2};

//D:8004E844
u16 D_8004E844[] = {HIT_BULLET_FLESH_SFX, 0x00};
//D:8004E848
u8 D_8004E848[] = {0x2, 0x0, 0x0, 0x0};
//D:8004E84C
struct image_sound isnd_chr = {&D_8004E844, &D_8004E848, 0x1, 0x1};

//D:8004E858
u16 D_8004E858[] = {HIT_BULLET_GLASS_SFX, 0x00};
//D:8004E85C
u8 D_8004E85C[] = {0x11, 0x12, 0x13, 0x0};
//D:8004E860
struct image_sound isnd_glass_xlu = {&D_8004E858, &D_8004E85C, 0x1, 0x3};

//D:8004E86C
struct image_sound *g_HitTypeSounds[] = { 
    &isnd_default,
    &isnd_stone,
    &isnd_wood,
    &isnd_metal,
    &isnd_glass,
    &isnd_water,
    &isnd_snow,
    &isnd_dirt,
    &isnd_mud,
    &isnd_tile,
    &isnd_metalobj,
    &isnd_chr,
    &isnd_glass_xlu
};

char * image_sound_names[] = {
    "HIT_DEFAULT",
    "HIT_STONE",
    "HIT_WOOD",
    "HIT_METAL",
    "HIT_GLASS",
    "HIT_WATER",
    "HIT_SNOW",
    "HIT_DIRT",
    "HIT_MUD",
    "HIT_TILE",
    "HIT_METALOBJ",
    "HIT_CHR",
    "HIT_GLASS_XLU"
};



void nullsub_47(void) {
    return;
}


void sub_GAME_7F0CC4C8(void)
{
    int i;

    for (i=0; i<8; i++)
    {
        g_TexTileStates[i].inuse = 0;
        g_TexTileSizes[i].inuse = 0;
    }
    g_TexLutMode = -1;
}


s32 unused_copy_byte_array(u8* src, s32 count, u8* dst)
{
    u8* src_itr;
    u8* dst_itr;

    src_itr = src;
    dst_itr = dst;

    while (count > 0)
    {
        *dst_itr = *src_itr;
        dst_itr++;
        src_itr++;
        count--;
    }

    return dst_itr - dst;
}


u32 texSetLutMode(s32 arg0) {
    if (arg0 == g_TexLutMode) {
        return 0;
    }
    g_TexLutMode = arg0;
    return 1;
}


//s32 texTrySetTileState(s32 index, s32 arg1, s32 arg2, s32 arg3, s32 arg4, s32 arg5, s32 arg6, s32 arg7, s32 arg8, s32 arg9, s32 argA)
s32 texTrySetTileState(s32 tile, s32 format, s32 depth, s32 line, s32 tmem, s32 smode, s32 tmode, s32 masks, s32 maskt, s32 shifts, s32 shiftt)
{
    s32 retval;

    retval = 0;
    if ((g_TexTileStates[tile].inuse == 0) || (format != g_TexTileStates[tile].unk_4) || 
     (depth != g_TexTileStates[tile].unk_8) || (line != g_TexTileStates[tile].unk_C) || 
     (tmem != g_TexTileStates[tile].unk_10) || (smode != g_TexTileStates[tile].unk_14) || 
     (tmode != g_TexTileStates[tile].unk_18) || (masks != g_TexTileStates[tile].unk_1C) || 
     (maskt != g_TexTileStates[tile].unk_20) || (shifts != g_TexTileStates[tile].unk_24) || 
     (shiftt != g_TexTileStates[tile].unk_28))
    {
        retval = 1;
        g_TexTileStates[tile].inuse = 1;
        g_TexTileStates[tile].unk_4 = format;
        g_TexTileStates[tile].unk_8 = depth;
        g_TexTileStates[tile].unk_C = line;
        g_TexTileStates[tile].unk_10 = tmem;
        g_TexTileStates[tile].unk_14 = smode;
        g_TexTileStates[tile].unk_18 = tmode;
        g_TexTileStates[tile].unk_1C = masks;
        g_TexTileStates[tile].unk_20 = maskt;
        g_TexTileStates[tile].unk_24 = shifts;
        g_TexTileStates[tile].unk_28 = shiftt;

    }
    return retval;
}


s32 texTrySetTileSize(s32 tile, s32 uls, s32 ult, s32 lrs, s32 lrt)
{
    s32 retval;

    retval = 0;
    if ((((g_TexTileSizes[tile].inuse == 0) || (uls != g_TexTileSizes[tile].unk_4)) || (ult != g_TexTileSizes[tile].unk_8)) ||
       ((lrs != g_TexTileSizes[tile].unk_C || (lrt != g_TexTileSizes[tile].unk_10))))
    {
        retval = 1;
        g_TexTileSizes[tile].inuse = 1;
        g_TexTileSizes[tile].unk_4 = uls;
        g_TexTileSizes[tile].unk_8 = ult;
        g_TexTileSizes[tile].unk_C = lrs;
        g_TexTileSizes[tile].unk_10 = lrt;
    }
    return retval;
}


s32 texGetWidthAtLod(struct tex *tex, s32 lod)
{
    s32 i;
    s32 width = tex->width;

    if (lod == 0)
    {
        return width;
    }

    if (tex->hasExplicitLods)
    {
        for (i = 0; i < g_TexCacheCount; i++)
        {
            if (tex->texturenum == g_TexCacheItems[i].texturenum)
            {
                return g_TexCacheItems[i].widths[lod - 1];
            }
        }

        return 1;
    }

    for (i = 0; i < lod; i++)
    {
        width = (width + 1) >> 1;
    }

    return width;
}


s32 texGetHeightAtLod(struct tex *tex, s32 lod)
{
    s32 i;
    s32 height = tex->height;

    if (lod == 0)
    {
        return height;
    }

    if (tex->hasExplicitLods)
    {
        for (i = 0; i < g_TexCacheCount; i++)
        {
            if (tex->texturenum == g_TexCacheItems[i].texturenum)
            {
                return g_TexCacheItems[i].heights[lod - 1];
            }
        }

        return 1;
    }

    for (i = 0; i < lod; i++)
    {
        height = (height + 1) >> 1;
    }

    return height;
}


s32 texGetLineSizeInBytes(struct tex *tex, s32 lod)
{
    s32 depth = tex->depth;
    s32 width = texGetWidthAtLod(tex, lod);

    if (depth == G_IM_SIZ_32b)
    {
        return (width + 3) / 4;
    }

    if (depth == G_IM_SIZ_16b)
    {
        return (width + 3) / 4;
    }

    if (depth == G_IM_SIZ_8b)
    {
        return (width + 7) / 8;
    }

    return (width + 15) / 16;
}


s32 texGetSizeInBytes(struct tex *tex, s32 lod)
{
    return texGetHeightAtLod(tex, lod) * texGetLineSizeInBytes(tex, lod);
}


void texGetDepthAndSize(struct tex *tex, s32 *deptharg, s32 *lenarg)
{
    s32 depth = tex->depth;
    s32 maxlod = tex->maxlod ? tex->maxlod : 1;
    s32 lod;

    *lenarg = 0;

    if (depth == G_IM_SIZ_32b)
    {
        *deptharg = G_IM_SIZ_32b;
    }
    else if (depth == G_IM_SIZ_16b)
    {
        *deptharg = G_IM_SIZ_16b;
    }
    else if (depth == G_IM_SIZ_8b)
    {
        *deptharg = G_IM_SIZ_16b;
    }
    else
    {
        *deptharg = G_IM_SIZ_16b;
    }

    for (lod = 0; lod < maxlod; lod++) {
        *lenarg += texGetSizeInBytes(tex, lod) * 4;
    }
}


s32 texDimensionToMask(s32 arg0) {
    s32 i = 0;

    arg0--;

    while (arg0 > 0 && i < 8) {
        arg0 >>= 1;
        i++;
    }

    return i;
}





u32 texModeToGbiMode(s32 arg0) {
    if (arg0 == 1) {
        return G_TX_CLAMP;
    }
    if (arg0 == 2) {
        return G_TX_MIRROR;
    }
    return G_TX_WRAP;
}


s32 sub_GAME_7F0CCAFC(f32 arg0)
{
    return (s32)arg0 < (f32)arg0 ? (s32)arg0+1 : (s32)arg0;
}


s32 sub_GAME_7F0CCB38(s32 *arg0)
{
    s32 temp_t7;
    s32 temp_v0;

    temp_v0 = texGetLineSizeInBytes(arg0, 0U);
    temp_t7 = arg0[2] & 3;
    if (temp_t7 == 3)
    {
        return sub_GAME_7F0CCAFC((2.0f / (f32) (temp_v0 * 4)) * 2048.0f);
    }
    if (temp_t7 == 2)
    {
        return sub_GAME_7F0CCAFC((4.0f / (f32) (temp_v0 * 4)) * 2048.0f);
    }
    if (temp_t7 == 1)
    {
        return sub_GAME_7F0CCAFC((8.0f / (f32) (temp_v0 * 8)) * 2048.0f);
    }
    return sub_GAME_7F0CCAFC((16.0f / (f32) (temp_v0 * 0x10)) * 2048.0f);
}


Gfx *texWriteTileFromDefinition(Gfx *gdl, struct tex *tex, s32 offset, s32 shifts, s32 shiftt, s32 arg5)
{
    struct image_entry *s0 = &g_Textures[tex->texturenum];
    s32 sp88;
    s32 sp84;
    s32 line;
    u32 stack[4];
    s32 uls;
    s32 ult;
    s32 lrs;
    s32 lrt;

    sp88 = texDimensionToMask(tex->width);
    sp84 = texDimensionToMask(tex->height);

    line = texGetLineSizeInBytes(tex, 0);

    gDPSetPrimColorViaWord(gdl++, arg5, 0, 0xffffffff);

    if (texSetLutMode(tex->lutmodeindex << G_MDSFT_TEXTLUT))
    {
        gDPSetTextureLUT(gdl++, tex->lutmodeindex << G_MDSFT_TEXTLUT);
    }

    if (texTrySetTileState(0, tex->gbiformat, tex->depth, line, s0->flag3 + line * s0->flag4, 0, 0, sp88 - s0->flag5, sp84 - s0->flag6, shifts, shiftt))
    {
        gDPSetTile(gdl++, tex->gbiformat, tex->depth, line, s0->flag3 + line * s0->flag4, 0, 0,
                texModeToGbiMode(0), sp84 - s0->flag6, shiftt,
                texModeToGbiMode(0), sp88 - s0->flag5, shifts);
    }

    uls = (offset == 2 && !tex->hasExplicitLods ? 2 : 0) + 0;
    ult = (offset == 2 && !tex->hasExplicitLods ? 2 : 0) + 0;
    lrs = (offset == 2 && !tex->hasExplicitLods ? 2 : 0) + ((tex->width - 1) << 2);
    lrt = (offset == 2 && !tex->hasExplicitLods ? 2 : 0) + ((tex->height - 1) << 2);

    if (texTrySetTileSize(0, uls, ult, lrs, lrt))
    {
        gDPSetTileSize(gdl++, 0, uls, ult, lrs, lrt);
    }

    return gdl;
}

Gfx* texWriteTextureCmd(Gfx* arg0, Gfx* arg1, struct tex* tex, s32 arg3)
{
    s32 lod = tex->maxlod ? tex->maxlod - 1 : 0;

    if (arg3)
    {
        if (arg1 != NULL)
        {
            u32 v0 = (arg1->words.w0 & ~0x3800) | (lod << 11);

            if (v0 != arg1->words.w0)
            {
                arg0->words.w0 = v0;
                arg0->words.w1 = arg1->words.w1;
                arg0++;
            }
        }
        else
        {
            gSPTexture(arg0++, 0xffff, 0xffff, lod, G_TX_RENDERTILE, G_ON);
        }
    }
    else
    {
        arg1->words.w0 &= ~0x3800;
        arg1->words.w0 |= lod << 11;
    }

    return arg0;
}


Gfx *texWriteLoadToTmemAddr(Gfx *gdl, struct tex *tex, s32 tmemoffset)
{
	s32 depth;
	s32 len;

	texGetDepthAndSize(tex, &depth, &len);

	if (tex->lutmodeindex == 0)
	{
		gDPSetTextureImage(gdl++, tex->gbiformat, depth, 1, tex->data);

		if (texTrySetTileState(7, tex->gbiformat, depth, 0, tmemoffset, 0, 0, 0, 0, 0, 0))
		{
			gDPSetTile(gdl++, tex->gbiformat, depth, 0, tmemoffset, 7, 0, 0, 0, 0, 0, 0, 0);
		}

		gDPLoadSync(gdl++);
		gDPLoadBlock(gdl++, 7, 0, 0, len - 1, 0);
		gDPPipeSync(gdl++);
	}
	else
	{
		u32 uls;
		u32 ult;

		gDPSetTextureImage(gdl++, tex->gbiformat, depth, 1, tex->data);

		if (texTrySetTileState(7, tex->gbiformat, depth, 0, tmemoffset, 0, 0, 0, 0, 0, 0))
		{
			gDPSetTile(gdl++, tex->gbiformat, depth, 0, tmemoffset, 7, 0, 0, 0, 0, 0, 0, 0);
		}

		gDPLoadSync(gdl++);
		gDPLoadBlock(gdl++, 7, 0, 0, len - 1, 0);
		gDPPipeSync(gdl++);

		uls = len;

		if ((u32)(0x3ff - tex->unk0a) < uls)
		{
			ult = 0x3ff - tex->unk0a;
		}
		else
		{
			ult = 0;
		}

		uls -= ult;

		if (texTrySetTileState(7, 0, 0, 1, 0x100, 0, 0, 0, 0, 0, 0))
		{
			gDPSetTile(gdl++, 0, 0, 1, 0x100, 7, 0, 0, 0, 0, 0, 0, 0);
		}
        
		gDPLoadSync(gdl++);
		gDPLoadTLUT07(gdl++, uls, ult, tex->unk0a + uls, ult);
		gDPPipeSync(gdl++);
	}

	return gdl;
}


Gfx *texWriteTileLods(Gfx *gdl, struct tex *tex, s32 smode, s32 tmode, s32 offset, s32 basetile, s32 tmemaddr)
{
    s32 tmem;
    s32 masks;
    s32 tile;
    s32 maskt;
    s32 line;
    s32 ult;
    s32 lrs;
    s32 lrt;
    s32 lod;
    s32 uls;
    bool hasExplicitLods;
    s32 end;
    s32 pad;
    s32 size;
    s32 pad2;

    tmem = tmemaddr;
    end = tex->maxlod;
    end = end + basetile;

    for (tile = basetile; tile < (end ^ 0); tile++)
    {
        pad2 = basetile;
        lod = tile - pad2;
        masks = texDimensionToMask(texGetWidthAtLod(tex, lod));
        maskt = texDimensionToMask(texGetHeightAtLod(tex, lod));
        line = texGetLineSizeInBytes(tex, lod);
        size = texGetSizeInBytes(tex, lod);
        hasExplicitLods = tex->hasExplicitLods;

        if (texSetLutMode(tex->lutmodeindex << 14))
        {
            gDPSetTextureLUT(gdl++, tex->lutmodeindex << G_MDSFT_TEXTLUT);
        }

        if (texTrySetTileState(tile, tex->gbiformat, tex->depth, line, tmem, smode, tmode, masks, maskt, tile - pad2, tile - pad2))
        {
            gDPSetTile(gdl++, tex->gbiformat, tex->depth, line, tmem, tile, 0, texModeToGbiMode(tmode), maskt, lod, texModeToGbiMode(smode), masks, tile - basetile);
        }

        uls = (((offset == 2) && (hasExplicitLods == 0)) ? (2) : (0)) + 0;
        ult = (((offset == 2) && (hasExplicitLods == 0)) ? (2) : (0)) + 0;
        lrs = ((texGetWidthAtLod(tex, lod) - 1) << 2) + (((offset == 2) && (hasExplicitLods == 0)) ? (2) : (0));
        lrt = ((texGetHeightAtLod(tex, lod) - 1) << 2) + (((offset == 2) && (hasExplicitLods == 0)) ? (2) : (0));

        if (texTrySetTileSize(tile, uls, ult, lrs, lrt))
        {
            gDPSetTileSize(gdl++, tile, uls, ult, lrs, lrt);
        }

        tmem += size;
    }

    lod = tile - pad2;
    
    return gdl;
}


Gfx *texWriteLoadToTmemZero(Gfx *gdl, struct tex *tex)
{
	s32 depth;
	s32 len;
	s32 dxt;

	dxt = sub_GAME_7F0CCB38((s32 *)tex);
	texGetDepthAndSize(tex, &depth, &len);

	if (tex->lutmodeindex == 0)
	{
		gDPSetTextureImage(gdl++, tex->gbiformat, depth, 1, tex->data);
		if (texTrySetTileState(7, tex->gbiformat, depth, 0, 0, 0, 0, 0, 0, 0, 0))
		{
			gDPSetTile(gdl++, tex->gbiformat, depth, 0, 0, 7, 0, 0, 0, 0, 0, 0, 0);
		}
		gDPLoadSync(gdl++);
		gDPLoadBlock(gdl++, 7, 0, 0, len - 1, dxt);
		gDPPipeSync(gdl++);
	}
	else
	{
		u32 uls;
		u32 ult;

		gDPSetTextureImage(gdl++, tex->gbiformat, depth, 1, tex->data);
		if (texTrySetTileState(7, tex->gbiformat, depth, 0, 0, 0, 0, 0, 0, 0, 0))
		{
			gDPSetTile(gdl++, tex->gbiformat, depth, 0, 0, 7, 0, 0, 0, 0, 0, 0, 0);
		}
		gDPLoadSync(gdl++);
		gDPLoadBlock(gdl++, 7, 0, 0, len - 1, dxt);
		gDPPipeSync(gdl++);

		uls = len;
		if ((u32)(0x3ff - tex->unk0a) < uls)
		{
			ult = 0x3ff - tex->unk0a;
		}
		else
		{
			ult = 0;
		}
		uls -= ult;
		if (texTrySetTileState(7, 0, 0, 1, 0x100, 0, 0, 0, 0, 0, 0))
		{
			gDPSetTile(gdl++, 0, 0, 1, 0x100, 7, 0, 0, 0, 0, 0, 0, 0);
		}
		gDPLoadSync(gdl++);
		gDPLoadTLUT07(gdl++, uls, ult, tex->unk0a + uls, ult);
		gDPPipeSync(gdl++);
	}

	return gdl;
}


Gfx *texWriteTile(Gfx *gdl, struct tex *tex, s32 arg2, s32 arg3, s32 arg4, s32 tile)
{
    s32 masks;
    s32 maskt;
    s32 line;
    s32 uls;
    s32 ult;
    s32 lrs;
    s32 lrt;
    s32 sp50;

    masks = texDimensionToMask(tex->width);
    maskt = texDimensionToMask(tex->height);
    line = texGetLineSizeInBytes(tex, 0);
    sp50 = tex->hasExplicitLods;

    if (texSetLutMode(tex->lutmodeindex << G_MDSFT_TEXTLUT))
    {
        gDPSetTextureLUT(gdl++, tex->lutmodeindex << G_MDSFT_TEXTLUT);
    }

    if (texTrySetTileState(tile, tex->gbiformat, tex->depth, line, 0, arg2, arg3, masks, maskt, 0, 0))
    {
        gDPSetTile(gdl++, tex->gbiformat, tex->depth, line, 0x0000, tile, 0,
                texModeToGbiMode(arg3), maskt, G_TX_NOLOD,
                texModeToGbiMode(arg2), masks, G_TX_NOLOD);
    }

    uls = (arg4 == 2 && sp50 == 0 ? 2 : 0) + 0;
    ult = (arg4 == 2 && sp50 == 0 ? 2 : 0) + 0;
    lrs = (arg4 == 2 && sp50 == 0 ? 2 : 0) + ((tex->width - 1) << 2);
    lrt = (arg4 == 2 && sp50 == 0 ? 2 : 0) + ((tex->height - 1) << 2);

    if (texTrySetTileSize(tile, uls, ult, lrs, lrt))
    {
        gDPSetTileSize(gdl++, tile, uls, ult, lrs, lrt);
    }

    return gdl;
}


Gfx *texHandleType2(Gfx *gdl, struct tex *tex, s32 smode, s32 tmode, s32 offset)
{
    gdl = texWriteLoadToTmemAddr(gdl,tex,0);
    gdl = texWriteTileLods(gdl,tex,smode,tmode,offset,0,0);
    if (tex->maxlod == 1) {
        gdl = texWriteTileLods(gdl,tex,smode,tmode,offset,1,0);
    }
    return gdl;
}


Gfx * texHandleType1(Gfx *gdl,struct tex *tex1,s32 smode,s32 tmode,s32 offset,struct tex *tex2,u32 shifts,u32 shiftt,u32 min)
{
    u32 size;

    size = texGetSizeInBytes(tex2,0);
    gdl = texWriteLoadToTmemZero(gdl,tex2);
    gDPTileSync(gdl++);
    gdl = texWriteLoadToTmemAddr(gdl,tex1,size);
    gdl = texWriteTileFromDefinition(gdl,tex2,offset,shifts,shiftt,min);
    gdl = texWriteTileLods(gdl,tex1,smode,tmode,offset,1,size);
    if (tex1->maxlod == 1) {
        gdl = texWriteTileLods(gdl,tex1,smode,tmode,offset,2,size);
    }
    return gdl;
}


Gfx * texHandleType0(Gfx *gdl,struct tex *tex,s32 smode,s32 tmode,s32 offset,u32 shifts,u32 shiftt,u32 min)
{
    gdl = texWriteLoadToTmemAddr(gdl,tex,0);
    gdl = texWriteTileFromDefinition(gdl,tex,offset,shifts,shiftt,min);
    gdl = texWriteTileLods(gdl,tex,smode,tmode,offset,1,0);
    if (tex->maxlod == 1) {
        gdl = texWriteTileLods(gdl,tex,smode,tmode,offset,2,0);
    }
    return gdl;
}


Gfx * texHandleType4(Gfx *DL,u32 *arg1,s32 arg2,s32 arg3,s32 arg4)
{
    DL = texWriteLoadToTmemZero(DL,arg1);
    DL = texWriteTile(DL,arg1,arg2,arg3,arg4,0);
    return DL;
}


Gfx * texHandleType3(Gfx *DL,u32 *arg1,s32 arg2,s32 arg3,s32 arg4)
{
    DL = texWriteLoadToTmemZero(DL,arg1);
    DL = texWriteTile(DL,arg1,arg2,arg3,arg4,0);
    DL = texWriteTile(DL,arg1,arg2,arg3,arg4,1);
    return DL;
}





#ifdef NONMATCHING
s32 texLoadFromGdl(Gfx *src, s32 srcsize, Gfx *dst, void *texpool) {

}
#else
GLOBAL_ASM(
.late_rodata
glabel PADDINGHACKING
.word 0
/*D:8005BF20*/
glabel jpt_tlb_ptrDL_type_B1toC0
.word .L7F0CE66C
.word .L7F0CE720
.word .L7F0CE720
.word .L7F0CE720
.word .L7F0CE720
.word .L7F0CE720
.word .L7F0CE720
.word .L7F0CE720
.word .L7F0CE720
.word .L7F0CE6B8
.word .L7F0CE694
.word .L7F0CE720
.word .L7F0CE720
.word .L7F0CE720
.word .L7F0CE66C
.word .L7F0CE1D8

/*D:8005BF60*/
glabel jpt_tlb_ptr_C0_image_recall_types
.word .L7F0CE2BC
.word .L7F0CE428
.word .L7F0CE4B8
.word .L7F0CE584
.word .L7F0CE5BC
.text
glabel texLoadFromGdl
/* 102C48 7F0CE118 27BDFF68 */  addiu $sp, $sp, -0x98
/* 102C4C 7F0CE11C AFB20038 */  sw    $s2, 0x38($sp)
/* 102C50 7F0CE120 AFBF0054 */  sw    $ra, 0x54($sp)
/* 102C54 7F0CE124 AFBE0050 */  sw    $fp, 0x50($sp)
/* 102C58 7F0CE128 AFB60048 */  sw    $s6, 0x48($sp)
/* 102C5C 7F0CE12C AFB40040 */  sw    $s4, 0x40($sp)
/* 102C60 7F0CE130 AFB10034 */  sw    $s1, 0x34($sp)
/* 102C64 7F0CE134 AFB00030 */  sw    $s0, 0x30($sp)
/* 102C68 7F0CE138 240E0001 */  li    $t6, 1
/* 102C6C 7F0CE13C 000590C3 */  sra   $s2, $a1, 3
/* 102C70 7F0CE140 00E0B025 */  move  $s6, $a3
/* 102C74 7F0CE144 AFB7004C */  sw    $s7, 0x4c($sp)
/* 102C78 7F0CE148 AFB50044 */  sw    $s5, 0x44($sp)
/* 102C7C 7F0CE14C AFB3003C */  sw    $s3, 0x3c($sp)
/* 102C80 7F0CE150 AFA600A0 */  sw    $a2, 0xa0($sp)
/* 102C84 7F0CE154 AFA0008C */  sw    $zero, 0x8c($sp)
/* 102C88 7F0CE158 0000A025 */  move  $s4, $zero
/* 102C8C 7F0CE15C 0000F025 */  move  $fp, $zero
/* 102C90 7F0CE160 AFAE005C */  sw    $t6, 0x5c($sp)
/* 102C94 7F0CE164 AFA00058 */  sw    $zero, 0x58($sp)
/* 102C98 7F0CE168 00808825 */  move  $s1, $a0
/* 102C9C 7F0CE16C 00C08025 */  move  $s0, $a2
/* 102CA0 7F0CE170 0FC33132 */  jal   sub_GAME_7F0CC4C8
/* 102CA4 7F0CE174 AFB20088 */   sw    $s2, 0x88($sp)
/* 102CA8 7F0CE178 16C00003 */  bnez  $s6, .L7F0CE188
/* 102CAC 7F0CE17C 2417000F */   li    $s7, 15
/* 102CB0 7F0CE180 3C168009 */  lui   $s6, %hi(ptr_texture_alloc_start)
/* 102CB4 7F0CE184 26D6C720 */  addiu $s6, %lo(ptr_texture_alloc_start) # addiu $s6, $s6, -0x38e0
.L7F0CE188:
/* 102CB8 7F0CE188 1A40016F */  blez  $s2, .L7F0CE748
/* 102CBC 7F0CE18C 3C158005 */   lui   $s5, %hi(D_800483C8)
/* 102CC0 7F0CE190 26B583C8 */  addiu $s5, %lo(D_800483C8) # addiu $s5, $s5, -0x7c38
.L7F0CE194:
/* 102CC4 7F0CE194 92220000 */  lbu   $v0, ($s1)
/* 102CC8 7F0CE198 284100C1 */  slti  $at, $v0, 0xc1
/* 102CCC 7F0CE19C 14200006 */  bnez  $at, .L7F0CE1B8
/* 102CD0 7F0CE1A0 244FFF4F */   addiu $t7, $v0, -0xb1
/* 102CD4 7F0CE1A4 240100E7 */  li    $at, 231
/* 102CD8 7F0CE1A8 50410129 */  beql  $v0, $at, .L7F0CE650
/* 102CDC 7F0CE1AC 8E210000 */   lw    $at, ($s1)
/* 102CE0 7F0CE1B0 1000015C */  b     .L7F0CE724
/* 102CE4 7F0CE1B4 8E210000 */   lw    $at, ($s1)
.L7F0CE1B8:
/* 102CE8 7F0CE1B8 2DE10010 */  sltiu $at, $t7, 0x10
/* 102CEC 7F0CE1BC 10200158 */  beqz  $at, .L7F0CE720
/* 102CF0 7F0CE1C0 000F7880 */   sll   $t7, $t7, 2
/* 102CF4 7F0CE1C4 3C018006 */  lui   $at, %hi(jpt_tlb_ptrDL_type_B1toC0)
/* 102CF8 7F0CE1C8 002F0821 */  addu  $at, $at, $t7
/* 102CFC 7F0CE1CC 8C2FBF20 */  lw    $t7, %lo(jpt_tlb_ptrDL_type_B1toC0)($at)
/* 102D00 7F0CE1D0 01E00008 */  jr    $t7
/* 102D04 7F0CE1D4 00000000 */   nop   
.L7F0CE1D8:
/* 102D08 7F0CE1D8 17C00006 */  bnez  $fp, .L7F0CE1F4
/* 102D0C 7F0CE1DC 02001025 */   move  $v0, $s0
/* 102D10 7F0CE1E0 3C18E700 */  lui   $t8, 0xe700
/* 102D14 7F0CE1E4 AC580000 */  sw    $t8, ($v0)
/* 102D18 7F0CE1E8 26100008 */  addiu $s0, $s0, 8
/* 102D1C 7F0CE1EC AC400004 */  sw    $zero, 4($v0)
/* 102D20 7F0CE1F0 241E0001 */  li    $fp, 1
.L7F0CE1F4:
/* 102D24 7F0CE1F4 8FB90058 */  lw    $t9, 0x58($sp)
/* 102D28 7F0CE1F8 02002025 */  move  $a0, $s0
/* 102D2C 7F0CE1FC 53200004 */  beql  $t9, $zero, .L7F0CE210
/* 102D30 7F0CE200 8E330004 */   lw    $s3, 4($s1)
/* 102D34 7F0CE204 0FC2ED80 */  jal   save_ptrDL_enpoint_to_current_init_lightfixture_table
/* 102D38 7F0CE208 AFA00058 */   sw    $zero, 0x58($sp)
/* 102D3C 7F0CE20C 8E330004 */  lw    $s3, 4($s1)
.L7F0CE210:
/* 102D40 7F0CE210 3C0E8005 */  lui   $t6, %hi(D_800483C4) 
/* 102D44 7F0CE214 8DCE83C4 */  lw    $t6, %lo(D_800483C4)($t6)
/* 102D48 7F0CE218 326D0FFF */  andi  $t5, $s3, 0xfff
/* 102D4C 7F0CE21C 01A09825 */  move  $s3, $t5
/* 102D50 7F0CE220 15AE000D */  bne   $t5, $t6, .L7F0CE258
/* 102D54 7F0CE224 02C02825 */   move  $a1, $s6
/* 102D58 7F0CE228 8EA40000 */  lw    $a0, ($s5)
/* 102D5C 7F0CE22C 000D7880 */  sll   $t7, $t5, 2
/* 102D60 7F0CE230 10800009 */  beqz  $a0, .L7F0CE258
/* 102D64 7F0CE234 008FC021 */   addu  $t8, $a0, $t7
/* 102D68 7F0CE238 93190000 */  lbu   $t9, ($t8)
/* 102D6C 7F0CE23C 2F2100FF */  sltiu $at, $t9, 0xff
/* 102D70 7F0CE240 10200003 */  beqz  $at, .L7F0CE250
/* 102D74 7F0CE244 00000000 */   nop   
/* 102D78 7F0CE248 10000004 */  b     .L7F0CE25C
/* 102D7C 7F0CE24C 24140001 */   li    $s4, 1
.L7F0CE250:
/* 102D80 7F0CE250 10000002 */  b     .L7F0CE25C
/* 102D84 7F0CE254 0000A025 */   move  $s4, $zero
.L7F0CE258:
/* 102D88 7F0CE258 0000A025 */  move  $s4, $zero
.L7F0CE25C:
/* 102D8C 7F0CE25C 0FC32FBA */  jal   texLoadFromTextureNum
/* 102D90 7F0CE260 02602025 */   move  $a0, $s3
/* 102D94 7F0CE264 02602025 */  move  $a0, $s3
/* 102D98 7F0CE268 0FC32EC3 */  jal   texFindInPool
/* 102D9C 7F0CE26C 02C02825 */   move  $a1, $s6
/* 102DA0 7F0CE270 104000F4 */  beqz  $v0, .L7F0CE644
/* 102DA4 7F0CE274 00409025 */   move  $s2, $v0
/* 102DA8 7F0CE278 8FA7005C */  lw    $a3, 0x5c($sp)
/* 102DAC 7F0CE27C AFA0005C */  sw    $zero, 0x5c($sp)
/* 102DB0 7F0CE280 02002025 */  move  $a0, $s0
/* 102DB4 7F0CE284 8FA5008C */  lw    $a1, 0x8c($sp)
/* 102DB8 7F0CE288 0FC333EF */  jal   texWriteTextureCmd
/* 102DBC 7F0CE28C 00403025 */   move  $a2, $v0
/* 102DC0 7F0CE290 8E230000 */  lw    $v1, ($s1)
/* 102DC4 7F0CE294 00408025 */  move  $s0, $v0
/* 102DC8 7F0CE298 306D0007 */  andi  $t5, $v1, 7
/* 102DCC 7F0CE29C 2DA10005 */  sltiu $at, $t5, 5
/* 102DD0 7F0CE2A0 102000D3 */  beqz  $at, .L7F0CE5F0
/* 102DD4 7F0CE2A4 000D6880 */   sll   $t5, $t5, 2
/* 102DD8 7F0CE2A8 3C018006 */  lui   $at, %hi(jpt_tlb_ptr_C0_image_recall_types)
/* 102DDC 7F0CE2AC 002D0821 */  addu  $at, $at, $t5
/* 102DE0 7F0CE2B0 8C2DBF60 */  lw    $t5, %lo(jpt_tlb_ptr_C0_image_recall_types)($at)
/* 102DE4 7F0CE2B4 01A00008 */  jr    $t5
/* 102DE8 7F0CE2B8 00000000 */   nop   
.L7F0CE2BC:
/* 102DEC 7F0CE2BC 8E290004 */  lw    $t1, 4($s1)
/* 102DF0 7F0CE2C0 8EA40000 */  lw    $a0, ($s5)
/* 102DF4 7F0CE2C4 00033582 */  srl   $a2, $v1, 0x16
/* 102DF8 7F0CE2C8 00097602 */  srl   $t6, $t1, 0x18
/* 102DFC 7F0CE2CC 31C900FF */  andi  $t1, $t6, 0xff
/* 102E00 7F0CE2D0 00033D02 */  srl   $a3, $v1, 0x14
/* 102E04 7F0CE2D4 00034482 */  srl   $t0, $v1, 0x12
/* 102E08 7F0CE2D8 00035382 */  srl   $t2, $v1, 0xe
/* 102E0C 7F0CE2DC 00035A82 */  srl   $t3, $v1, 0xa
/* 102E10 7F0CE2E0 30D80003 */  andi  $t8, $a2, 3
/* 102E14 7F0CE2E4 30F90003 */  andi  $t9, $a3, 3
/* 102E18 7F0CE2E8 310D0003 */  andi  $t5, $t0, 3
/* 102E1C 7F0CE2EC 314E000F */  andi  $t6, $t2, 0xf
/* 102E20 7F0CE2F0 316F000F */  andi  $t7, $t3, 0xf
/* 102E24 7F0CE2F4 03003025 */  move  $a2, $t8
/* 102E28 7F0CE2F8 03203825 */  move  $a3, $t9
/* 102E2C 7F0CE2FC 01A04025 */  move  $t0, $t5
/* 102E30 7F0CE300 01C05025 */  move  $t2, $t6
/* 102E34 7F0CE304 1080001B */  beqz  $a0, .L7F0CE374
/* 102E38 7F0CE308 01E05825 */   move  $t3, $t7
/* 102E3C 7F0CE30C 00131880 */  sll   $v1, $s3, 2
/* 102E40 7F0CE310 00831021 */  addu  $v0, $a0, $v1
/* 102E44 7F0CE314 90580000 */  lbu   $t8, ($v0)
/* 102E48 7F0CE318 240100FF */  li    $at, 255
/* 102E4C 7F0CE31C 17010015 */  bne   $t8, $at, .L7F0CE374
/* 102E50 7F0CE320 00000000 */   nop   
/* 102E54 7F0CE324 A0490000 */  sb    $t1, ($v0)
/* 102E58 7F0CE328 8EB90000 */  lw    $t9, ($s5)
/* 102E5C 7F0CE32C 2404000F */  li    $a0, 15
/* 102E60 7F0CE330 008E6823 */  subu  $t5, $a0, $t6
/* 102E64 7F0CE334 03231021 */  addu  $v0, $t9, $v1
/* 102E68 7F0CE338 90590001 */  lbu   $t9, 1($v0)
/* 102E6C 7F0CE33C 01A07025 */  move  $t6, $t5
/* 102E70 7F0CE340 000EC140 */  sll   $t8, $t6, 5
/* 102E74 7F0CE344 332DFF1F */  andi  $t5, $t9, 0xff1f
/* 102E78 7F0CE348 030D7025 */  or    $t6, $t8, $t5
/* 102E7C 7F0CE34C A04E0001 */  sb    $t6, 1($v0)
/* 102E80 7F0CE350 8EAF0000 */  lw    $t7, ($s5)
/* 102E84 7F0CE354 008BC023 */  subu  $t8, $a0, $t3
/* 102E88 7F0CE358 00186880 */  sll   $t5, $t8, 2
/* 102E8C 7F0CE35C 01E31021 */  addu  $v0, $t7, $v1
/* 102E90 7F0CE360 904F0001 */  lbu   $t7, 1($v0)
/* 102E94 7F0CE364 31AE001C */  andi  $t6, $t5, 0x1c
/* 102E98 7F0CE368 31F9FFE3 */  andi  $t9, $t7, 0xffe3
/* 102E9C 7F0CE36C 01D9C025 */  or    $t8, $t6, $t9
/* 102EA0 7F0CE370 A0580001 */  sb    $t8, 1($v0)
.L7F0CE374:
/* 102EA4 7F0CE374 12800024 */  beqz  $s4, .L7F0CE408
/* 102EA8 7F0CE378 02002025 */   move  $a0, $s0
/* 102EAC 7F0CE37C 8EAD0000 */  lw    $t5, ($s5)
/* 102EB0 7F0CE380 00137880 */  sll   $t7, $s3, 2
/* 102EB4 7F0CE384 AFA80010 */  sw    $t0, 0x10($sp)
/* 102EB8 7F0CE388 01AF1021 */  addu  $v0, $t5, $t7
/* 102EBC 7F0CE38C 904E0001 */  lbu   $t6, 1($v0)
/* 102EC0 7F0CE390 02002025 */  move  $a0, $s0
/* 102EC4 7F0CE394 02402825 */  move  $a1, $s2
/* 102EC8 7F0CE398 000EC942 */  srl   $t9, $t6, 5
/* 102ECC 7F0CE39C 02F9C023 */  subu  $t8, $s7, $t9
/* 102ED0 7F0CE3A0 AFB80014 */  sw    $t8, 0x14($sp)
/* 102ED4 7F0CE3A4 8C4D0000 */  lw    $t5, ($v0)
/* 102ED8 7F0CE3A8 000D7AC0 */  sll   $t7, $t5, 0xb
/* 102EDC 7F0CE3AC 000F7742 */  srl   $t6, $t7, 0x1d
/* 102EE0 7F0CE3B0 02EEC823 */  subu  $t9, $s7, $t6
/* 102EE4 7F0CE3B4 AFB90018 */  sw    $t9, 0x18($sp)
/* 102EE8 7F0CE3B8 90580000 */  lbu   $t8, ($v0)
/* 102EEC 7F0CE3BC 0FC337EA */  jal   texHandleType0
/* 102EF0 7F0CE3C0 AFB8001C */   sw    $t8, 0x1c($sp)
/* 102EF4 7F0CE3C4 3C0DBA00 */  lui   $t5, (0xBA001402 >> 16) # lui $t5, 0xba00
/* 102EF8 7F0CE3C8 35AD1402 */  ori   $t5, (0xBA001402 & 0xFFFF) # ori $t5, $t5, 0x1402
/* 102EFC 7F0CE3CC AC4D0000 */  sw    $t5, ($v0)
/* 102F00 7F0CE3D0 3C0EBA00 */  lui   $t6, (0xBA001001 >> 16) # lui $t6, 0xba00
/* 102F04 7F0CE3D4 3C18BA00 */  lui   $t8, (0xBA001102 >> 16) # lui $t8, 0xba00
/* 102F08 7F0CE3D8 3C0F0010 */  lui   $t7, 0x10
/* 102F0C 7F0CE3DC 35CE1001 */  ori   $t6, (0xBA001001 & 0xFFFF) # ori $t6, $t6, 0x1001
/* 102F10 7F0CE3E0 3C190001 */  lui   $t9, 1
/* 102F14 7F0CE3E4 37181102 */  ori   $t8, (0xBA001102 & 0xFFFF) # ori $t8, $t8, 0x1102
/* 102F18 7F0CE3E8 3C0D0004 */  lui   $t5, 4
/* 102F1C 7F0CE3EC AC4F0004 */  sw    $t7, 4($v0)
/* 102F20 7F0CE3F0 AC59000C */  sw    $t9, 0xc($v0)
/* 102F24 7F0CE3F4 AC4E0008 */  sw    $t6, 8($v0)
/* 102F28 7F0CE3F8 AC4D0014 */  sw    $t5, 0x14($v0)
/* 102F2C 7F0CE3FC AC580010 */  sw    $t8, 0x10($v0)
/* 102F30 7F0CE400 1000007B */  b     .L7F0CE5F0
/* 102F34 7F0CE404 24500018 */   addiu $s0, $v0, 0x18
.L7F0CE408:
/* 102F38 7F0CE408 02402825 */  move  $a1, $s2
/* 102F3C 7F0CE40C AFA80010 */  sw    $t0, 0x10($sp)
/* 102F40 7F0CE410 AFAA0014 */  sw    $t2, 0x14($sp)
/* 102F44 7F0CE414 AFAB0018 */  sw    $t3, 0x18($sp)
/* 102F48 7F0CE418 0FC337EA */  jal   texHandleType0
/* 102F4C 7F0CE41C AFA9001C */   sw    $t1, 0x1c($sp)
/* 102F50 7F0CE420 10000073 */  b     .L7F0CE5F0
/* 102F54 7F0CE424 00408025 */   move  $s0, $v0
.L7F0CE428:
/* 102F58 7F0CE428 8E240004 */  lw    $a0, 4($s1)
/* 102F5C 7F0CE42C 02C02825 */  move  $a1, $s6
/* 102F60 7F0CE430 00047B02 */  srl   $t7, $a0, 0xc
/* 102F64 7F0CE434 31E40FFF */  andi  $a0, $t7, 0xfff
/* 102F68 7F0CE438 0FC32FBA */  jal   texLoadFromTextureNum
/* 102F6C 7F0CE43C AFA40080 */   sw    $a0, 0x80($sp)
/* 102F70 7F0CE440 8FA40080 */  lw    $a0, 0x80($sp)
/* 102F74 7F0CE444 0FC32EC3 */  jal   texFindInPool
/* 102F78 7F0CE448 02C02825 */   move  $a1, $s6
/* 102F7C 7F0CE44C 10400068 */  beqz  $v0, .L7F0CE5F0
/* 102F80 7F0CE450 02002025 */   move  $a0, $s0
/* 102F84 7F0CE454 8E230000 */  lw    $v1, ($s1)
/* 102F88 7F0CE458 8E290004 */  lw    $t1, 4($s1)
/* 102F8C 7F0CE45C AFA20014 */  sw    $v0, 0x14($sp)
/* 102F90 7F0CE460 00033582 */  srl   $a2, $v1, 0x16
/* 102F94 7F0CE464 0009CE02 */  srl   $t9, $t1, 0x18
/* 102F98 7F0CE468 332900FF */  andi  $t1, $t9, 0xff
/* 102F9C 7F0CE46C 00033D02 */  srl   $a3, $v1, 0x14
/* 102FA0 7F0CE470 00034482 */  srl   $t0, $v1, 0x12
/* 102FA4 7F0CE474 00035382 */  srl   $t2, $v1, 0xe
/* 102FA8 7F0CE478 00035A82 */  srl   $t3, $v1, 0xa
/* 102FAC 7F0CE47C 30CD0003 */  andi  $t5, $a2, 3
/* 102FB0 7F0CE480 30EF0003 */  andi  $t7, $a3, 3
/* 102FB4 7F0CE484 310E0003 */  andi  $t6, $t0, 3
/* 102FB8 7F0CE488 3159000F */  andi  $t9, $t2, 0xf
/* 102FBC 7F0CE48C 3178000F */  andi  $t8, $t3, 0xf
/* 102FC0 7F0CE490 01A03025 */  move  $a2, $t5
/* 102FC4 7F0CE494 01E03825 */  move  $a3, $t7
/* 102FC8 7F0CE498 AFB8001C */  sw    $t8, 0x1c($sp)
/* 102FCC 7F0CE49C AFB90018 */  sw    $t9, 0x18($sp)
/* 102FD0 7F0CE4A0 AFAE0010 */  sw    $t6, 0x10($sp)
/* 102FD4 7F0CE4A4 AFA90020 */  sw    $t1, 0x20($sp)
/* 102FD8 7F0CE4A8 0FC337AA */  jal   texHandleType1
/* 102FDC 7F0CE4AC 02402825 */   move  $a1, $s2
/* 102FE0 7F0CE4B0 1000004F */  b     .L7F0CE5F0
/* 102FE4 7F0CE4B4 00408025 */   move  $s0, $v0
.L7F0CE4B8:
/* 102FE8 7F0CE4B8 00033582 */  srl   $a2, $v1, 0x16
/* 102FEC 7F0CE4BC 00033D02 */  srl   $a3, $v1, 0x14
/* 102FF0 7F0CE4C0 00034482 */  srl   $t0, $v1, 0x12
/* 102FF4 7F0CE4C4 30CD0003 */  andi  $t5, $a2, 3
/* 102FF8 7F0CE4C8 30EF0003 */  andi  $t7, $a3, 3
/* 102FFC 7F0CE4CC 310E0003 */  andi  $t6, $t0, 3
/* 103000 7F0CE4D0 01A03025 */  move  $a2, $t5
/* 103004 7F0CE4D4 01E03825 */  move  $a3, $t7
/* 103008 7F0CE4D8 12800024 */  beqz  $s4, .L7F0CE56C
/* 10300C 7F0CE4DC 01C04025 */   move  $t0, $t6
/* 103010 7F0CE4E0 8EB90000 */  lw    $t9, ($s5)
/* 103014 7F0CE4E4 0013C080 */  sll   $t8, $s3, 2
/* 103018 7F0CE4E8 AFAE0010 */  sw    $t6, 0x10($sp)
/* 10301C 7F0CE4EC 03381021 */  addu  $v0, $t9, $t8
/* 103020 7F0CE4F0 904D0001 */  lbu   $t5, 1($v0)
/* 103024 7F0CE4F4 02002025 */  move  $a0, $s0
/* 103028 7F0CE4F8 02402825 */  move  $a1, $s2
/* 10302C 7F0CE4FC 000D7942 */  srl   $t7, $t5, 5
/* 103030 7F0CE500 02EF7023 */  subu  $t6, $s7, $t7
/* 103034 7F0CE504 AFAE0014 */  sw    $t6, 0x14($sp)
/* 103038 7F0CE508 8C590000 */  lw    $t9, ($v0)
/* 10303C 7F0CE50C 0019C2C0 */  sll   $t8, $t9, 0xb
/* 103040 7F0CE510 00186F42 */  srl   $t5, $t8, 0x1d
/* 103044 7F0CE514 02ED7823 */  subu  $t7, $s7, $t5
/* 103048 7F0CE518 AFAF0018 */  sw    $t7, 0x18($sp)
/* 10304C 7F0CE51C 904E0000 */  lbu   $t6, ($v0)
/* 103050 7F0CE520 0FC337EA */  jal   texHandleType0
/* 103054 7F0CE524 AFAE001C */   sw    $t6, 0x1c($sp)
/* 103058 7F0CE528 3C19BA00 */  lui   $t9, (0xBA001402 >> 16) # lui $t9, 0xba00
/* 10305C 7F0CE52C 37391402 */  ori   $t9, (0xBA001402 & 0xFFFF) # ori $t9, $t9, 0x1402
/* 103060 7F0CE530 AC590000 */  sw    $t9, ($v0)
/* 103064 7F0CE534 3C0DBA00 */  lui   $t5, (0xBA001001 >> 16) # lui $t5, 0xba00
/* 103068 7F0CE538 3C0EBA00 */  lui   $t6, (0xBA001102 >> 16) # lui $t6, 0xba00
/* 10306C 7F0CE53C 3C180010 */  lui   $t8, 0x10
/* 103070 7F0CE540 35AD1001 */  ori   $t5, (0xBA001001 & 0xFFFF) # ori $t5, $t5, 0x1001
/* 103074 7F0CE544 3C0F0001 */  lui   $t7, 1
/* 103078 7F0CE548 35CE1102 */  ori   $t6, (0xBA001102 & 0xFFFF) # ori $t6, $t6, 0x1102
/* 10307C 7F0CE54C 3C190004 */  lui   $t9, 4
/* 103080 7F0CE550 AC580004 */  sw    $t8, 4($v0)
/* 103084 7F0CE554 AC4F000C */  sw    $t7, 0xc($v0)
/* 103088 7F0CE558 AC4D0008 */  sw    $t5, 8($v0)
/* 10308C 7F0CE55C AC590014 */  sw    $t9, 0x14($v0)
/* 103090 7F0CE560 AC4E0010 */  sw    $t6, 0x10($v0)
/* 103094 7F0CE564 10000022 */  b     .L7F0CE5F0
/* 103098 7F0CE568 24500018 */   addiu $s0, $v0, 0x18
.L7F0CE56C:
/* 10309C 7F0CE56C 02002025 */  move  $a0, $s0
/* 1030A0 7F0CE570 02402825 */  move  $a1, $s2
/* 1030A4 7F0CE574 0FC33786 */  jal   texHandleType2
/* 1030A8 7F0CE578 AFA80010 */   sw    $t0, 0x10($sp)
/* 1030AC 7F0CE57C 1000001C */  b     .L7F0CE5F0
/* 1030B0 7F0CE580 00408025 */   move  $s0, $v0
.L7F0CE584:
/* 1030B4 7F0CE584 00033582 */  srl   $a2, $v1, 0x16
/* 1030B8 7F0CE588 00033D02 */  srl   $a3, $v1, 0x14
/* 1030BC 7F0CE58C 00034482 */  srl   $t0, $v1, 0x12
/* 1030C0 7F0CE590 30D80003 */  andi  $t8, $a2, 3
/* 1030C4 7F0CE594 30ED0003 */  andi  $t5, $a3, 3
/* 1030C8 7F0CE598 310F0003 */  andi  $t7, $t0, 3
/* 1030CC 7F0CE59C 03003025 */  move  $a2, $t8
/* 1030D0 7F0CE5A0 01A03825 */  move  $a3, $t5
/* 1030D4 7F0CE5A4 AFAF0010 */  sw    $t7, 0x10($sp)
/* 1030D8 7F0CE5A8 02002025 */  move  $a0, $s0
/* 1030DC 7F0CE5AC 0FC3382B */  jal   texHandleType3
/* 1030E0 7F0CE5B0 02402825 */   move  $a1, $s2
/* 1030E4 7F0CE5B4 1000000E */  b     .L7F0CE5F0
/* 1030E8 7F0CE5B8 00408025 */   move  $s0, $v0
.L7F0CE5BC:
/* 1030EC 7F0CE5BC 00033582 */  srl   $a2, $v1, 0x16
/* 1030F0 7F0CE5C0 00033D02 */  srl   $a3, $v1, 0x14
/* 1030F4 7F0CE5C4 00034482 */  srl   $t0, $v1, 0x12
/* 1030F8 7F0CE5C8 30CE0003 */  andi  $t6, $a2, 3
/* 1030FC 7F0CE5CC 30F90003 */  andi  $t9, $a3, 3
/* 103100 7F0CE5D0 31180003 */  andi  $t8, $t0, 3
/* 103104 7F0CE5D4 01C03025 */  move  $a2, $t6
/* 103108 7F0CE5D8 03203825 */  move  $a3, $t9
/* 10310C 7F0CE5DC AFB80010 */  sw    $t8, 0x10($sp)
/* 103110 7F0CE5E0 02002025 */  move  $a0, $s0
/* 103114 7F0CE5E4 0FC33819 */  jal   texHandleType4
/* 103118 7F0CE5E8 02402825 */   move  $a1, $s2
/* 10311C 7F0CE5EC 00408025 */  move  $s0, $v0
def_7F0CE2B4:
.L7F0CE5F0:
/* 103120 7F0CE5F0 240105E4 */  li    $at, 1508
/* 103124 7F0CE5F4 16610005 */  bne   $s3, $at, .L7F0CE60C
/* 103128 7F0CE5F8 02002025 */   move  $a0, $s0
/* 10312C 7F0CE5FC 0FC24D0F */  jal   sub_GAME_7F09343C
/* 103130 7F0CE600 24050001 */   li    $a1, 1
/* 103134 7F0CE604 0FC33132 */  jal   sub_GAME_7F0CC4C8
/* 103138 7F0CE608 00408025 */   move  $s0, $v0
.L7F0CE60C:
/* 10313C 7F0CE60C 240105E7 */  li    $at, 1511
/* 103140 7F0CE610 16610005 */  bne   $s3, $at, .L7F0CE628
/* 103144 7F0CE614 02002025 */   move  $a0, $s0
/* 103148 7F0CE618 0FC24D97 */  jal   sub_GAME_7F09365C
/* 10314C 7F0CE61C 24050001 */   li    $a1, 1
/* 103150 7F0CE620 0FC33132 */  jal   sub_GAME_7F0CC4C8
/* 103154 7F0CE624 00408025 */   move  $s0, $v0
.L7F0CE628:
/* 103158 7F0CE628 0FC2ED8C */  jal   check_if_imageID_is_light
/* 10315C 7F0CE62C 02602025 */   move  $a0, $s3
/* 103160 7F0CE630 10400004 */  beqz  $v0, .L7F0CE644
/* 103164 7F0CE634 02002025 */   move  $a0, $s0
/* 103168 7F0CE638 240D0001 */  li    $t5, 1
/* 10316C 7F0CE63C 0FC2ED67 */  jal   add_entry_to_init_lightfixture_table
/* 103170 7F0CE640 AFAD0058 */   sw    $t5, 0x58($sp)
.L7F0CE644:
/* 103174 7F0CE644 1000003C */  b     .L7F0CE738
/* 103178 7F0CE648 26310008 */   addiu $s1, $s1, 8
/* 10317C 7F0CE64C 8E210000 */  lw    $at, ($s1)
.L7F0CE650:
/* 103180 7F0CE650 241E0001 */  li    $fp, 1
/* 103184 7F0CE654 26100008 */  addiu $s0, $s0, 8
/* 103188 7F0CE658 AE01FFF8 */  sw    $at, -8($s0)
/* 10318C 7F0CE65C 8E390004 */  lw    $t9, 4($s1)
/* 103190 7F0CE660 26310008 */  addiu $s1, $s1, 8
/* 103194 7F0CE664 10000034 */  b     .L7F0CE738
/* 103198 7F0CE668 AE19FFFC */   sw    $t9, -4($s0)
.L7F0CE66C:
/* 10319C 7F0CE66C 24180001 */  li    $t8, 1
/* 1031A0 7F0CE670 AFB8005C */  sw    $t8, 0x5c($sp)
/* 1031A4 7F0CE674 8E210000 */  lw    $at, ($s1)
/* 1031A8 7F0CE678 0000F025 */  move  $fp, $zero
/* 1031AC 7F0CE67C 26100008 */  addiu $s0, $s0, 8
/* 1031B0 7F0CE680 AE01FFF8 */  sw    $at, -8($s0)
/* 1031B4 7F0CE684 8E2F0004 */  lw    $t7, 4($s1)
/* 1031B8 7F0CE688 26310008 */  addiu $s1, $s1, 8
/* 1031BC 7F0CE68C 1000002A */  b     .L7F0CE738
/* 1031C0 7F0CE690 AE0FFFFC */   sw    $t7, -4($s0)
.L7F0CE694:
/* 1031C4 7F0CE694 AFB0008C */  sw    $s0, 0x8c($sp)
/* 1031C8 7F0CE698 AFA0005C */  sw    $zero, 0x5c($sp)
/* 1031CC 7F0CE69C 8E210000 */  lw    $at, ($s1)
/* 1031D0 7F0CE6A0 26100008 */  addiu $s0, $s0, 8
/* 1031D4 7F0CE6A4 26310008 */  addiu $s1, $s1, 8
/* 1031D8 7F0CE6A8 AE01FFF8 */  sw    $at, -8($s0)
/* 1031DC 7F0CE6AC 8E2EFFFC */  lw    $t6, -4($s1)
/* 1031E0 7F0CE6B0 10000021 */  b     .L7F0CE738
/* 1031E4 7F0CE6B4 AE0EFFFC */   sw    $t6, -4($s0)
.L7F0CE6B8:
/* 1031E8 7F0CE6B8 52800013 */  beql  $s4, $zero, .L7F0CE708
/* 1031EC 7F0CE6BC 8E210000 */   lw    $at, ($s1)
/* 1031F0 7F0CE6C0 82220002 */  lb    $v0, 2($s1)
/* 1031F4 7F0CE6C4 24010011 */  li    $at, 17
/* 1031F8 7F0CE6C8 10410005 */  beq   $v0, $at, .L7F0CE6E0
/* 1031FC 7F0CE6CC 24010014 */   li    $at, 20
/* 103200 7F0CE6D0 10410003 */  beq   $v0, $at, .L7F0CE6E0
/* 103204 7F0CE6D4 24010010 */   li    $at, 16
/* 103208 7F0CE6D8 54410004 */  bnel  $v0, $at, .L7F0CE6EC
/* 10320C 7F0CE6DC 8E210000 */   lw    $at, ($s1)
.L7F0CE6E0:
/* 103210 7F0CE6E0 10000015 */  b     .L7F0CE738
/* 103214 7F0CE6E4 26310008 */   addiu $s1, $s1, 8
/* 103218 7F0CE6E8 8E210000 */  lw    $at, ($s1)
.L7F0CE6EC:
/* 10321C 7F0CE6EC 26100008 */  addiu $s0, $s0, 8
/* 103220 7F0CE6F0 26310008 */  addiu $s1, $s1, 8
/* 103224 7F0CE6F4 AE01FFF8 */  sw    $at, -8($s0)
/* 103228 7F0CE6F8 8E2FFFFC */  lw    $t7, -4($s1)
/* 10322C 7F0CE6FC 1000000E */  b     .L7F0CE738
/* 103230 7F0CE700 AE0FFFFC */   sw    $t7, -4($s0)
/* 103234 7F0CE704 8E210000 */  lw    $at, ($s1)
.L7F0CE708:
/* 103238 7F0CE708 26100008 */  addiu $s0, $s0, 8
/* 10323C 7F0CE70C 26310008 */  addiu $s1, $s1, 8
/* 103240 7F0CE710 AE01FFF8 */  sw    $at, -8($s0)
/* 103244 7F0CE714 8E39FFFC */  lw    $t9, -4($s1)
/* 103248 7F0CE718 10000007 */  b     .L7F0CE738
/* 10324C 7F0CE71C AE19FFFC */   sw    $t9, -4($s0)
.L7F0CE720:
/* 103250 7F0CE720 8E210000 */  lw    $at, ($s1)
.L7F0CE724:
/* 103254 7F0CE724 26100008 */  addiu $s0, $s0, 8
/* 103258 7F0CE728 26310008 */  addiu $s1, $s1, 8
/* 10325C 7F0CE72C AE01FFF8 */  sw    $at, -8($s0)
/* 103260 7F0CE730 8E2DFFFC */  lw    $t5, -4($s1)
/* 103264 7F0CE734 AE0DFFFC */  sw    $t5, -4($s0)
.L7F0CE738:
/* 103268 7F0CE738 8FAF0088 */  lw    $t7, 0x88($sp)
/* 10326C 7F0CE73C 25F8FFFF */  addiu $t8, $t7, -1
/* 103270 7F0CE740 1F00FE94 */  bgtz  $t8, .L7F0CE194
/* 103274 7F0CE744 AFB80088 */   sw    $t8, 0x88($sp)
.L7F0CE748:
/* 103278 7F0CE748 8FB90058 */  lw    $t9, 0x58($sp)
/* 10327C 7F0CE74C 53200004 */  beql  $t9, $zero, .L7F0CE760
/* 103280 7F0CE750 8FAE00A0 */   lw    $t6, 0xa0($sp)
/* 103284 7F0CE754 0FC2ED80 */  jal   save_ptrDL_enpoint_to_current_init_lightfixture_table
/* 103288 7F0CE758 02002025 */   move  $a0, $s0
/* 10328C 7F0CE75C 8FAE00A0 */  lw    $t6, 0xa0($sp)
.L7F0CE760:
/* 103290 7F0CE760 8FBF0054 */  lw    $ra, 0x54($sp)
/* 103294 7F0CE764 8FB10034 */  lw    $s1, 0x34($sp)
/* 103298 7F0CE768 020E1023 */  subu  $v0, $s0, $t6
/* 10329C 7F0CE76C 8FB00030 */  lw    $s0, 0x30($sp)
/* 1032A0 7F0CE770 8FB20038 */  lw    $s2, 0x38($sp)
/* 1032A4 7F0CE774 8FB3003C */  lw    $s3, 0x3c($sp)
/* 1032A8 7F0CE778 8FB40040 */  lw    $s4, 0x40($sp)
/* 1032AC 7F0CE77C 8FB50044 */  lw    $s5, 0x44($sp)
/* 1032B0 7F0CE780 8FB60048 */  lw    $s6, 0x48($sp)
/* 1032B4 7F0CE784 8FB7004C */  lw    $s7, 0x4c($sp)
/* 1032B8 7F0CE788 8FBE0050 */  lw    $fp, 0x50($sp)
/* 1032BC 7F0CE78C 03E00008 */  jr    $ra
/* 1032C0 7F0CE790 27BD0098 */   addiu $sp, $sp, 0x98
)
#endif


/***
 * NTSC address 0x7F0CE794.
*/
void texCopyGdls(Gfx *arg0, Gfx *arg1, s32 arg2)
{
    arg2 = (arg2 >> 3);
    arg0 = arg0 + (arg2 - 1);
    arg1 = arg1 + (arg2 - 1);

    while (arg2--)
    {
        arg1->force_structure_alignment = arg0->force_structure_alignment;
        arg1--;
        arg0--;
    }
}
