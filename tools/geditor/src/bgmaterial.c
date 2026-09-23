#include "bgmaterial.h"

/* Encodings of the named combinations in include/PR/gbi.h. Keeping the host
 * tool independent of the N64 ABI avoids importing its platform typedefs. */
#define BG_COMBINE_SHADE_0       0xFCFFFFFFu /* G_CC_SHADE, G_CC_SHADE */
#define BG_COMBINE_SHADE_1       0xFFFE793Cu
#define BG_COMBINE_SHADE_PASS_1  0xFFFE7838u /* G_CC_SHADE, G_CC_PASS2 */
#define BG_COMBINE_MODULATE_0    0xFC121824u /* G_CC_MODULATEIA, G_CC_MODULATEIA */
#define BG_COMBINE_MODULATE_1    0xFF33FFFFu
#define BG_COMBINE_MIPMAP_0      0xFC26A004u /* G_CC_TRILERP, G_CC_MODULATEIA2 */
#define BG_COMBINE_MIPMAP_1      0x1F1093FFu
#define BG_TEXTURETYPE_MIPMAP    2u

void BgMaterialInit(BgMaterial *material)
{
    material->alphasource = BG_ALPHA_AUTO;
    material->fog = BG_FOG_AUTO;
    material->textureword0 = material->textureword1 = 0;
    material->modeword0 = (BG_G_TEXTURE << 24) | 1u;
    material->modeword1 = 0xFFFFFFFFu;
    material->combineword0 = BG_COMBINE_MODULATE_0;
    material->combineword1 = BG_COMBINE_MODULATE_1;
}

BOOL BgMaterialReadCommand(BgMaterial *material, DWORD word0, DWORD word1)
{
    if (BG_FOG_IS_MARKER(word0, word1))
    {
        DWORD kind = word1 & 255u;
        if (kind <= BG_FOG_OFF) { material->fog = kind; }
        return TRUE;
    }
    if (BG_ALPHA_IS_MARKER(word0, word1))
    {
        DWORD kind = BG_ALPHA_TAG_KIND(word1);
        if (BG_ALPHA_IS_PRESET(kind)) { material->alphasource = kind; }
        return TRUE;
    }
    if (BG_SURFACE_IS_MARKER(word0, word1)) { return FALSE; }
    switch (word0 >> 24)
    {
    case BG_G_SETTEXTURE:
        material->textureword0 = word0;
        material->textureword1 = word1;
        return TRUE;
    case BG_G_TEXTURE:
        material->modeword0 = word0;
        material->modeword1 = word1;
        return TRUE;
    case BG_G_SETCOMBINE:
        material->combineword0 = word0;
        material->combineword1 = word1;
        return TRUE;
    }
    return FALSE;
}

unsigned short BgMaterialTextureId(const BgMaterial *material)
{
    return (material->modeword0 & 0xFFu) != 0
        && (material->textureword0 >> 24) == BG_G_SETTEXTURE
        ? (unsigned short)(material->textureword1 & BG_TEX_ID_MASK) : BG_TEX_NONE;
}

void BgMaterialSetTexture(BgMaterial *material, DWORD textureid)
{
    if (textureid == BG_TEX_NONE)
    {
        material->alphasource = BG_ALPHA_WITHOUT_TEXTURE(material->alphasource);
        /* G_OFF plus SHADE in both cycles: no stale TEXEL0/TEXEL1 sampling.
         * This shade combination also participates in BG's existing fog LUT. */
        material->modeword0 &= ~0xFFu;
        material->combineword0 = BG_COMBINE_SHADE_0;
        material->combineword1 = BG_COMBINE_SHADE_1;
        return;
    }

    /* Native faces can inherit an enabled texture and image binding while
     * drawing only SHADE. An explicit image assignment must enable sampling
     * on those faces too, even when reapplying the already-bound image. */
    BOOL wasuntextured = BgMaterialTextureId(material) == BG_TEX_NONE
        || (material->combineword0 == BG_COMBINE_SHADE_0
            && (material->combineword1 == BG_COMBINE_SHADE_1
                || material->combineword1 == BG_COMBINE_SHADE_PASS_1));
    if ((material->textureword0 >> 24) != BG_G_SETTEXTURE)
    {
        material->textureword0 = (BG_G_SETTEXTURE << 24) | BG_TEXTURETYPE_MIPMAP;
        material->textureword1 = 0;
    }
    if (wasuntextured)
    {
        /* A texture can be assigned again after saving an untextured face.
         * Use a standard shaded material; undo retains the exact old one. */
        DWORD type = material->textureword0 & 7u;
        BOOL mipmap = type <= BG_TEXTURETYPE_MIPMAP;
        material->combineword0 = mipmap ? BG_COMBINE_MIPMAP_0 : BG_COMBINE_MODULATE_0;
        material->combineword1 = mipmap ? BG_COMBINE_MIPMAP_1 : BG_COMBINE_MODULATE_1;
        /* Types 0/1 retain their authored detail tiles; select the matching
         * combiner so the base image and its alpha come from the right tile. */
        if (type < BG_TEXTURETYPE_MIPMAP) { BgMaterialDetailCombiner(material, TRUE); }
    }
    material->modeword0 = (material->modeword0 & ~0xFFu) | 1u;
    material->textureword1 = (material->textureword1 & ~(DWORD)BG_TEX_ID_MASK) | textureid;
}

