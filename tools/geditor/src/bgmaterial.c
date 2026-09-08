#include "bgmaterial.h"

/* Encodings of the named combinations in include/PR/gbi.h. Keeping the host
 * tool independent of the N64 ABI avoids importing its platform typedefs. */
#define BG_COMBINE_SHADE_0       0xFCFFFFFFu /* G_CC_SHADE, G_CC_SHADE */
#define BG_COMBINE_SHADE_1       0xFFFE793Cu
#define BG_COMBINE_MODULATE_0    0xFC121824u /* G_CC_MODULATEIA, G_CC_MODULATEIA */
#define BG_COMBINE_MODULATE_1    0xFF33FFFFu
#define BG_COMBINE_MIPMAP_0      0xFC26A004u /* G_CC_TRILERP, G_CC_MODULATEIA2 */
#define BG_COMBINE_MIPMAP_1      0x1F1093FFu
#define BG_TEXTURETYPE_MIPMAP    2u

void BgMaterialInit(BgMaterial *material)
{
    material->textureword0 = material->textureword1 = 0;
    material->modeword0 = (BG_G_TEXTURE << 24) | 1u;
    material->modeword1 = 0xFFFFFFFFu;
    material->combineword0 = BG_COMBINE_MODULATE_0;
    material->combineword1 = BG_COMBINE_MODULATE_1;
}

BOOL BgMaterialReadCommand(BgMaterial *material, DWORD word0, DWORD word1)
{
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
        /* G_OFF plus SHADE in both cycles: no stale TEXEL0/TEXEL1 sampling.
         * This shade combination also participates in BG's existing fog LUT. */
        material->modeword0 &= ~0xFFu;
        material->combineword0 = BG_COMBINE_SHADE_0;
        material->combineword1 = BG_COMBINE_SHADE_1;
        return;
    }

    BOOL wasuntextured = BgMaterialTextureId(material) == BG_TEX_NONE;
    if ((material->textureword0 >> 24) != BG_G_SETTEXTURE)
    {
        material->textureword0 = (BG_G_SETTEXTURE << 24) | BG_TEXTURETYPE_MIPMAP;
        material->textureword1 = 0;
    }
    if (wasuntextured)
    {
        /* A texture can be assigned again after saving an untextured face.
         * Use a standard shaded material; undo retains the exact old one. */
        BOOL mipmap = (material->textureword0 & 7u) == BG_TEXTURETYPE_MIPMAP;
        material->combineword0 = mipmap ? BG_COMBINE_MIPMAP_0 : BG_COMBINE_MODULATE_0;
        material->combineword1 = mipmap ? BG_COMBINE_MIPMAP_1 : BG_COMBINE_MODULATE_1;
    }
    material->modeword0 = (material->modeword0 & ~0xFFu) | 1u;
    material->textureword1 = (material->textureword1 & ~(DWORD)BG_TEX_ID_MASK) | textureid;
}

BOOL BgMaterialEqual(const BgMaterial *a, const BgMaterial *b)
{
    return a->textureword0 == b->textureword0 && a->textureword1 == b->textureword1
        && a->modeword0 == b->modeword0 && a->modeword1 == b->modeword1
        && a->combineword0 == b->combineword0 && a->combineword1 == b->combineword1;
}
