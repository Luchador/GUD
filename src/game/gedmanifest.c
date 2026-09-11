#include <ultra64.h>
#include "bg.h"
#include "environment.h"
#include "image.h"
#include "lv.h"
#include "monitorconstants.h"

extern u8 _imagesSegmentRomStart[], _imagesSegmentRomEnd[];
extern u8 _obsegSegmentRomStart[], _obsegSegmentRomEnd[];
extern u8 _musicfilesSegmentRomStart[], _musicfilesSegmentRomEnd[];
extern u8 _gedLevelTableRom[];
extern u8 _gedFileTableRom[];
extern u8 _gedEnvTableRom[];
extern u8 _gedTextureTableRom[], _gedTextureConfigRom[];

extern u8 _gedMonitorScriptsRom[], _gedMonitorImagesRom[];
extern u8 _gedMonitorInitialRom[];

#define GEDM_KIND(a, b, c, d) \
    (((u32)(a) << 24) | ((u32)(b) << 16) | ((u32)(c) << 8) | (u32)(d))

#define GEDM_F_1172 0x00000001

typedef struct GedManifestEntry {
    u32 kind;
    u32 romstart;
    u32 romend;
    u32 flags;
} GedManifestEntry;

typedef struct GedManifest {
    u8  magic[16];  /* "GUDGEDITORMANIF" + NUL */
    u32 version;
    u32 entrycount;
    GedManifestEntry entries[12];
} GedManifest;

const GedManifest g_GedManifest = {
    "GUDGEDITORMANIF",
    2, /* LevelEntry now includes levelName; older editors must not read it as v1. */
    12,
    {
        { GEDM_KIND('I','M','G','S'), (u32)_imagesSegmentRomStart,     (u32)_imagesSegmentRomEnd,                                       0                          },
        { GEDM_KIND('O','B','S','G'), (u32)_obsegSegmentRomStart,      (u32)_obsegSegmentRomEnd,                                        0                          },
        { GEDM_KIND('M','U','S','F'), (u32)_musicfilesSegmentRomStart, (u32)_musicfilesSegmentRomEnd,                                   GEDM_F_1172                },
        { GEDM_KIND('S','T','G','T'), (u32)_gedLevelTableRom,          (u32)_gedLevelTableRom + STAGES_MAX * sizeof(struct LevelEntry), STAGES_MAX                 },
        { GEDM_KIND('C','M','A','P'), (u32)&_csegmentSegmentRomStart,  (u32)&_csegmentSegmentRomEnd,                                    (u32)&_csegmentSegmentStart},
        { GEDM_KIND('F','T','B','L'), (u32)_gedFileTableRom,           0,                                                               0                          },
        { GEDM_KIND('E','N','V','T'), (u32)_gedEnvTableRom,            0,                                                               sizeof(EnvironmentRecord)  },
        /* TXTB includes a sentinel slot. flags is the usable image capacity. */
        { GEDM_KIND('T','X','T','B'), (u32)_gedTextureTableRom, (u32)_gedTextureTableRom + (MAX_TEXTURES + 1) * sizeof(struct image_entry), MAX_TEXTURES },
        { GEDM_KIND('T','X','C','F'), (u32)_gedTextureConfigRom, (u32)_gedTextureConfigRom + sizeof(TextureRomConfig), 1 },
        /* Native command pointers use CMAP. Image descriptors live in segment 2.
           flags gives the script count or the encoded record size. */
        { GEDM_KIND('M','O','N','A'), (u32)_gedMonitorScriptsRom, (u32)_gedMonitorScriptsRom + MONITOR_ANIMATION_COUNT * 4, MONITOR_ANIMATION_COUNT },
        { GEDM_KIND('M','O','N','T'), (u32)_gedMonitorImagesRom, (u32)_gedMonitorImagesRom + MONITOR_IMAGE_COUNT * 12, 12 },
        { GEDM_KIND('M','O','N','D'), (u32)_gedMonitorInitialRom, (u32)_gedMonitorInitialRom + 116, 116 },
    },
};