BOOL BgMaterialEqual(const BgMaterial *a, const BgMaterial *b)
{
    return a->textureword0 == b->textureword0 && a->textureword1 == b->textureword1
        && a->modeword0 == b->modeword0 && a->modeword1 == b->modeword1
        && a->combineword0 == b->combineword0 && a->combineword1 == b->combineword1
        && a->alphasource == b->alphasource && a->fog == b->fog;
}

BgTextureWrap BgMaterialGetWrap(const BgMaterial *material, BOOL t)
{
    DWORD mode = (material->textureword0 >> (t ? 20 : 22)) & 3u;
    /* texModeToGbiMode treats mode 3 as repeat as well. */
    return (material->textureword0 >> 24) != BG_G_SETTEXTURE || mode == 3
        ? BG_TEXTURE_REPEAT : (BgTextureWrap)mode;
}

void BgMaterialSetWrap(BgMaterial *material, BOOL t, BgTextureWrap wrap)
{
    unsigned int shift = t ? 20 : 22;
    /* Preserve authored aliases and every unrelated marker bit on a no-op. */
    if (BgMaterialGetWrap(material, t) != wrap)
    {
        material->textureword0 = (material->textureword0 & ~(3u << shift))
                              | ((DWORD)wrap << shift);
    }
}

void BgMaterialGetDetail(const BgMaterial *material, BgDetailTexture *detail)
{
    DWORD type = material->textureword0 & 7u;
    detail->mode = BG_DETAIL_NONE;
    detail->textureid = BG_TEX_NONE;
    detail->shiftu = detail->shiftv = detail->minlod = detail->offset = 0;
    if (BgMaterialTextureId(material) == BG_TEX_NONE) { return; }
    if (type > 1)
    {
        if (type > 4) { detail->mode = BG_DETAIL_UNKNOWN; }
        return;
    }
    /* texHandleType0 reuses the base image; only type 1 has a second ID. */
    detail->mode = type == 0 ? BG_DETAIL_BASE_IMAGE : BG_DETAIL_SEPARATE_IMAGE;
    detail->textureid = (unsigned short)((material->textureword1 >> (type == 1 ? 12 : 0)) & BG_TEX_ID_MASK);
    detail->shiftu = (material->textureword0 >> 14) & 15u;
    detail->shiftv = (material->textureword0 >> 10) & 15u;
    detail->offset = (material->textureword0 >> 18) & 3u;
    detail->minlod = material->textureword1 >> 24;
}

void BgMaterialSetDetail(BgMaterial *material, const BgDetailTexture *detail)
{
    BgDetailTexture old;
    BgMaterialGetDetail(material, &old);
    if (detail->mode == BG_DETAIL_NONE)
    {
        if (old.mode == BG_DETAIL_BASE_IMAGE || old.mode == BG_DETAIL_SEPARATE_IMAGE)
        { material->textureword0 = (material->textureword0 & ~7u) | BG_TEXTURETYPE_MIPMAP; }
        return;
    }
    material->textureword0 = (material->textureword0 & ~(7u | (0x3FFu << 10)))
        | (detail->mode == BG_DETAIL_SEPARATE_IMAGE ? 1u : 0u)
        | ((DWORD)detail->shiftu << 14) | ((DWORD)detail->shiftv << 10)
        | ((DWORD)detail->offset << 18);
    material->textureword1 = (material->textureword1 & 0x00FFFFFFu) | ((DWORD)detail->minlod << 24);
    if (detail->mode == BG_DETAIL_SEPARATE_IMAGE)
    { material->textureword1 = (material->textureword1 & ~0x00FFF000u) | ((DWORD)detail->textureid << 12); }
}

BOOL BgMaterialDetailCombiner(BgMaterial *material, BOOL enabled)
{
    static const DWORD modes[][4] = {
        {0xFC26A004u, 0x1F1093FFu, 0xFC26E404u, 0x1F10FFFFu},
        {0xFC26A004u, 0x1FFC93FCu, 0xFC26E404u, 0x1FFCFFFCu}
    };
    unsigned int i;
    /* The base image is in TEXEL1 at close range with detail enabled. Use its
       alpha, not the detail tile's alpha. These pairs also match GE's fog LUT. */
    if (enabled && material->combineword0 == BG_COMBINE_MODULATE_0
        && material->combineword1 == BG_COMBINE_MODULATE_1)
    { material->combineword0 = modes[0][0]; material->combineword1 = modes[0][1]; }
    if (enabled && material->combineword0 == 0xFC127E24u && material->combineword1 == 0xFFFFF9FCu)
    { material->combineword0 = modes[1][0]; material->combineword1 = modes[1][1]; }
    for (i = 0; i < sizeof(modes) / sizeof(modes[0]); i++)
    {
        if ((material->combineword0 == modes[i][0] && material->combineword1 == modes[i][1])
            || (material->combineword0 == modes[i][2] && material->combineword1 == modes[i][3]))
        {
            material->combineword0 = modes[i][enabled ? 2 : 0];
            material->combineword1 = modes[i][enabled ? 3 : 1];
            return TRUE;
        }
    }
    /* Some native faces load detail tiles but draw only vertex colors. They
       can be turned off without changing their intentionally plain shading. */
    return !enabled && material->combineword0 == BG_COMBINE_SHADE_0
        && material->combineword1 == BG_COMBINE_SHADE_1;
}
