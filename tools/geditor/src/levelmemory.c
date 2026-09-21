#include <ctype.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "levelmemory.h"

const char *const g_LevelMemoryKeys[LEVEL_MEMORY_FIELDS] = {"mgfx", "mvtx", "mt", "ma"};
const char *const g_LevelMemoryLabels[LEVEL_MEMORY_FIELDS] = {
    "Display lists", "Vertices and matrices", "Textures", "Dynamic heap"
};
static char g_LevelMemoryError[256];
static BOOL Fail(const char **why, const char *message)
{ if (why) { *why = message; } return FALSE; }
static DWORD Read32(const unsigned char *p)
{ return (DWORD)p[0] << 24 | (DWORD)p[1] << 16 | (DWORD)p[2] << 8 | p[3]; }
static BOOL ValidValue(DWORD value) { return value >= 1 && value <= 8192; }
const LevelMemory *LevelMemoryFind(const LevelMemoryTable *table, DWORD id)
{
    if (table && table->count <= ROM_MAX_LEVELS)
        for (DWORD i = 0; i < table->count; i++)
            if (table->rows[i].id == id) { return &table->rows[i]; }
    return NULL;
}
BOOL LevelMemoryParseField(LevelMemory *value, int field, const char *text, const char **why)
{
    char *end;
    if (!value || field < 0 || field >= LEVEL_MEMORY_FIELDS || !text) { return Fail(why, "Invalid memory field."); }
    while (isspace((unsigned char)*text)) { text++; }
    errno = 0;
    unsigned long n = strtoul(text, &end, 10);
    while (isspace((unsigned char)*end)) { end++; }
    if (!isdigit((unsigned char)*text) || *end || errno || n > 8192 || !ValidValue((DWORD)n))
    {
        snprintf(g_LevelMemoryError, sizeof(g_LevelMemoryError), "%s must be a whole number from 1 to 8192 KiB.", g_LevelMemoryLabels[field]);
        return Fail(why, g_LevelMemoryError);
    }
    value->kib[field] = (DWORD)n; return TRUE;
}
/* Accept only the known token contract. Never silently discard future tokens
 * or change the meaning of an old ROM's allocation string during export. */
