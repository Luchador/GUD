#ifndef GEDITOR_LEVELSLOTS_H
#define GEDITOR_LEVELSLOTS_H
#include <stdio.h>
#include "rom.h"
#include "../../../src/levelmenuformat.h"
typedef struct LevelSlotTable { BOOL available; LevelMenuConfig value; } LevelSlotTable;
typedef struct LevelSlotOverrides { DWORD mask; int stages[LEVEL_MENU_SLOT_COUNT]; } LevelSlotOverrides;
BOOL LevelSlotsReadRom(const RomFile *rom, LevelSlotTable *table, const char **why);
int LevelSlotsGet(const LevelSlotTable *table, const LevelSlotOverrides *overrides, unsigned slot);
BOOL LevelSlotsValidate(const LevelSlotTable *table, const LevelSlotOverrides *overrides,
    const RomLevel *levels, DWORD count, const char **why);
BOOL LevelSlotsSet(const LevelSlotTable *table, LevelSlotOverrides *overrides,
    const RomLevel *levels, DWORD count, unsigned slot, int stage, const char **why);
BOOL LevelSlotsReadOverride(LevelSlotOverrides *overrides, const char *text);
BOOL LevelSlotsWriteOverrides(FILE *file, const LevelSlotOverrides *overrides);
BOOL LevelSlotsApplyRom(RomFile *rom, const LevelSlotOverrides *overrides, const char **why);
#endif
