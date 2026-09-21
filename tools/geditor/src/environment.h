#ifndef GEDITOR_ENVIRONMENT_H
#define GEDITOR_ENVIRONMENT_H

#include <stdio.h>
#include "rom.h"

#define ENVIRONMENT_RECORD_SIZE 104u
#define ENVIRONMENT_MAX_RECORDS 128u
#define ENVIRONMENT_FIELD_COUNT 28
#define ENVIRONMENT_MAX_CHOICES 6

/* Retain the native bytes so reserved fields and unrelated ROM defaults are
 * never rewritten. Overrides contain only the fields explicitly changed. */
typedef struct EditorEnvironment { unsigned char data[ENVIRONMENT_RECORD_SIZE]; } EditorEnvironment;
typedef struct EnvironmentTable {
    DWORD count;
    EditorEnvironment rows[ENVIRONMENT_MAX_RECORDS];
} EnvironmentTable;
typedef struct EnvironmentOverride {
    DWORD fields;
    EditorEnvironment value;
} EnvironmentOverride;
typedef struct EnvironmentOverrides {
    DWORD count;
    EnvironmentOverride rows[ENVIRONMENT_MAX_RECORDS];
} EnvironmentOverrides;
typedef enum EnvironmentFieldType {
    ENV_FLOAT, ENV_U32, ENV_S32, ENV_BYTE, ENV_SHORT, ENV_BOOL32, ENV_BOOL8
} EnvironmentFieldType;
typedef struct EnvironmentField {
    const char *key, *label; /* NULL label: retained project data, no control */
    unsigned offset, size;
    EnvironmentFieldType type;
    double minimum, maximum;
} EnvironmentField;
typedef struct EnvironmentChoice { DWORD id; const char *label; BOOL shared; } EnvironmentChoice;

extern const EnvironmentField g_EnvironmentFields[ENVIRONMENT_FIELD_COUNT];
DWORD EnvironmentId(const EditorEnvironment *row);
const EditorEnvironment *EnvironmentFind(const EnvironmentTable *table, DWORD id);
BOOL EnvironmentReadRom(const RomFile *rom, EnvironmentTable *table, DWORD *offset, const char **why);
BOOL EnvironmentGet(const EnvironmentTable *table, const EnvironmentOverrides *overrides,
                    DWORD id, EditorEnvironment *value);
BOOL EnvironmentValidate(const EditorEnvironment *value, const char **why);
BOOL EnvironmentValidateOverrides(const EnvironmentTable *table, const EnvironmentOverrides *overrides, const char **why);
BOOL EnvironmentSet(const EnvironmentTable *table, EnvironmentOverrides *overrides,
                    const EditorEnvironment *value, const char **why);
BOOL EnvironmentApplyRom(RomFile *rom, const EnvironmentOverrides *overrides, const char **why);
BOOL EnvironmentRebase(const EnvironmentTable *oldbase, const EnvironmentTable *newbase,
                       EnvironmentOverrides *overrides, const char **why);
void EnvironmentFormatField(const EditorEnvironment *value, int field, char *text, size_t size);
BOOL EnvironmentParseField(EditorEnvironment *value, int field, const char *text, const char **why);
BOOL EnvironmentReadOverride(EnvironmentOverrides *overrides, const char *text);
BOOL EnvironmentWriteOverrides(FILE *file, const EnvironmentOverrides *overrides);
int EnvironmentChoices(const EnvironmentTable *table, LONG levelid, EnvironmentChoice choices[ENVIRONMENT_MAX_CHOICES]);
void EnvironmentPreview(const EditorEnvironment *value, unsigned char rgb[3], RomFog *fog, RomClouds *clouds);
void EnvironmentRefreshLevels(const EnvironmentTable *table, const EnvironmentOverrides *overrides, RomLevel *levels, DWORD count);

#endif