static BOOL ReadTokens(const char *text, LevelMemory *value)
{
    const char *const keys[] = {"mgfx", "mvtx", "mt", "ma", "ml", "me"};
    unsigned seen = 0;
    while (*text)
    {
        while (*text == ' ') { text++; }
        if (!*text) { break; }
        if (*text++ != '-') { return FALSE; }
        unsigned field;
        for (field = 0; field < 6; field++)
            if (!strncmp(text, keys[field], strlen(keys[field])) && isdigit((unsigned char)text[strlen(keys[field])])) { break; }
        if (field == 6 || (seen & (1u << field))) { return FALSE; }
        text += strlen(keys[field]);
        errno = 0; char *end;
        unsigned long n = strtoul(text, &end, 0); /* same base as runtime tokens */
        if (end == text || errno || (*end && *end != ' ') || n > 8192
            || (field < 4 ? !ValidValue((DWORD)n) : n != 0)) { return FALSE; }
        if (field < 4) { value->kib[field] = (DWORD)n; }
        seen |= 1u << field; text = end;
    }
    return seen == 63;
}
BOOL LevelMemoryReadRom(const RomFile *rom, LevelMemoryTable *table, const char **why)
{
    const RomManifestEntry *slots = NULL, *stages = NULL, *cmap = NULL;
    LevelMemoryTable result = {0};
    DWORD used[ROM_MAX_LEVELS], usedcount = 0;
    memset(table, 0, sizeof(*table));
    if (!rom || !rom->data || rom->info.entrycount > ROM_MAX_ENTRIES || rom->info.levelcount > ROM_MAX_LEVELS)
    { return Fail(why, "No base ROM memory settings are available."); }
    for (DWORD i = 0; i < rom->info.entrycount; i++)
    {
        const RomManifestEntry *e = &rom->info.entries[i];
        if (e->kind == 0x4c4d454du) { if (slots) { goto invalid; } slots = e; }
        if (e->kind == 0x53544754u) { if (stages) { goto invalid; } stages = e; }
        if (e->kind == 0x434d4150u) { if (cmap) { goto invalid; } cmap = e; }
    }
    if (!slots) { return Fail(why, "Rebase onto a rebuilt GUD ROM to edit level memory allocations."); }
    if (!RomLevelTableIsValid(stages, rom->size) || (stages->romend - stages->romstart) / stages->flags != ROM_LEVEL_ROW_SIZE
        || !cmap || cmap->romstart >= cmap->romend || cmap->romend > rom->size
        || stages->romstart < cmap->romstart || stages->romend > cmap->romend
        || slots->flags != LEVEL_MEMORY_SLOT_SIZE || slots->romstart >= slots->romend
        || slots->romstart < cmap->romstart || slots->romend > cmap->romend
        || (slots->romend - slots->romstart) % LEVEL_MEMORY_SLOT_SIZE
        || (slots->romend - slots->romstart) / LEVEL_MEMORY_SLOT_SIZE > ROM_MAX_LEVELS
        || (slots->romstart < stages->romend && stages->romstart < slots->romend)) { goto invalid; }
    for (DWORD i = 0; i < stages->flags; i++)
    {
        const unsigned char *row = rom->data + stages->romstart + i * ROM_LEVEL_ROW_SIZE;
        DWORD id = Read32(row), pointer = Read32(row + 20);
        LONGLONG off = (LONGLONG)pointer - cmap->flags + cmap->romstart;
        BOOL selectable = FALSE;
        for (DWORD j = 0; j < rom->info.levelcount; j++)
            if ((DWORD)rom->info.levels[j].levelID == id) { selectable = TRUE; break; }
        if (!pointer && !selectable) { continue; }
        if (off < slots->romstart || off > (LONGLONG)slots->romend - LEVEL_MEMORY_SLOT_SIZE
            || (off - slots->romstart) % LEVEL_MEMORY_SLOT_SIZE) { goto invalid; }
        for (DWORD j = 0; j < usedcount; j++) { if (used[j] == (DWORD)off) { goto invalid; } }
        used[usedcount++] = (DWORD)off;
        if (!selectable) { continue; }
        const char *text = (const char *)rom->data + (DWORD)off;
        if (!memchr(text, 0, LEVEL_MEMORY_SLOT_SIZE) || LevelMemoryFind(&result, id)) { goto invalid; }
        LevelMemory value = {0}; value.id = id;
        if (!ReadTokens(text, &value)) { goto invalid; }
        result.offsets[result.count] = (DWORD)off;
        result.rows[result.count++] = value;
    }
    if (result.count != rom->info.levelcount) { goto invalid; }
    *table = result; return TRUE;
invalid:
    return Fail(why, "The ROM's level memory slots or allocation tokens are invalid or unsupported.");
}
BOOL LevelMemoryGet(const LevelMemoryTable *table, const LevelMemoryOverrides *overrides, DWORD id, LevelMemory *value)
{
    const LevelMemory *base = LevelMemoryFind(table, id);
    if (!base || !overrides || overrides->count > ROM_MAX_LEVELS) { return FALSE; }
    *value = *base;
    for (DWORD i = 0; i < overrides->count; i++)
        if (overrides->rows[i].value.id == id)
        {
            for (int f = 0; f < LEVEL_MEMORY_FIELDS; f++)
                if (overrides->rows[i].fields & (1u << f)) { value->kib[f] = overrides->rows[i].value.kib[f]; }
            break;
        }
    return TRUE;
}
BOOL LevelMemoryValidateOverrides(const LevelMemoryTable *table, const LevelMemoryOverrides *overrides, const char **why)
{
    if (!overrides || overrides->count > ROM_MAX_LEVELS) { return Fail(why, "Invalid memory overrides."); }
    for (DWORD i = 0; i < overrides->count; i++)
    {
        const LevelMemoryOverride *o = &overrides->rows[i]; LevelMemory value;
        if (!o->fields || o->fields >> LEVEL_MEMORY_FIELDS || !LevelMemoryGet(table, overrides, o->value.id, &value))
        { return Fail(why, "An edited level has no supported memory allocation in the base ROM."); }
        for (DWORD j = 0; j < i; j++)
            if (overrides->rows[j].value.id == o->value.id) { return Fail(why, "Duplicate memory override."); }
        for (int f = 0; f < LEVEL_MEMORY_FIELDS; f++)
            if (!ValidValue(value.kib[f])) { return Fail(why, "Memory allocations must be between 1 and 8192 KiB."); }
    }
    return TRUE;
}
BOOL LevelMemorySet(const LevelMemoryTable *table, LevelMemoryOverrides *overrides, const LevelMemory *value, const char **why)
{
    const LevelMemory *base = LevelMemoryFind(table, value->id);
    LevelMemoryOverride next = {0}; DWORD at;
    if (!base || overrides->count > ROM_MAX_LEVELS) { return Fail(why, "No matching base memory allocation exists."); }
    next.value.id = value->id;
    for (int f = 0; f < LEVEL_MEMORY_FIELDS; f++)
    {
        if (!ValidValue(value->kib[f])) { return Fail(why, "Memory allocations must be between 1 and 8192 KiB."); }
        if (base->kib[f] != value->kib[f]) { next.fields |= 1u << f; next.value.kib[f] = value->kib[f]; }
    }
    for (at = 0; at < overrides->count; at++) { if (overrides->rows[at].value.id == value->id) { break; } }
    if (!next.fields)
    {
        if (at < overrides->count)
        {
            memmove(&overrides->rows[at], &overrides->rows[at + 1], (overrides->count - at - 1) * sizeof(next));
            memset(&overrides->rows[--overrides->count], 0, sizeof(next));
        }
        return TRUE;
    }
    if (at == overrides->count)
    {
        if (at == ROM_MAX_LEVELS) { return Fail(why, "Too many memory overrides."); }
        overrides->count++;
    }
    overrides->rows[at] = next; return TRUE;
}
BOOL LevelMemoryReadOverride(LevelMemoryOverrides *overrides, const char *text)
{
    char *end; const char *why;
    if (!text || !isdigit((unsigned char)*text) || overrides->count > ROM_MAX_LEVELS) { return FALSE; }
    errno = 0; unsigned long id = strtoul(text, &end, 10);
    if (*end++ != '|' || id > INT32_MAX || errno) { return FALSE; }
    const char *separator = strchr(end, '|');
    if (!separator) { return FALSE; }
    int field;
    for (field = 0; field < LEVEL_MEMORY_FIELDS; field++)
        if (strlen(g_LevelMemoryKeys[field]) == (size_t)(separator - end)
            && !memcmp(end, g_LevelMemoryKeys[field], separator - end)) { break; }
    LevelMemory value = {0}; value.id = (DWORD)id;
    if (!LevelMemoryParseField(&value, field, separator + 1, &why)) { return FALSE; }
    DWORD at;
    for (at = 0; at < overrides->count; at++) { if (overrides->rows[at].value.id == value.id) { break; } }
    if (at == overrides->count)
    {
        if (at == ROM_MAX_LEVELS) { return FALSE; }
        memset(&overrides->rows[at], 0, sizeof(overrides->rows[at])); overrides->rows[at].value.id = value.id;
        overrides->count++;
    }
    if (overrides->rows[at].fields & (1u << field)) { return FALSE; }
    overrides->rows[at].fields |= 1u << field; overrides->rows[at].value.kib[field] = value.kib[field]; return TRUE;
}
BOOL LevelMemoryWriteOverrides(FILE *file, const LevelMemoryOverrides *overrides)
{
    if (overrides->count > ROM_MAX_LEVELS) { return FALSE; }
    for (DWORD i = 0; i < overrides->count; i++)
        for (int f = 0; f < LEVEL_MEMORY_FIELDS; f++)
            if (overrides->rows[i].fields & (1u << f))
                if (fprintf(file, "memory = %lu|%s|%lu\n", (unsigned long)overrides->rows[i].value.id,
                    g_LevelMemoryKeys[f], (unsigned long)overrides->rows[i].value.kib[f]) < 0) { return FALSE; }
    return TRUE;
}
BOOL LevelMemoryApplyRom(RomFile *rom, const LevelMemoryOverrides *overrides, const char **why)
{
    LevelMemoryTable table;
    if (!overrides->count) { return TRUE; }
    if (!LevelMemoryReadRom(rom, &table, why) || !LevelMemoryValidateOverrides(&table, overrides, why)) { return FALSE; }
    for (DWORD i = 0; i < overrides->count; i++)
    {
        LevelMemory value; DWORD id = overrides->rows[i].value.id;
        LevelMemoryGet(&table, overrides, id, &value);
        for (DWORD j = 0; j < table.count; j++) if (table.rows[j].id == id)
        {
            char slot[LEVEL_MEMORY_SLOT_SIZE] = {0};
            int written = snprintf(slot, sizeof(slot), "-ml0 -me0 -mgfx%lu -mvtx%lu -mt%lu -ma%lu",
                (unsigned long)value.kib[0], (unsigned long)value.kib[1], (unsigned long)value.kib[2], (unsigned long)value.kib[3]);
            if (written < 0 || written >= (int)sizeof(slot)) { return Fail(why, "The allocation string exceeds its reserved slot."); }
            memcpy(rom->data + table.offsets[j], slot, sizeof(slot)); break;
        }
    }
    return TRUE;
}
BOOL LevelMemoryRebase(const LevelMemoryTable *oldbase, const LevelMemoryTable *newbase, LevelMemoryOverrides *overrides, const char **why)
{
    LevelMemoryOverrides result = {0};
    if (!LevelMemoryValidateOverrides(oldbase, overrides, why)) { return FALSE; }
    for (DWORD i = 0; i < overrides->count; i++)
    {
        const LevelMemoryOverride *o = &overrides->rows[i];
        const LevelMemory *old = LevelMemoryFind(oldbase, o->value.id), *next = LevelMemoryFind(newbase, o->value.id);
        if (!next) { return Fail(why, "An edited level's memory allocation is missing from the new ROM."); }
        LevelMemory value = *next;
        for (int f = 0; f < LEVEL_MEMORY_FIELDS; f++) if (o->fields & (1u << f))
        {
            if (old->kib[f] != next->kib[f] && next->kib[f] != o->value.kib[f])
            {
                snprintf(g_LevelMemoryError, sizeof(g_LevelMemoryError), "Level %lu: %s changed in both project and ROM.", (unsigned long)value.id, g_LevelMemoryLabels[f]);
                return Fail(why, g_LevelMemoryError);
            }
            value.kib[f] = o->value.kib[f];
        }
        if (!LevelMemorySet(newbase, &result, &value, why)) { return FALSE; }
    }
    *overrides = result; return TRUE;
}
