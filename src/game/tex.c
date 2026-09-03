#include <ultra64.h>
#include <R4300.h>
#include "bondtypes.h"
#include "gbi_extension.h"
#include "lightfixture.h"
#include "lv.h"
#include "image.h"
#include "tex.h"

s32 g_TexLutMode;
s_bss_8008D148 g_TexTileStates[8]; //[0x160]; //8 entries of 0x2c ea
s_bss_8008D2A8 g_TexTileSizes[8]; //[0xA0]; //8 entries of 0x14 ea

u16 default_hit_sfx[] = {HIT_BULLET_STONE1_SFX, HIT_BULLET_STONE2_SFX};
u8 default_impact_types[] = {0x7, 0x0, 0x0, 0x0};
struct image_sound isnd_default = {default_hit_sfx, default_impact_types, 0x0, 0x1};

u16 stone_hit_sfx[] = {HIT_BULLET_STONE1_SFX, HIT_BULLET_STONE2_SFX};
u8 stone_impact_types[] = {0x1, 0x0, 0x0, 0x0};
struct image_sound isnd_stone = {stone_hit_sfx, stone_impact_types, 0x2, 0x1};

u16 wood_hit_sfx[] = {HIT_BULLET_WOOD_SFX, HIT_BULLET_WOOD2_SFX};
u8 wood_impact_types[] = {0x1, 0x0, 0x0, 0x0};
struct image_sound isnd_wood = {wood_hit_sfx, wood_impact_types, 0x2, 0x1};

u16 metal_hit_sfx[] = {HIT_BULLET_METAL_A_SFX, HIT_BULLET_METAL_A4_SFX, HIT_BULLET_METAL_A3_SFX, 0x00};
u8 metal_impact_types[] = {0x7, 0x0, 0x0, 0x0};
struct image_sound isnd_metal = {metal_hit_sfx, metal_impact_types, 0x3, 0x1};

u16 glass_hit_sfx[] = {HIT_BULLET_GLASS_SFX, 0x00};
u8 glass_impact_types[] = {0x4, 0x5, 0x6, 0x0};
struct image_sound isnd_glass = {glass_hit_sfx, glass_impact_types, 0x1, 0x3};

u16 water_hit_sfx[] = {HIT_BULLET_WATER_SFX, 0x00};
u8 water_impact_types[] = {0, 0, 0, 0};
struct image_sound isnd_water = {water_hit_sfx, water_impact_types, 0x1, 0x0};

u16 snow_hit_sfx[] = {HIT_BULLET_SNOW_SFX, 0x00};
u8 snow_impact_types[] = {0x1, 0x0, 0x0, 0x0};
struct image_sound isnd_snow = {snow_hit_sfx, snow_impact_types, 0x1, 0x1};

u16 dirt_hit_sfx[] = {HIT_BULLET_DIRT1_SFX, HIT_BULLET_DIRT2_SFX};
u8 dirt_impact_types[] = {0x2, 0x0, 0x0, 0x0};
struct image_sound isnd_dirt = {dirt_hit_sfx, dirt_impact_types, 0x2, 0x1};

u16 mud_hit_sfx[] = {HIT_BULLET_MUD1_SFX, HIT_BULLET_MUD2_SFX, HIT_BULLET_MUD3_SFX, 0x00};
u8 mud_impact_types[] = {0x2, 0x0, 0x0, 0x0};
struct image_sound isnd_mud = {mud_hit_sfx, mud_impact_types, 0x3, 0x1};

u16 tile_hit_sfx[] = {HIT_BULLET_TILE_SFX, 0x00};
u8 tile_impact_types[] = {0x1, 0x0, 0x0, 0x0};
struct image_sound isnd_tile = {tile_hit_sfx, tile_impact_types, 0x1, 0x1};

u16 metal_object_hit_sfx[] = {HIT_METAL_OBJECT1_SFX, HIT_METAL_OBJECT2_SFX};
u8 metal_object_impact_types[] = {0x1, 0x7, 0x0, 0x0};
struct image_sound isnd_metalobj = {metal_object_hit_sfx, metal_object_impact_types, 0x2, 0x2};

