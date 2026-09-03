#include <ultra64.h>
#include <ramrom.h>
#include <memp.h>
#include "image_bank.h"


u8* img_curpos;
u32 img_curdatatable;
s32 img_bitcount;
s32 globalbank_rdram_offset;
s32 *pGlobalimagetable;
struct sImageTableEntry *genericimage;
struct sImageTableEntry *impactimages;
struct sImageTableEntry *explosion_smokeimages;
struct sImageTableEntry *scattered_explosions;
struct sImageTableEntry *flareimage1;
struct sImageTableEntry *flareimage2;
struct sImageTableEntry *flareimage3;
struct sImageTableEntry *flareimage4;
struct sImageTableEntry *flareimage5;
struct sImageTableEntry *ammo9mmimage;
struct sImageTableEntry *rifleammoimage;
struct sImageTableEntry *shotgunammoimage;
struct sImageTableEntry *knifeammoimage;
struct sImageTableEntry *glaunchammoimage;
struct sImageTableEntry *rocketammoimage;
struct sImageTableEntry *genericmineammoimage;
struct sImageTableEntry *grenadeammoimage;
struct sImageTableEntry *magnumammoimage;
struct sImageTableEntry *goldengunammoimage;
struct sImageTableEntry *remotemineammoimage;
struct sImageTableEntry *timedmineammoimage;
struct sImageTableEntry *proxmineammoimage;
struct sImageTableEntry *tankammoimage;
struct sImageTableEntry *crosshairimage;
struct sImageTableEntry *betacrosshairimage;
struct sImageTableEntry *glassoverlayimage;
struct sImageTableEntry *monitorimages;
struct sImageTableEntry *skywaterimages;
struct sImageTableEntry *mainfolderimages;
struct sImageTableEntry *mpradarimages;
struct sImageTableEntry *mpcharselimages;
struct sImageTableEntry *mpstageselimages;
extern u8* _GlobalimagetableSegmentRomStart;
extern u32* _GlobalimagetableSegmentStart;
extern u32* _GlobalimagetableSegmentEnd;
extern void* s_genericimage;
extern void* s_impactimages;
extern void* s_explosion_smokeimages;
extern void* s_scattered_explosions;
extern void* s_flareimage1;
extern void* s_flareimage2;
extern void* s_flareimage3;
extern void* s_flareimage4;
extern void* s_flareimage5;
extern void* s_ammo9mmimage;
extern void* s_rifleammoimage;
extern void* s_shotgunammoimage;
extern void* s_knifeammoimage;
extern void* s_glammoimage;
extern void* s_rocketammoimage;
extern void* s_genericmineammoimage;
extern void* s_grenadeammoimage;
extern void* s_magnumammoimage;
extern void* s_goldengunammoimage;
extern void* s_remotemineammoimage;
extern void* s_timedmineammoimage;
extern void* s_proxmineammoimage;
extern void* s_tankammoimage;
extern void* s_crosshairimage;
extern void* s_betacrosshairimage;
extern void* s_glassoverlayimage;
extern void* s_monitorimages;
extern void* s_skywaterimages;
extern void* s_mainfolderimages;
extern void* s_mpradarimages;
extern void* s_mpcharselimages;
extern void* s_mpstageselimages;

extern Gfx* globalDL_0x000;
extern Gfx* globalDL_0x078;
extern Gfx* globalDL_0x120;
extern Gfx* globalDL_0x1c8;
extern Gfx* globalDL_0x270;
extern Gfx* globalDL_0x318;
extern Gfx* globalDL_0x3c0;
extern Gfx* globalDL_0x468;
extern Gfx* globalDL_0x510;
extern Gfx* globalDL_0x5b8;
extern Gfx* globalDL_0x660;
extern Gfx* globalDL_0x708;
extern Gfx* globalDL_0x7b0;
extern Gfx* globalDL_0x858;
extern Gfx* globalDL_0x900;
extern Gfx* globalDL_0x9a8;
extern Gfx* globalDL_0xa50;


