#include <ultra64.h>
#include "bg.h"
#include "environment.h"
#include "image.h"
#include "lv.h"
#include "monitorconstants.h"
#include "gedmanifest.h"

extern u8 _imagesSegmentRomStart[], _imagesSegmentRomEnd[];
extern u8 _obsegSegmentRomStart[], _obsegSegmentRomEnd[];
extern u8 _musicfilesSegmentRomStart[], _musicfilesSegmentRomEnd[];
extern u8 _gedLevelTableRom[];
extern u8 _gedFileTableRom[];
extern u8 _gedEnvTableRom[];
extern u8 _gedTextureTableRom[], _gedTextureConfigRom[];

extern u8 _gedMonitorScriptsRom[], _gedMonitorImagesRom[];
extern u8 _gedMonitorInitialRom[];
extern u8 _sfxctlSegmentRomStart[], _sfxctlSegmentRomEnd[];
extern u8 _sfxtblSegmentRomStart[], _sfxtblSegmentRomEnd[];
extern u8 _instrumentsctlSegmentRomStart[], _instrumentsctlSegmentRomEnd[];
extern u8 _instrumentstblSegmentRomStart[], _instrumentstblSegmentRomEnd[];
extern u8 _musicsampletblSegmentRomStart[], _musicsampletblSegmentRomEnd[];
extern u8 _gedMusicDataRomStart[], _gedMusicDataRomEnd[];
extern u8 _animation_dataSegmentRomEnd[];
extern u8 _animation_entriesSegmentRomStart[], _animation_entriesSegmentRomEnd[];
extern u8 _gedMusicVolumesRom[], _gedCharacterAnimationsRom[], _gedObjectAnimationsRom[];
extern u8 _gedCharacterModelsRom[], _gedPropModelsRom[], _gedItemModelsRom[];
extern u8 _gedGlobalAIListsRom[], _gedTextBanksRom[];

#define GEDM_KIND(a, b, c, d) \
    (((u32)(a) << 24) | ((u32)(b) << 16) | ((u32)(c) << 8) | (u32)(d))

#define GEDM_ENTRY_COUNT 28

/* A bounded, versioned descriptor. The native table's size comes from its
 * defining translation unit, avoiding duplicate hard-coded catalog counts. */
#define GEDM_CATALOG(a, b, c, d, rom) \
    { GEDM_KIND(a, b, c, d), (u32)(rom), (u32)(rom) + sizeof(GedManifestTable), GEDM_F_TABLE }

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
    GedManifestEntry entries[GEDM_ENTRY_COUNT];
} GedManifest;

const GedManifest g_GedManifest = {
    "GUDGEDITORMANIF",
    2, /* LevelEntry now includes levelName; older editors must not read it as v1. */
    GEDM_ENTRY_COUNT,
    {
        { GEDM_KIND('I','M','G','S'), (u32)_imagesSegmentRomStart,     (u32)_imagesSegmentRomEnd,                                       0                          },
        { GEDM_KIND('O','B','S','G'), (u32)_obsegSegmentRomStart,      (u32)_obsegSegmentRomEnd,                                        0                          },
        { GEDM_KIND('M','U','S','F'), (u32)_musicfilesSegmentRomStart, (u32)_musicfilesSegmentRomEnd,                                   0                          },
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
        /* Native audio banks: ctl offsets are local to their own ctl bank,
           sample offsets are local to the corresponding tbl bank. No 1172 wrapper. */
        { GEDM_KIND('S','F','X','C'), (u32)_sfxctlSegmentRomStart, (u32)_sfxctlSegmentRomEnd, 0 },
        { GEDM_KIND('S','F','X','T'), (u32)_sfxtblSegmentRomStart, (u32)_sfxtblSegmentRomEnd, 0 },
        { GEDM_KIND('I','N','S','C'), (u32)_instrumentsctlSegmentRomStart, (u32)_instrumentsctlSegmentRomEnd, 0 },
        { GEDM_KIND('I','N','S','T'), (u32)_instrumentstblSegmentRomStart, (u32)_instrumentstblSegmentRomEnd, 0 },
        /* MUST starts with a four-byte header, then eight-byte sequence rows.
           Track addresses are relative to MUST's start, not MUSD's start. */
        { GEDM_KIND('M','U','S','T'), (u32)_musicsampletblSegmentRomStart, (u32)_musicsampletblSegmentRomEnd, 8 },
        { GEDM_KIND('M','U','S','D'), (u32)_gedMusicDataRomStart, (u32)_gedMusicDataRomEnd, 0 },
        GEDM_CATALOG('M','U','S','V', _gedMusicVolumesRom),
        /* ANID contains headers/root motion; ANIF contains expanded frame rotations. */
        { GEDM_KIND('A','N','I','D'), (u32)&_animation_dataSegmentRomStart, (u32)_animation_dataSegmentRomEnd, 0 },
        { GEDM_KIND('A','N','I','F'), (u32)_animation_entriesSegmentRomStart, (u32)_animation_entriesSegmentRomEnd, 0 },
        GEDM_CATALOG('A','N','I','C', _gedCharacterAnimationsRom),
        GEDM_CATALOG('A','N','I','O', _gedObjectAnimationsRom),
        GEDM_CATALOG('C','H','R','M', _gedCharacterModelsRom),
        GEDM_CATALOG('P','R','O','P', _gedPropModelsRom),
        GEDM_CATALOG('I','T','E','M', _gedItemModelsRom),
        GEDM_CATALOG('A','I','G','L', _gedGlobalAIListsRom),
        GEDM_CATALOG('T','X','B','K', _gedTextBanksRom),
    },
};
