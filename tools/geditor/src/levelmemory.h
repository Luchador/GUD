#ifndef GEDITOR_LEVELMEMORY_H
#define GEDITOR_LEVELMEMORY_H

#include <stdio.h>
#include "rom.h"

#define LEVEL_MEMORY_FIELDS 4
#define LEVEL_MEMORY_SLOT_SIZE 64u
typedef struct LevelMemory {
    DWORD id;
    DWORD kib[LEVEL_MEMORY_FIELDS];
} LevelMemory;
typedef struct LevelMemoryTable {
    DWORD count;
    LevelMemory rows[ROM_MAX_LEVELS];
    DWORD offsets[ROM_MAX_LEVELS]; /* ROM offsets, refreshed with the base */
} LevelMemoryTable;
typedef struct LevelMemoryOverride { LevelMemory value; DWORD fields; } LevelMemoryOverride;
typedef struct LevelMemoryOverrides {
    DWORD count;
    LevelMemoryOverride rows[ROM_MAX_LEVELS];
} LevelMemoryOverrides;

extern const char *const g_LevelMemoryKeys[LEVEL_MEMORY_FIELDS];
extern const char *const g_LevelMemoryLabels[LEVEL_MEMORY_FIELDS];
const LevelMemory *LevelMemoryFind(const LevelMemoryTable *table, DWORD id);
BOOL LevelMemoryReadRom(const RomFile *rom, LevelMemoryTable *table, const char **why);
BOOL LevelMemoryParseField(LevelMemory *value, int field, const char *text, const char **why);
BOOL LevelMemoryGet(const LevelMemoryTable *table, const LevelMemoryOverrides *overrides, DWORD id, LevelMemory *value);
BOOL LevelMemorySet(const LevelMemoryTable *table, LevelMemoryOverrides *overrides, const LevelMemory *value, const char **why);
BOOL LevelMemoryValidateOverrides(const LevelMemoryTable *table, const LevelMemoryOverrides *overrides, const char **why);
BOOL LevelMemoryReadOverride(LevelMemoryOverrides *overrides, const char *text);
BOOL LevelMemoryWriteOverrides(FILE *file, const LevelMemoryOverrides *overrides);
BOOL LevelMemoryApplyRom(RomFile *rom, const LevelMemoryOverrides *overrides, const char **why);
BOOL LevelMemoryRebase(const LevelMemoryTable *oldbase, const LevelMemoryTable *newbase, LevelMemoryOverrides *overrides, const char **why);

#endif