u16 character_hit_sfx[] = {HIT_BULLET_FLESH_SFX, 0x00};
u8 character_impact_types[] = {0x2, 0x0, 0x0, 0x0};
struct image_sound isnd_chr = {character_hit_sfx, character_impact_types, 0x1, 0x1};

u16 translucent_glass_hit_sfx[] = {HIT_BULLET_GLASS_SFX, 0x00};
u8 translucent_glass_impact_types[] = {0x11, 0x12, 0x13, 0x0};
struct image_sound isnd_glass_xlu = {translucent_glass_hit_sfx, translucent_glass_impact_types, 0x1, 0x3};

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


void sub_GAME_7F0CC4C8(void)
{
    s32 i;

    for (i = 0; i <8 ; i++)
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





const char PADDINGHACKING[4] = "";

s32 texLoadFromGdl(Gfx *src, s32 srcsize, Gfx *dst, void *texpool)
{
    s32         texnum;
    struct tex *tex;
    Gfx        *saved = NULL;
    s32         count;
    s32         valid = FALSE;
    s32         texnum2;
    s32         syncEmitted = FALSE;
    s32         smode;
    s32         tmode;
    s32         offset;
    s32         shifts;
    s32         shiftt;
    s32         min;
    s32         pad;
    s32         writeTexFlag = TRUE;
    s32         lightPending = FALSE;
    Gfx        *in           = src;
    Gfx        *out          = dst;

    count = srcsize >> 3;

    sub_GAME_7F0CC4C8();

    if (texpool == NULL)
    {
        texpool = &ptr_texture_alloc_start;
    }

    while (count > 0)
    {
        switch (*(u8 *)in)
        {
            case G_NOOP:
                if (!syncEmitted)
                {
                    gDPPipeSync(out++);
                    syncEmitted = TRUE;
                }

                if (lightPending)
                {
                    lightFixtureEntryEnd(out);
                    lightPending = FALSE;
                }

                texnum = in->words.w1 & 0xfff;

                if ((texnum == g_LastImpactTexNum) && (D_800483C8 != NULL))
                {
                    if (D_800483C8[texnum].unk_0_0 < 0xffU)
                    {
                        valid = TRUE;
                    }
                    else
                    {
                        valid = FALSE;
                    }
                }
                else
                {
                    valid = FALSE;
                }

                texLoadFromTextureNum(texnum, texpool);
                tex = texFindInPool(texnum, texpool);

                if (tex != NULL)
                {
                    out          = texWriteTextureCmd(out, saved, tex, writeTexFlag);
                    writeTexFlag = FALSE;

                    switch (in->words.w0 & 7)
                    {
                        case TEXTURETYPE_LOD:
                            min    = (in->words.w1 >> 24) & 0xff;
                            smode  = (in->words.w0 >> 22) & 3;
                            tmode  = (in->words.w0 >> 20) & 3;
                            offset = (in->words.w0 >> 18) & 3;
                            shifts = (in->words.w0 >> 14) & 0xf;
                            shiftt = (in->words.w0 >> 10) & 0xf;

                            if ((D_800483C8 != NULL) && (D_800483C8[texnum].unk_0_0 == 0xff))
                            {
                                do
                                {
                                    if (TRUE) {}
                                } while (FALSE);

                                D_800483C8[texnum].unk_0_0 = min;
                                D_800483C8[texnum].unk_1_3 = 15 - shifts;
                                D_800483C8[texnum].unk_1_0 = 15 - shiftt;
                            }

                            if (valid)
                            {
                                out = texHandleType0(
                                    out,
                                    tex,
                                    smode,
                                    tmode,
                                    offset,
                                    15 - D_800483C8[texnum].unk_1_3,
                                    15 - D_800483C8[texnum].unk_1_0,
                                    D_800483C8[texnum].unk_0_0);

                                gDPSetCycleType(out++, G_CYC_2CYCLE);
                                gDPSetTextureLOD(out++, G_TL_LOD);
                                gDPSetTextureDetail(out++, G_TD_DETAIL);
                            }
                            else
                            {
                                out = texHandleType0(out, tex, smode, tmode, offset, shifts, shiftt, min);
                            }
                            break;

                        case TEXTURETYPE_DETAIL:
                        {
                            struct tex *tex2;

                            texnum2 = (in->words.w1 >> 12) & 0xfff;

                            texLoadFromTextureNum(texnum2, texpool);
                            tex2 = texFindInPool(texnum2, texpool);

                            if (tex2 != NULL)
                            {
                                min    = ((*in).words.w1 >> 24) & 0xff;
                                smode  = (in->words.w0 >> 22) & 3;
                                tmode  = (in->words.w0 >> 20) & 3;
                                offset = (in->words.w0 >> 18) & 3;
                                shifts = (in->words.w0 >> 14) & 0xf;
                                shiftt = (in->words.w0 >> 10) & 0xf;

                                out = texHandleType1(out, tex, smode, tmode, offset, tex2, shifts, shiftt, min);
                            }
                            break;
                        }

                        case TEXTURETYPE_MIPMAP:
                            smode  = (in->words.w0 >> 22) & 3;
                            tmode  = (in->words.w0 >> 20) & 3;
                            offset = (in->words.w0 >> 18) & 3;

                            if (valid)
                            {
                                out = texHandleType0(
                                    out,
                                    tex,
                                    smode,
                                    tmode,
                                    offset,
                                    15 - D_800483C8[texnum].unk_1_3,
                                    15 - D_800483C8[texnum].unk_1_0,
                                    D_800483C8[texnum].unk_0_0);

                                gDPSetCycleType(out++, G_CYC_2CYCLE);
                                gDPSetTextureLOD(out++, G_TL_LOD);
                                gDPSetTextureDetail(out++, G_TD_DETAIL);
                            }
                            else
                            {
                                out = texHandleType2(out, tex, smode, tmode, offset);
                            }
                            break;

                        case TEXTURETYPE_TILE:
                            smode  = (in->words.w0 >> 22) & 3;
                            tmode  = (in->words.w0 >> 20) & 3;
                            offset = (in->words.w0 >> 18) & 3;
                            out    = texHandleType3(out, tex, smode, tmode, offset);
                            break;

                        case TEXTURETYPE_TILE_PRESWAPPED:
                            smode  = (in->words.w0 >> 22) & 3;
                            tmode  = (in->words.w0 >> 20) & 3;
                            offset = (in->words.w0 >> 18) & 3;
                            out    = texHandleType4(out, tex, smode, tmode, offset);
                            break;
                    }

                    // Caverns water
                    if (texnum == 1508)
                    {
                        out = dyntexConfigureTwoLayerWater(out, TRUE);
                        sub_GAME_7F0CC4C8();
                    }

                    /**
                     * This texture is used on the water in Dam and Complex. You can't actually
                     * see the texture in Complex because it's at the bottom of the pits and the
                     * vertex coloring is pitch black. You can still tell it's water by
                     * the splash sound it makes when shot.
                     */
                    if (texnum == 1511)
                    {
                        out = dyntexConfigureTwoLayerCiWater(out);
                        sub_GAME_7F0CC4C8();
                    }

                    if (check_if_imageID_is_light(texnum))
                    {
                        lightFixtureEntryBegin(out);
                        lightPending = TRUE;
                    }
                }

                in++;
                break;

            case G_RDPPIPESYNC:
                syncEmitted = TRUE;
                *(out++)    = *(in++);
                break;

            case 0xb1:
            case 0xbf:
                writeTexFlag = TRUE;
                syncEmitted  = FALSE;
                *(out++)     = *(in++);
                break;

            case 0xbb:
                saved        = out;
                writeTexFlag = FALSE;
                *(out++)     = *(in++);
                break;

            case 0xba:
                if (valid)
                {
                    if ((((*(((s8 *)in) + 2)) == 17) || ((*(((s8 *)in) + 2)) == 20)) || ((*(((s8 *)in) + 2)) == 16))
                    {
                        in++;
                    }
                    else
                    {
                        *(out++) = *(in++);
                    }
                }
                else
                {
                    *(out++) = *(in++);
                }
                break;

            default:
                *(out++) = *(in++);
                break;
        }

        count--;
    }

    if (lightPending)
    {
        lightFixtureEntryEnd(out);
    }

    return ((s32)out) - ((s32)dst);
}


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

s32 is_less_than_certain_power_of_2(int number)
{
    if (number < 2) {
        return 0;
    }

    if (number < 3) {
        return 1;
    }

    if (number < 5) {
        return 2;
    }

    if (number < 9) {
        return 3;
    }

    if (number < 0x11) {
        return 4;
    }

    if (number < 0x21) {
        return 5;
    }

    if (number < 0x41) {
        return 6;
    }

    if (number < 0x81) {
        return 7;
    }

    return 8;
}


s32 ceil8000(s32 arg0)
{
    f32 temp_f0;
    s32 temp_f10;

    temp_f0 = 32768.0f / ((((s32) (arg0 + 15)) / 16) * 16);
    temp_f10 = temp_f0;

    return (temp_f10 & 0xFFFFFFFF)
         + ((s32) ((temp_f0 - temp_f10) + 0.999999f));
}


s32 ceil4000(s32 arg0)
{
    f32 temp_f0;
    s32 temp_f10;

    temp_f0 = 16384.0f / ((((s32) (arg0 + 7)) / 8) * 8);
    temp_f10 = temp_f0;

    return (temp_f10 & 0xFFFFFFFF)
         + ((s32) ((temp_f0 - temp_f10) + 0.999999f));
}


s32 ceil2000(s32 arg0)
{
    f32 temp_f0;
    s32 temp_f10;

    temp_f0 = 8192.0f / ((((s32) (arg0 + 3)) / 4) * 4);
    temp_f10 = temp_f0;

    return (temp_f10 & 0xFFFFFFFF)
         + ((s32) ((temp_f0 - temp_f10) + 0.999999f));
}


s32 ceil1000(s32 arg0)
{
    f32 temp_f0;
    s32 temp_f10;

    temp_f0 = 4096.0f / ((((s32) (arg0 + 3)) / 4) * 4);
    temp_f10 = temp_f0;

    return (temp_f10 & 0xFFFFFFFF)
         + ((s32) ((temp_f0 - temp_f10) + 0.999999f));
}


s32 sub_GAME_7F0767D8(s32 arg0, s32 arg1, s32 arg2)
{
    s32 ret;
    ret = 0;

    if (arg2 <= 0) { arg2 = 1; }

    while (arg2 > 0)
    {
        ret += ((arg0 + 15) / 16) * 4 * arg1;
        arg2--;
        if (arg0 >= 2) { arg0 >>= 1; }
        if (arg1 >= 2) { arg1 >>= 1; }
    }
    return ret;
}


s32 sub_GAME_7F076848(s32 arg0, s32 arg1, s32 arg2)
{
    s32 ret;
    ret = 0;

    if (arg2 <= 0) { arg2 = 1; }

    while (arg2 > 0)
    {
        ret += ((arg0 + 7) / 8) * 4 * arg1;
        arg2--;
        if (arg0 >= 2) { arg0 >>= 1; }
        if (arg1 >= 2) { arg1 >>= 1; }
    }
    return ret;
}


s32 sub_GAME_7F0768B8(s32 arg0, s32 arg1, s32 arg2)
{
    s32 ret;
    ret = 0;

    if (arg2 <= 0) { arg2 = 1; }

    while (arg2 > 0)
    {
        ret += ((arg0 + 3) / 4) * 4 * arg1;
        arg2--;
        if (arg0 >= 2) { arg0 >>= 1; }
        if (arg1 >= 2) { arg1 >>= 1; }
    }
    return ret;
}


s32 sub_GAME_7F076928(s32 arg0, s32 arg1, s32 arg2)
{
    s32 ret;
    ret = 0;

    if (arg2 <= 0) { arg2 = 1; }

    while (arg2 > 0)
    {
        ret += ((arg0 + 3) / 4) * 4 * arg1;
        arg2--;
        if (arg0 >= 2) { arg0 >>= 1; }
        if (arg1 >= 2) { arg1 >>= 1; }
    }
    return ret;
}


void texSetRenderMode(Gfx **gdlptr, TEXTURE_RENDER_STYLE renderStyle, s32 numcycles, s32 zbufferMode)
{
    Gfx *gdl = *gdlptr;

    if (numcycles == 1)
    {
        gDPPipeSync(gdl++);
        gDPSetCycleType(gdl++, G_CYC_1CYCLE);

        switch (renderStyle)
        {
            default:
            case 1:
                if (zbufferMode)
                {
                    if (zbufferMode >= 2)
                    {
                        gDPSetRenderMode(gdl++, G_RM_AA_ZB_OPA_DECAL, G_RM_AA_ZB_OPA_DECAL2);
                    }
                    else
                    {
                        gDPSetRenderMode(gdl++, G_RM_AA_ZB_OPA_SURF, G_RM_AA_ZB_OPA_SURF2);
                    }
                }
                else
                {
                    gDPSetRenderMode(gdl++, G_RM_AA_OPA_SURF, G_RM_AA_OPA_SURF2);
                }
                break;

            case 2:
                if (zbufferMode)
                {
                    if (zbufferMode >= 2)
                    {
                        gDPSetRenderMode(gdl++, G_RM_AA_ZB_XLU_DECAL, G_RM_AA_ZB_XLU_DECAL2);
                    }
                    else
                    {
                        gDPSetRenderMode(gdl++, G_RM_AA_ZB_XLU_SURF, G_RM_AA_ZB_XLU_SURF2);
                    }
                }
                else
                {
                    gDPSetRenderMode(gdl++, G_RM_AA_XLU_SURF, G_RM_AA_XLU_SURF2);
                }
                break;

            case 3:
                if (zbufferMode)
                {
                    gDPSetRenderMode(gdl++, G_RM_AA_ZB_TEX_EDGE, G_RM_AA_ZB_TEX_EDGE2);
                }
                else
                {
                    gDPSetRenderMode(gdl++, G_RM_AA_TEX_EDGE, G_RM_AA_TEX_EDGE2);
                }
                break;

            case 4:
                if (zbufferMode)
                {
                    gDPSetRenderMode(gdl++, G_RM_ZB_CLD_SURF, G_RM_ZB_CLD_SURF2);
                }
                else
                {
                    gDPSetRenderMode(gdl++, G_RM_CLD_SURF, G_RM_CLD_SURF2);
                }
                break;
        }
    }
    else
    {
        gDPPipeSync(gdl++);
        gDPSetCycleType(gdl++, G_CYC_2CYCLE);

        switch (renderStyle)
        {
            default:
            case 1:
                if (zbufferMode)
                {
                    if (zbufferMode >= 2)
                    {
                        gDPSetRenderMode(gdl++, G_RM_PASS, G_RM_AA_ZB_OPA_DECAL2);
                    }
                    else
                    {
                        gDPSetRenderMode(gdl++, G_RM_PASS, G_RM_AA_ZB_OPA_SURF2);
                    }
                }
                else
                {
                    gDPSetRenderMode(gdl++, G_RM_PASS, G_RM_AA_OPA_SURF2);
                }
                break;

            case 2:
                if (zbufferMode)
                {
                    if (zbufferMode >= 2)
                    {
                        gDPSetRenderMode(gdl++, G_RM_PASS, G_RM_AA_ZB_XLU_DECAL2);
                    }
                    else
                    {
                        gDPSetRenderMode(gdl++, G_RM_PASS, G_RM_AA_ZB_XLU_SURF2);
                    }
                }
                else
                {
                    gDPSetRenderMode(gdl++, G_RM_PASS, G_RM_AA_XLU_SURF2);
                }
                break;

            case 3:
                if (zbufferMode)
                {
                    gDPSetRenderMode(gdl++, G_RM_PASS, G_RM_AA_ZB_TEX_EDGE2);
                }
                else
                {
                    gDPSetRenderMode(gdl++, G_RM_PASS, G_RM_AA_TEX_EDGE2);
                }
                break;

            case 4:
                if (zbufferMode)
                {
                    gDPSetRenderMode(gdl++, G_RM_PASS, G_RM_ZB_CLD_SURF2);
                }
                else
                {
                    gDPSetRenderMode(gdl++, G_RM_PASS, G_RM_CLD_SURF2);
                }
                break;
        }
    }

    *gdlptr = gdl;
}


void texSelect(Gfx **gdlptr, struct sImageTableEntry *tconfig, TEXTURE_RENDER_STYLE renderStyle, s32 arg3, u32 ulst)
{    
	Gfx *gdl;
    struct tex *tex;
    s32 tile;

    gdl = *gdlptr;

    if (tconfig == NULL)
    {
        texSetRenderMode(&gdl, renderStyle, 1, arg3);

        if (arg3 >= 2)
        {
            gSPTextureL(gdl++, 0xffff, 0xffff, 0, arg3, G_TX_RENDERTILE, G_ON);
        }
        else
        {
            gSPTextureL(gdl++, 0xffff, 0xffff, 0, 0, G_TX_RENDERTILE, G_ON);
        }

        gDPSetCombineMode(gdl++, G_CC_SHADE, G_CC_SHADE);
    }
    else
    {
        s32 width;
        s32 height;
        
		u8 format;
		u8 depth;
        s32 stack_padding;
		s32 lutmode;
		s32 depth2;
		s32 lrs;
        s32 sp138;
		s32 line;
            
        u16* aa;

        tex = NULL;
        
        width = tconfig->width;
        height = tconfig->height;
        
        if ((u32) tconfig->index < NUM_TEXTURES)
        {
            texLoad((s32 *)tconfig, NULL);
        }

        aa = PHYS_TO_K0(tconfig->index);
        tex = texFindInPool((aa)[-4], NULL);

        if (tconfig->level == 0)
        {
            if (tex != NULL)
            {
                format = tex->gbiformat;
                depth = tex->depth;
                lutmode = tex->lutmodeindex << G_MDSFT_TEXTLUT;                
            }
            else
            {
                format = tconfig->format;
                depth = tconfig->depth;
            }

            switch (depth)
            {
                default:
                    break;
                case G_IM_SIZ_32b:
                    depth2 = G_IM_SIZ_32b;
                    lrs = sub_GAME_7F076928(width, height, 1) - 1;
                    sp138 = ceil1000(width);
                    line = (s32) (width + 3) >> 2;
                    break;
                case G_IM_SIZ_16b:
                    depth2 = G_IM_SIZ_16b;
                    lrs = sub_GAME_7F0768B8(width, height, 1) - 1;
                    sp138 = ceil2000(width);
                    line = (s32) (width + 3) >> 2;
                    break;
                case G_IM_SIZ_8b:
                    depth2 = G_IM_SIZ_16b;
                    lrs = sub_GAME_7F076848(width, height, 1) - 1;
                    sp138 = ceil4000(width);
                    line = (s32) (width + 7) >> 3;
                    break;
                case G_IM_SIZ_4b:
                    depth2 = G_IM_SIZ_16b;
                    lrs = sub_GAME_7F0767D8(width, height, 1) - 1;
                    sp138 = ceil8000(width);
                    line = (s32) (width + 0xF) >> 4;
                    break;
            }

            texSetRenderMode(&gdl, renderStyle, 1, arg3);

            if (arg3 >= 2)
            {
                gSPTextureL(gdl++, 0xffff, 0xffff, 0, arg3, G_TX_RENDERTILE, G_ON);
            }
            else
            {
                gSPTextureL(gdl++, 0xffff, 0xffff, 0, 0, G_TX_RENDERTILE, G_ON);
            }
    
            gDPSetTextureLOD(gdl++, G_TL_TILE);

            switch (format)
            {
                case G_IM_FMT_RGBA:
                    gDPSetCombineMode(gdl++, G_CC_MODULATEIA, G_CC_MODULATEIA);
                break;

                case G_IM_FMT_IA:
                    gDPSetCombineMode(gdl++, G_CC_MODULATEIA, G_CC_MODULATEIA);
                break;

                case G_IM_FMT_I:
                    gDPSetCombineMode(gdl++, G_CC_MODULATEI, G_CC_MODULATEI);
                break;

                case G_IM_FMT_CI:
                    switch (lutmode)
                    {
                        case G_TT_RGBA16:
                            gDPSetCombineMode(gdl++, G_CC_MODULATEIA, G_CC_MODULATEIA);
                        break;

                        case G_TT_IA16:
                            gDPSetCombineMode(gdl++, G_CC_MODULATEIA, G_CC_MODULATEIA);
                        break;
                    }
                break;
            }

            gDPSetTextureImage(gdl++, format, depth2, 1, tconfig->index);

            gDPSetTile(gdl++, format, depth2, 0, 0, G_TX_LOADTILE, 0,
                tconfig->flagsT, G_TX_NOMASK, G_TX_NOLOD,
                tconfig->flagsS, G_TX_NOMASK, G_TX_NOLOD);

            gDPLoadSync(gdl++);

            gDPLoadBlock(gdl++, G_TX_LOADTILE, 0, 0, lrs, sp138);
            
            gDPPipeSync(gdl++);

            if (format == G_IM_FMT_CI)
            {
                u32 a3 = lrs + 1;
				u32 t0 = (0x3ff - tex->unk0a) < a3 ? (0x3ff - tex->unk0a) : 0;

				a3 -= t0;
                
                gDPSetTile(gdl++, G_IM_FMT_RGBA, G_IM_SIZ_4b, 0, 0x0100, G_TX_LOADTILE, 0,
                    G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOLOD,
                    G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOLOD);
                gDPLoadSync(gdl++);

                gDPLoadTLUT07(gdl++, a3, t0, tex->unk0a + a3, t0);
                gDPPipeSync(gdl++);

                gDPSetTextureLUT(gdl++, lutmode);
            }
            else
            {
                gDPSetTextureLUT(gdl++, G_TT_NONE);
            }

            gDPSetTile(gdl++, format, depth, line, 0, G_TX_RENDERTILE, 0,
                tconfig->flagsT, is_less_than_certain_power_of_2(height), G_TX_NOLOD,
                tconfig->flagsS, is_less_than_certain_power_of_2(width), G_TX_NOLOD);

            gDPSetTileSize(gdl++, G_TX_RENDERTILE, ulst, ulst, (((width - 1) << 2) + ulst), (((height - 1) << 2) + ulst));
        }
        else
        {
            s32 tmem;
			s32 lod;
			u8 format;
			u8 depth;
			s32 lutmode;
			s32 depth2;
			s32 lrs;

            tmem = 0;
            lod = (s32)tconfig->level;

            if (tex != NULL)
            {
                format = tex->gbiformat;
                depth = tex->depth;
                lutmode = tex->lutmodeindex << G_MDSFT_TEXTLUT;
            }
            else
            {
                format = tconfig->format;
                depth = tconfig->depth;
            }

            if ((tex != NULL) && (tex->hasExplicitLods))
            {
                texGetDepthAndSize(tex, &depth2, &lrs);
            }
            else
            {
                switch (depth)
                {
                    case G_IM_SIZ_32b:
                        depth2 = G_IM_SIZ_32b;
                        lrs = sub_GAME_7F076928(width, height, lod) - 1;
                    break;
                    
                    case G_IM_SIZ_16b:
                        depth2 = G_IM_SIZ_16b;
                        lrs = sub_GAME_7F0768B8(width, height, lod) - 1;
                    break;
                    
                    case G_IM_SIZ_8b:
                        depth2 = G_IM_SIZ_16b;
                        lrs = sub_GAME_7F076848(width, height, lod) - 1;
                    break;
                    
                    case G_IM_SIZ_4b:
                        depth2 = G_IM_SIZ_16b;
                        lrs = sub_GAME_7F0767D8(width, height, lod) - 1;
                    break;
                }
            }

            texSetRenderMode(&gdl, renderStyle, 2, arg3);

            if (arg3 >= 2)
            {
                gSPTextureL(gdl++, 0xffff, 0xffff, lod - 1, arg3, G_TX_RENDERTILE, G_ON);
            }
            else
            {
                gSPTexture(gdl++, 0xffff, 0xffff, lod - 1, G_TX_RENDERTILE, G_ON);
            }

            gDPSetTextureLOD(gdl++, G_TL_LOD);

            // line 417
            switch (format)
            {
                case G_IM_FMT_RGBA:
                    gDPSetCombineMode(gdl++, G_CC_TRILERP, G_CC_MODULATEIA2);
                break;

                case G_IM_FMT_IA:
                    gDPSetCombineMode(gdl++, G_CC_TRILERP, G_CC_MODULATEIA2);
                break;

                case G_IM_FMT_I:
                    gDPSetCombineMode(gdl++, G_CC_TRILERP, G_CC_MODULATEI2);
                break;

                case G_IM_FMT_CI:
                    switch (lutmode)
                    {
                        case G_TT_RGBA16:
                            gDPSetCombineMode(gdl++, G_CC_MODULATEIA, G_CC_MODULATEIA);
                        break;

                        case G_TT_IA16:
                            gDPSetCombineMode(gdl++, G_CC_MODULATEIA, G_CC_MODULATEIA);
                        break;
                    }
                break;
            }

            gDPSetTextureImage(gdl++, format, depth2, 1, tconfig->index);

            gDPSetTile(gdl++, format, depth2, 0, 0, G_TX_LOADTILE, 0,
                G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOLOD,
                G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOLOD);
            
            gDPLoadSync(gdl++);

            gDPLoadBlock(gdl++, G_TX_LOADTILE, 0, 0, lrs, 0);

            gDPPipeSync(gdl++);

            if (format == 2)
            {
                u32 a2 = lrs + 1;
				u32 a3 = (0x3ff - tex->unk0a) < a2 ? (0x3ff - tex->unk0a) : 0;

				a2 -= a3;
                
                gDPSetTile(gdl++, G_IM_FMT_RGBA, G_IM_SIZ_4b, 0, 0x0100, G_TX_LOADTILE, 0,
                    G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOLOD,
                    G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOLOD);
                gDPLoadSync(gdl++);

                gDPLoadTLUT07(gdl++, a2, a3, tex->unk0a + a2, a3);
                gDPPipeSync(gdl++);

                gDPSetTextureLUT(gdl++, lutmode);
            }
            else
            {
                gDPSetTextureLUT(gdl++, G_TT_NONE);
            }
    
            for (tile = 0; tile < lod; tile++)
            {
                s32 line;

                if (tile > 0)
                {
                    if ((tex != NULL) && (tex->hasExplicitLods))
                    {
                        width = texGetWidthAtLod(tex, tile);
                        height = texGetHeightAtLod(tex, tile);
                    }
                    else
                    {
                        if (width >= 2)
                        {
                            width >>= 1;
                        }
                        
                        if (height >= 2)
                        {
                            height >>= 1;
                        }
                    }
                }

                switch (depth)
                {
                    default:
                        break;
                    case G_IM_SIZ_32b:
                        line = (s32) (width + 3) / 4;
                        break;
                    case G_IM_SIZ_16b:
                        line = (s32) (width + 3) / 4;
                        break;
                    case G_IM_SIZ_8b:
                        line = (s32) (width + 7) / 8;
                        break;
                    case G_IM_SIZ_4b:
                        line = (s32) (width + 0xF) / 16;
                        break;
                }
                
                gDPSetTile(gdl++, format, depth, line, tmem, tile, 0,
                    tconfig->flagsT, is_less_than_certain_power_of_2(height), tile,
                    tconfig->flagsS, is_less_than_certain_power_of_2(width), tile);

                gDPSetTileSize(gdl++, tile, ulst, ulst, (((width - 1) << 2) + ulst), (((height - 1) << 2) + ulst));

                tmem += line * height;
            }
        }
        
    }

    *gdlptr = gdl;
}
