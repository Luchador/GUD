#include <ctype.h>
#include <errno.h>
#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "environment.h"
#include "fog.h"

const EnvironmentField g_EnvironmentFields[ENVIRONMENT_FIELD_COUNT] = {
    {"fogEnabled", "Fog enabled", 4, 4, ENV_BOOL32, 0, 1},
    {"nearclip", "Near clip", 8, 4, ENV_FLOAT, 0, FLT_MAX},
    {"farclip", "Far clip", 12, 4, ENV_FLOAT, 0, FLT_MAX},
    {"nearfog", "Near fog", 16, 4, ENV_FLOAT, 0, FLT_MAX},
    {"maxvisrng", "Max visibility", 20, 4, ENV_FLOAT, 0, FLT_MAX},
    {"maxobfnrng", "Max obfuscation", 24, 4, ENV_FLOAT, 0, FLT_MAX},
    {"minvisrng", "Min visibility", 28, 4, ENV_FLOAT, 0, FLT_MAX},
    {"intensity", "Intensity", 32, 4, ENV_U32, 0, UINT32_MAX},
    {"fogstart", "Fog start", 36, 4, ENV_S32, INT32_MIN, INT32_MAX},
    {"fogend", "Fog end", 40, 4, ENV_S32, INT32_MIN, INT32_MAX},
    {"fogRed", "Fog red", 44, 1, ENV_BYTE, 0, 255},
    {"fogGreen", "Fog green", 45, 1, ENV_BYTE, 0, 255},
    {"fogBlue", "Fog blue", 46, 1, ENV_BYTE, 0, 255},
    {"clouds", "Clouds", 47, 1, ENV_BOOL8, 0, 1},
    {"skyheight", "Sky height", 48, 4, ENV_FLOAT, -FLT_MAX, FLT_MAX},
    {"skyid", "Sky image (0-2)", 52, 2, ENV_SHORT, 0, 2},
    {"cloudRed", "Cloud red", 56, 4, ENV_FLOAT, 0, 255},
    {"cloudGreen", "Cloud green", 60, 4, ENV_FLOAT, 0, 255},
    {"cloudBlue", "Cloud blue", 64, 4, ENV_FLOAT, 0, 255},
    {"iswater", "Water", 68, 1, ENV_BOOL8, 0, 1},
    {"waterheight", "Water height", 72, 4, ENV_FLOAT, -FLT_MAX, FLT_MAX},
    {"waterid", "Water image (0-2)", 76, 2, ENV_SHORT, 0, 2},
    {"waterRed", "Water red", 80, 4, ENV_FLOAT, 0, 255},
    {"waterGreen", "Water green", 84, 4, ENV_FLOAT, 0, 255},
    {"waterBlue", "Water blue", 88, 4, ENV_FLOAT, 0, 255},
    {"horizonyoffset", "Horizon Y offset", 92, 4, ENV_FLOAT, -FLT_MAX, FLT_MAX},
    {"propStartFade", "Prop fade start (px)", 96, 4, ENV_FLOAT, -FLT_MAX, FLT_MAX},
    {"propEndFade", "Prop fade end (px)", 100, 4, ENV_FLOAT, 0, FLT_MAX}
};
static char g_EnvironmentError[256];
static BOOL Fail(const char **why, const char *message)
{ if (why) { *why = message; } return FALSE; }
static DWORD Read32(const unsigned char *p)
{ return (DWORD)p[0] << 24 | (DWORD)p[1] << 16 | (DWORD)p[2] << 8 | p[3]; }
static void Write32(unsigned char *p, DWORD value)
{ p[0] = value >> 24; p[1] = value >> 16; p[2] = value >> 8; p[3] = value; }
DWORD EnvironmentId(const EditorEnvironment *row) { return Read32(row->data); }
static double Value(const EditorEnvironment *row, int field)
{
    const EnvironmentField *f = &g_EnvironmentFields[field];
    const unsigned char *p = row->data + f->offset;
    if (f->size == 1) { return *p; }
    if (f->size == 2) { return (int16_t)((unsigned)p[0] << 8 | p[1]); }
    DWORD bits = Read32(p);
    if (f->type == ENV_FLOAT) { float v; memcpy(&v, &bits, 4); return v; }
    return f->type == ENV_S32 ? (double)(int32_t)bits : (double)bits;
}
static void SetValue(EditorEnvironment *row, int field, double value)
{
    const EnvironmentField *f = &g_EnvironmentFields[field];
    unsigned char *p = row->data + f->offset;
    if (f->size == 1) { *p = (unsigned char)value; }
    else if (f->size == 2) { unsigned v = (unsigned)value; p[0] = v >> 8; p[1] = v; }
    else if (f->type == ENV_FLOAT) { float v = (float)value; DWORD bits; memcpy(&bits, &v, 4); Write32(p, bits); }
    else { Write32(p, f->type == ENV_S32 ? (DWORD)(int32_t)value : (DWORD)value); }
}
static BOOL FieldValid(int field, double value, const char **why)
{
    const EnvironmentField *f = &g_EnvironmentFields[field];
    if (!isfinite(value) || value < f->minimum || value > f->maximum
        || (f->type != ENV_FLOAT && value != trunc(value)))
    {
        snprintf(g_EnvironmentError, sizeof(g_EnvironmentError), "%s must be %s between %.9g and %.9g.",
            f->key, f->type == ENV_FLOAT ? "a finite number" : "an integer", f->minimum, f->maximum);
        return Fail(why, g_EnvironmentError);
    }
    return TRUE;
}
void EnvironmentFormatField(const EditorEnvironment *value, int field, char *text, size_t size)
{ snprintf(text, size, g_EnvironmentFields[field].type == ENV_FLOAT ? "%.9g" : "%.0f", Value(value, field)); }
BOOL EnvironmentParseField(EditorEnvironment *value, int field, const char *text, const char **why)
{
    char *end;
    if (!value || field < 0 || field >= ENVIRONMENT_FIELD_COUNT || !text)
    { return Fail(why, "Invalid environment field."); }
    errno = 0;
    double v = g_EnvironmentFields[field].type == ENV_FLOAT ? (double)strtof(text, &end) : strtod(text, &end);
    if (end == text) { return Fail(why, "Enter a number for every environment field."); }
    while (isspace((unsigned char)*end)) { end++; }
    if (*end || errno == ERANGE) { return Fail(why, "An environment value is not a complete number or is out of range."); }
    if (!FieldValid(field, v, why)) { return FALSE; }
    SetValue(value, field, v); return TRUE;
}
const EditorEnvironment *EnvironmentFind(const EnvironmentTable *table, DWORD id)
{
    if (table && table->count <= ENVIRONMENT_MAX_RECORDS)
        for (DWORD i = 0; i < table->count; i++)
        { if (EnvironmentId(&table->rows[i]) == id) { return &table->rows[i]; } }
    return NULL;
}
BOOL EnvironmentReadRom(const RomFile *rom, EnvironmentTable *table, DWORD *offset, const char **why)
{
    const RomManifestEntry *env = NULL, *cmap = NULL;
    memset(table, 0, sizeof(*table));
    if (!rom || !rom->data || rom->info.entrycount > ROM_MAX_ENTRIES)
    { return Fail(why, "No base ROM environment table is available."); }
    for (DWORD i = 0; i < rom->info.entrycount; i++)
    {
        const RomManifestEntry *e = &rom->info.entries[i];
        if (e->kind == 0x454e5654u) { if (env) { return Fail(why, "Duplicate environment manifest."); } env = e; }
        if (e->kind == 0x434d4150u) { cmap = e; }
    }
    if (!env || !cmap || env->flags != ENVIRONMENT_RECORD_SIZE || env->romend
        || cmap->romstart >= cmap->romend || cmap->romend > rom->size
        || env->romstart < cmap->romstart || env->romstart >= cmap->romend)
    { return Fail(why, "The base ROM has no supported environment table."); }
    DWORD at = env->romstart;
    while (cmap->romend - at >= ENVIRONMENT_RECORD_SIZE)
    {
        DWORD id = Read32(rom->data + at);
        if (!id) { if (offset) { *offset = env->romstart; } return TRUE; }
        if (table->count == ENVIRONMENT_MAX_RECORDS || EnvironmentFind(table, id)) { break; }
        memcpy(table->rows[table->count++].data, rom->data + at, ENVIRONMENT_RECORD_SIZE);
        at += ENVIRONMENT_RECORD_SIZE;
    }
    memset(table, 0, sizeof(*table));
    return Fail(why, "The environment table is incomplete, too large, or contains duplicate IDs.");
}
static void CopyFields(EditorEnvironment *target, const EnvironmentOverride *override)
{
    for (int i = 0; i < ENVIRONMENT_FIELD_COUNT; i++)
        if (override->fields & ((DWORD)1 << i))
        {
            const EnvironmentField *f = &g_EnvironmentFields[i];
            memcpy(target->data + f->offset, override->value.data + f->offset, f->size);
        }
}
BOOL EnvironmentGet(const EnvironmentTable *table, const EnvironmentOverrides *overrides,
                    DWORD id, EditorEnvironment *value)
{
    const EditorEnvironment *base = EnvironmentFind(table, id);
    if (!base || !overrides || overrides->count > ENVIRONMENT_MAX_RECORDS) { return FALSE; }
    *value = *base;
    for (DWORD i = 0; i < overrides->count; i++)
        if (EnvironmentId(&overrides->rows[i].value) == id) { CopyFields(value, &overrides->rows[i]); break; }
    return TRUE;
}
BOOL EnvironmentValidate(const EditorEnvironment *value, const char **why)
{
    FogCurve curve;
    for (int i = 0; i < ENVIRONMENT_FIELD_COUNT; i++)
    { if (!FieldValid(i, Value(value, i), why)) { return FALSE; } }
    if (Value(value, 1) <= 0 || Value(value, 2) <= Value(value, 1))
    { return Fail(why, "nearclip must be positive and farclip must be greater than nearclip."); }
    if (Value(value, 0) && !FogConfigure(&curve, Value(value, 1), Value(value, 2), 1, (int)Value(value, 8), (int)Value(value, 9)))
    { return Fail(why, "Fog needs fogend greater than fogstart and a range that the N64 fog calculation can represent (for example, 996 to 1000)."); }
    if (Value(value, 26) > 0 && Value(value, 27) >= Value(value, 26))
    { return Fail(why, "propEndFade must be less than a positive propStartFade. A zero start uses engine defaults; a negative start disables fading."); }
    return TRUE;
}
BOOL EnvironmentValidateOverrides(const EnvironmentTable *table, const EnvironmentOverrides *overrides, const char **why)
{
    if (!overrides || overrides->count > ENVIRONMENT_MAX_RECORDS)
    { return Fail(why, "Invalid environment overrides."); }
    for (DWORD i = 0; i < overrides->count; i++)
    {
        const EnvironmentOverride *o = &overrides->rows[i];
        DWORD id = EnvironmentId(&o->value);
        EditorEnvironment value;
        if (!o->fields || o->fields >> ENVIRONMENT_FIELD_COUNT || !id
            || !EnvironmentGet(table, overrides, id, &value))
        { return Fail(why, "An environment override is invalid or its row is absent from the base ROM."); }
        for (DWORD j = 0; j < i; j++)
            if (EnvironmentId(&overrides->rows[j].value) == id) { return Fail(why, "Duplicate environment override."); }
        if (!EnvironmentValidate(&value, why)) { return FALSE; }
    }
    return TRUE;
}
BOOL EnvironmentSet(const EnvironmentTable *table, EnvironmentOverrides *overrides,
                    const EditorEnvironment *value, const char **why)
{
    DWORD id = EnvironmentId(value), at;
    const EditorEnvironment *base = EnvironmentFind(table, id);
    EnvironmentOverride next = {0};
    if (!base || overrides->count > ENVIRONMENT_MAX_RECORDS) { return Fail(why, "No matching base environment exists."); }
    /* Restoring defaults must work even for an old, unusual authored row. */
    for (int i = 0; i < ENVIRONMENT_FIELD_COUNT; i++)
    {
        const EnvironmentField *f = &g_EnvironmentFields[i];
        if (memcmp(base->data + f->offset, value->data + f->offset, f->size))
        { next.fields |= (DWORD)1 << i; memcpy(next.value.data + f->offset, value->data + f->offset, f->size); }
    }
    if (next.fields && !EnvironmentValidate(value, why)) { return FALSE; }
    Write32(next.value.data, id);
    for (at = 0; at < overrides->count; at++)
    { if (EnvironmentId(&overrides->rows[at].value) == id) { break; } }
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
        if (at == ENVIRONMENT_MAX_RECORDS) { return Fail(why, "Too many environment overrides."); }
        overrides->count++;
    }
    overrides->rows[at] = next; return TRUE;
}
BOOL EnvironmentApplyRom(RomFile *rom, const EnvironmentOverrides *overrides, const char **why)
{
    EnvironmentTable table;
    DWORD offset;
    if (!overrides->count) { return TRUE; }
    if (!EnvironmentReadRom(rom, &table, &offset, why) || !EnvironmentValidateOverrides(&table, overrides, why)) { return FALSE; }
    /* Validate the complete change first, then patch only named field bytes. */
    for (DWORD i = 0; i < table.count; i++)
    {
        EditorEnvironment row;
        EnvironmentGet(&table, overrides, EnvironmentId(&table.rows[i]), &row);
        memcpy(rom->data + offset + i * ENVIRONMENT_RECORD_SIZE, row.data, ENVIRONMENT_RECORD_SIZE);
    }
    return TRUE;
}
BOOL EnvironmentRebase(const EnvironmentTable *oldbase, const EnvironmentTable *newbase,
                       EnvironmentOverrides *overrides, const char **why)
{
    EnvironmentOverrides result = {0};
    if (!EnvironmentValidateOverrides(oldbase, overrides, why)) { return FALSE; }
    for (DWORD i = 0; i < overrides->count; i++)
    {
        const EnvironmentOverride *o = &overrides->rows[i];
        DWORD id = EnvironmentId(&o->value);
        const EditorEnvironment *old = EnvironmentFind(oldbase, id), *next = EnvironmentFind(newbase, id);
        if (!next) { return Fail(why, "An edited environment row is missing from the new ROM."); }
        EditorEnvironment value = *next;
        for (int f = 0; f < ENVIRONMENT_FIELD_COUNT; f++)
            if (o->fields & ((DWORD)1 << f))
            {
                const EnvironmentField *field = &g_EnvironmentFields[f];
                unsigned at = field->offset, size = field->size;
                if (memcmp(old->data + at, next->data + at, size) && memcmp(o->value.data + at, next->data + at, size))
                {
                    snprintf(g_EnvironmentError, sizeof(g_EnvironmentError), "Environment %lu: %s changed in both project and ROM.", (unsigned long)id, field->key);
                    return Fail(why, g_EnvironmentError);
                }
            }
        CopyFields(&value, o);
        if (!EnvironmentSet(newbase, &result, &value, why)) { return FALSE; }
    }
    *overrides = result; return TRUE;
}
BOOL EnvironmentReadOverride(EnvironmentOverrides *overrides, const char *text)
{
    char *end;
    const char *why;
    if (!text || !isdigit((unsigned char)*text) || overrides->count > ENVIRONMENT_MAX_RECORDS) { return FALSE; }
    errno = 0;
    unsigned long id = strtoul(text, &end, 10);
    if (end == text || *end++ != '|' || !id || id > UINT32_MAX || errno) { return FALSE; }
    const char *separator = strchr(end, '|');
    if (!separator) { return FALSE; }
    int field;
    for (field = 0; field < ENVIRONMENT_FIELD_COUNT; field++)
        if (strlen(g_EnvironmentFields[field].key) == (size_t)(separator - end)
            && !memcmp(end, g_EnvironmentFields[field].key, separator - end)) { break; }
    if (field == ENVIRONMENT_FIELD_COUNT) { return FALSE; }
    EditorEnvironment value = {0};
    Write32(value.data, (DWORD)id);
    if (!EnvironmentParseField(&value, field, separator + 1, &why)) { return FALSE; }
    DWORD at;
    for (at = 0; at < overrides->count; at++)
    { if (EnvironmentId(&overrides->rows[at].value) == (DWORD)id) { break; } }
    if (at == overrides->count)
    {
        if (at == ENVIRONMENT_MAX_RECORDS) { return FALSE; }
        memset(&overrides->rows[at], 0, sizeof(overrides->rows[at]));
        Write32(overrides->rows[at].value.data, (DWORD)id); overrides->count++;
    }
    EnvironmentOverride *o = &overrides->rows[at];
    if (o->fields & ((DWORD)1 << field)) { return FALSE; }
    const EnvironmentField *f = &g_EnvironmentFields[field];
    memcpy(o->value.data + f->offset, value.data + f->offset, f->size);
    o->fields |= (DWORD)1 << field; return TRUE;
}
BOOL EnvironmentWriteOverrides(FILE *file, const EnvironmentOverrides *overrides)
{
    if (overrides->count > ENVIRONMENT_MAX_RECORDS) { return FALSE; }
    for (DWORD i = 0; i < overrides->count; i++)
        for (int f = 0; f < ENVIRONMENT_FIELD_COUNT; f++)
            if (overrides->rows[i].fields & ((DWORD)1 << f))
            {
                char text[64];
                EnvironmentFormatField(&overrides->rows[i].value, f, text, sizeof(text));
                if (fprintf(file, "environment = %lu|%s|%s\n", (unsigned long)EnvironmentId(&overrides->rows[i].value),
                    g_EnvironmentFields[f].key, text) < 0) { return FALSE; }
            }
    return TRUE;
}
static DWORD GameplayId(const EnvironmentTable *table, LONG levelid)
{
    BOOL mp = levelid >= 400 && levelid < 500;
    DWORD id = mp ? (DWORD)levelid - 400u + 200u : (DWORD)levelid;
    if (EnvironmentFind(table, id)) { return id; }
    if (mp && EnvironmentFind(table, 200)) { return 200; }
    return UINT32_MAX;
}
int EnvironmentChoices(const EnvironmentTable *table, LONG levelid, EnvironmentChoice choices[ENVIRONMENT_MAX_CHOICES])
{
    BOOL mp = levelid >= 400 && levelid < 500;
    DWORD stage = mp ? (DWORD)levelid - 400 : (DWORD)levelid;
    int count = 0;
    static const DWORD offsets[] = {0, 100, 900, 200, 300, 400};
    static const char *labels[] = {"Gameplay", "Alternate", "Cinema", "2 players", "3 players", "4 players"};
    for (int i = 0; i < ENVIRONMENT_MAX_CHOICES; i++)
    {
        if (mp && i < 3) { continue; }
        DWORD id = stage + offsets[i];
        BOOL shared = FALSE;
        if (!EnvironmentFind(table, id))
        {
            if (i == 0) { id = UINT32_MAX; }
            else if (mp && i >= 3) { id = offsets[i]; }
            else { continue; }
            if (!EnvironmentFind(table, id)) { id = UINT32_MAX; }
            shared = TRUE;
        }
        if (EnvironmentFind(table, id)) { choices[count++] = (EnvironmentChoice){id, labels[i], shared}; }
    }
    return count;
}
void EnvironmentPreview(const EditorEnvironment *value, unsigned char rgb[3], RomFog *fog, RomClouds *clouds)
{
    static const DWORD images[] = {0x08b4u, 0x05e4u, 0x05e5u};
    memcpy(rgb, value->data + 44, 3);
    *fog = (RomFog){Value(value, 0) != 0, (float)Value(value, 1), (float)Value(value, 2), (LONG)Value(value, 8), (LONG)Value(value, 9)};
    memset(clouds, 0, sizeof(*clouds));
    int image = (int)Value(value, 15);
    if (!Value(value, 13) || image < 0 || image >= 3 || !isfinite(Value(value, 14)) || !isfinite(Value(value, 25))) { return; }
    for (int i = 0; i < 3; i++)
    { if (!isfinite(Value(value, 16 + i)) || Value(value, 16 + i) < 0 || Value(value, 16 + i) > 255) { return; } }
    clouds->enabled = TRUE; clouds->textureid = images[image]; clouds->height = (float)Value(value, 14);
    clouds->horizonoffset = (float)Value(value, 25);
    for (int i = 0; i < 3; i++) { clouds->color[i] = (float)Value(value, 16 + i); }
}
void EnvironmentRefreshLevels(const EnvironmentTable *table, const EnvironmentOverrides *overrides, RomLevel *levels, DWORD count)
{
    for (DWORD i = 0; i < count; i++)
    {
        EditorEnvironment row;
        if (EnvironmentGet(table, overrides, GameplayId(table, levels[i].levelID), &row))
        {
            levels[i].hasbackgroundcolor = TRUE;
            EnvironmentPreview(&row, levels[i].backgroundcolor, &levels[i].fog, &levels[i].clouds);
        }
    }
}
