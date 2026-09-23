#ifndef GEDITOR_BGMATERIAL_H
#define GEDITOR_BGMATERIAL_H

#include "../../../src/bgtransparency.h"

#include "bgload.h"

/* Fast3D material commands stored in a raw GoldenEye room stream. The C0
 * marker is expanded by texLoadFromGdl; it is never an RDP texture address. */
#define BG_G_TEXTURE     0xBBu
#define BG_G_SETTEXTURE  0xC0u
#define BG_G_SETCOMBINE  0xFCu
#define BG_G_PIPESYNC    0xE7u

typedef struct BgMaterial {
    DWORD textureword0, textureword1; /* complete GoldenEye texture marker */
    DWORD modeword0, modeword1;       /* gSPTexture: scale, tile, LOD, enable */
    DWORD combineword0, combineword1;
    DWORD alphasource; /* BG_ALPHA_* preset; authored mux stays intact */
    DWORD fog; /* BG_FOG_* participation; authored pipeline stays intact */
} BgMaterial;

/* GoldenEye C0 texture-marker modes, before conversion to RDP tile flags. */
typedef enum BgTextureWrap {
    BG_TEXTURE_REPEAT = 0,
    BG_TEXTURE_CLAMP = 1,
    BG_TEXTURE_MIRROR = 2
} BgTextureWrap;

typedef enum BgDetailMode {
    BG_DETAIL_NONE,
    BG_DETAIL_BASE_IMAGE, /* type 0: an extra LOD tile using the base image */
    BG_DETAIL_SEPARATE_IMAGE, /* type 1: a separate image in tile 0 */
    BG_DETAIL_UNKNOWN
} BgDetailMode;

/* Authored C0 parameters, before texLoadFromGdl expands them into tiles. */
typedef struct BgDetailTexture {
    BgDetailMode mode;
    unsigned short textureid;
    unsigned char shiftu, shiftv, minlod, offset;
} BgDetailTexture;

void BgMaterialInit(BgMaterial *material);
BOOL BgMaterialReadCommand(BgMaterial *material, DWORD word0, DWORD word1);
unsigned short BgMaterialTextureId(const BgMaterial *material);
void BgMaterialSetTexture(BgMaterial *material, DWORD textureid);
BOOL BgMaterialEqual(const BgMaterial *a, const BgMaterial *b);
BgTextureWrap BgMaterialGetWrap(const BgMaterial *material, BOOL t);
void BgMaterialSetWrap(BgMaterial *material, BOOL t, BgTextureWrap wrap);
void BgMaterialGetDetail(const BgMaterial *material, BgDetailTexture *detail);
/* Changes only the native detail fields, retaining wraps, UV scale and other bits. */
void BgMaterialSetDetail(BgMaterial *material, const BgDetailTexture *detail);
/* Switch standard shaded/mipmapped combiners without changing their alpha source. */
BOOL BgMaterialDetailCombiner(BgMaterial *material, BOOL enabled);

#endif