void texReset(void)
{
    u32 size;
    s32 i;

    size = (u32)&_GlobalimagetableSegmentEnd - (u32)&_GlobalimagetableSegmentStart;
    pGlobalimagetable = mempAllocBytesInBank(size + 0x1000, MEMPOOL_STAGE);
    pGlobalimagetable = ((u32)pGlobalimagetable + 0xFFFU) & 0xFFFFF000;

    romCopy(pGlobalimagetable, &_GlobalimagetableSegmentRomStart, size);

    globalbank_rdram_offset = (u32)pGlobalimagetable + 0xFE000000;
    genericimage = (void *) (globalbank_rdram_offset + (u32)&s_genericimage);
    impactimages = (void *) (globalbank_rdram_offset + (u32)&s_impactimages);
    explosion_smokeimages = (void *) (globalbank_rdram_offset + (u32)&s_explosion_smokeimages);
    scattered_explosions = (void *) (globalbank_rdram_offset + (u32)&s_scattered_explosions);
    flareimage1 = (void *) (globalbank_rdram_offset + (u32)&s_flareimage1);
    flareimage2 = (void *) (globalbank_rdram_offset + (u32)&s_flareimage2);
    flareimage3 = (void *) (globalbank_rdram_offset + (u32)&s_flareimage3);
    flareimage4 = (void *) (globalbank_rdram_offset + (u32)&s_flareimage4);
    flareimage5 = (void *) (globalbank_rdram_offset + (u32)&s_flareimage5);
    ammo9mmimage = (void *) (globalbank_rdram_offset + (u32)&s_ammo9mmimage);
    rifleammoimage = (void *) (globalbank_rdram_offset + (u32)&s_rifleammoimage);
    shotgunammoimage = (void *) (globalbank_rdram_offset + (u32)&s_shotgunammoimage);
    knifeammoimage = (void *) (globalbank_rdram_offset + (u32)&s_knifeammoimage);
    glaunchammoimage = (void *) (globalbank_rdram_offset + (u32)&s_glammoimage);
    rocketammoimage = (void *) (globalbank_rdram_offset + (u32)&s_rocketammoimage);
    genericmineammoimage = (void *) (globalbank_rdram_offset + (u32)&s_genericmineammoimage);
    grenadeammoimage = (void *) (globalbank_rdram_offset + (u32)&s_grenadeammoimage);
    magnumammoimage = (void *) (globalbank_rdram_offset + (u32)&s_magnumammoimage);
    goldengunammoimage = (void *) (globalbank_rdram_offset + (u32)&s_goldengunammoimage);
    remotemineammoimage = (void *) (globalbank_rdram_offset + (u32)&s_remotemineammoimage);
    timedmineammoimage = (void *) (globalbank_rdram_offset + (u32)&s_timedmineammoimage);
    proxmineammoimage = (void *) (globalbank_rdram_offset + (u32)&s_proxmineammoimage);
    tankammoimage = (void *) (globalbank_rdram_offset + (u32)&s_tankammoimage);
    crosshairimage = (void *) (globalbank_rdram_offset + (u32)&s_crosshairimage);
    betacrosshairimage = (void *) (globalbank_rdram_offset + (u32)&s_betacrosshairimage);
    glassoverlayimage = (void *) (globalbank_rdram_offset + (u32)&s_glassoverlayimage);
    monitorimages = (void *) (globalbank_rdram_offset + (u32)&s_monitorimages);
    skywaterimages = (void *) (globalbank_rdram_offset + (u32)&s_skywaterimages);
    mainfolderimages = (void *) (globalbank_rdram_offset + (u32)&s_mainfolderimages);
    mpradarimages = (void *) (globalbank_rdram_offset + (u32)&s_mpradarimages);
    mpcharselimages = (void *) (globalbank_rdram_offset + (u32)&s_mpcharselimages);
    mpstageselimages = (void *) (globalbank_rdram_offset + (u32)&s_mpstageselimages);

    texLoadFromDisplayList(globalbank_rdram_offset + (u32)&globalDL_0x000, 0);
    texLoadFromDisplayList(globalbank_rdram_offset + (u32)&globalDL_0x078, 0);
    texLoadFromDisplayList(globalbank_rdram_offset + (u32)&globalDL_0x120, 0);
    texLoadFromDisplayList(globalbank_rdram_offset + (u32)&globalDL_0x1c8, 0);
    texLoadFromDisplayList(globalbank_rdram_offset + (u32)&globalDL_0x270, 0);
    texLoadFromDisplayList(globalbank_rdram_offset + (u32)&globalDL_0x318, 0);
    texLoadFromDisplayList(globalbank_rdram_offset + (u32)&globalDL_0x3c0, 0);
    texLoadFromDisplayList(globalbank_rdram_offset + (u32)&globalDL_0x468, 0);
    texLoadFromDisplayList(globalbank_rdram_offset + (u32)&globalDL_0x510, 0);
    texLoadFromDisplayList(globalbank_rdram_offset + (u32)&globalDL_0x5b8, 0);
    texLoadFromDisplayList(globalbank_rdram_offset + (u32)&globalDL_0x660, 0);
    texLoadFromDisplayList(globalbank_rdram_offset + (u32)&globalDL_0x708, 0);
    texLoadFromDisplayList(globalbank_rdram_offset + (u32)&globalDL_0x7b0, 0);
    texLoadFromDisplayList(globalbank_rdram_offset + (u32)&globalDL_0x858, 0);
    texLoadFromDisplayList(globalbank_rdram_offset + (u32)&globalDL_0x900, 0);
    texLoadFromDisplayList(globalbank_rdram_offset + (u32)&globalDL_0x9a8, 0);
    texLoadFromDisplayList(globalbank_rdram_offset + (u32)&globalDL_0xa50, 0);

    texLoad(genericimage, 0);

    for (i=0; i < 6; i++)
    {
        texLoad(&explosion_smokeimages[i], 0);
    }

    for (i=0; i < 5; i++)
    {
        texLoad(&scattered_explosions[i], 0);
    }
}
