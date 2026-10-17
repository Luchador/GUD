#ifndef GUD_LEVELMENUASSETS_H
#define GUD_LEVELMENUASSETS_H
#include "levelmenuformat.h"
#define LEVEL_ASSETS_KIND 0x4c564153u /* LVAS, independent of the v1 LVSL table */
#define LEVEL_ASSETS_VERSION 1u
#define LEVEL_ASSET_NAME_SIZE 32
#define LEVEL_ASSET_FIELDS 7
#define LEVEL_ASSET_MENU_IMAGE 0
#define LEVEL_ASSET_BRIEF_TOP 1
#define LEVEL_ASSET_BRIEF_BOTTOM 2
#define LEVEL_ASSET_MENU_NAME 3
#define LEVEL_ASSET_MISSION_NAME 4
#define LEVEL_ASSET_PART_NAME 5
#define LEVEL_ASSET_BRIEF_SOURCE 6
#define LEVEL_ASSET_BIT(field) (1u << (field))
#define LEVEL_ASSET_ALL 127u
#define LEVEL_ASSET_MP_FIELDS (LEVEL_ASSET_BIT(LEVEL_ASSET_MENU_IMAGE) | LEVEL_ASSET_BIT(LEVEL_ASSET_MENU_NAME))
typedef struct LevelMenuAsset {
    unsigned int flags;
    int images[3]; /* Native image IDs; briefing photograph is two 128x64 halves. */
    int briefingStage; /* -1: empty briefing, otherwise a known solo briefing source. */
    char names[3][LEVEL_ASSET_NAME_SIZE]; /* Menu, mission and part; ASCII, no newline. */
} LevelMenuAsset;
typedef struct LevelMenuAssetConfig {
    unsigned int version;
    LevelMenuAsset slots[LEVEL_MENU_SLOT_COUNT];
} LevelMenuAssetConfig;
#define LEVEL_ASSET_RECORD_SIZE 116u
#define LEVEL_ASSETS_ROM_SIZE (4u + LEVEL_MENU_SLOT_COUNT * LEVEL_ASSET_RECORD_SIZE)
typedef char LevelMenuAssetSizeCheck[sizeof(LevelMenuAsset) == LEVEL_ASSET_RECORD_SIZE ? 1 : -1];
typedef char LevelMenuAssetsSizeCheck[sizeof(LevelMenuAssetConfig) == LEVEL_ASSETS_ROM_SIZE ? 1 : -1];
/* Each upload must fit RDP texture memory; CI uses only half for pixels. */
#define LEVEL_ASSET_IMAGE_COMPATIBLE(f,w,h,field) \
    ((field) == LEVEL_ASSET_MENU_IMAGE \
        ? (w) == 68 && (h) == 44 && ((f) == 5 || (f) == 6 || (f) == 7 || (f) == 8 || (f) == 10 || (f) == 12) \
        : (w) == 128 && (h) == 64 && ((f) == 6 || (f) == 8))
#endif
