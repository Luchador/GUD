#include <ultra64.h>
#include "bg.h"

extern u8 _imagesSegmentRomStart[], _imagesSegmentRomEnd[];
extern u8 _obsegSegmentRomStart[], _obsegSegmentRomEnd[];
extern u8 _musicfilesSegmentRomStart[], _musicfilesSegmentRomEnd[];
extern u8 _gedLevelTableRom[];
extern u8 _csegmentSegmentRomStart[], _csegmentSegmentRomEnd[], _csegmentSegmentStart[];

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
    GedManifestEntry entries[5];
} GedManifest;

const GedManifest g_GedManifest = {
    "GUDGEDITORMANIF",
    1,
    5,
    {
        { GEDM_KIND('I','M','G','S'), (u32)_imagesSegmentRomStart,     (u32)_imagesSegmentRomEnd,                      0                          },
        { GEDM_KIND('O','B','S','G'), (u32)_obsegSegmentRomStart,      (u32)_obsegSegmentRomEnd,                       0                          },
        { GEDM_KIND('M','U','S','F'), (u32)_musicfilesSegmentRomStart, (u32)_musicfilesSegmentRomEnd,                  GEDM_F_1172                },
        { GEDM_KIND('S','T','G','T'), (u32)_gedLevelTableRom,          (u32)_gedLevelTableRom + STAGES_MAX * 32,       STAGES_MAX                 },
        { GEDM_KIND('C','M','A','P'), (u32)_csegmentSegmentRomStart,   (u32)_csegmentSegmentRomEnd,                    (u32)_csegmentSegmentStart },
    },
};
