#ifndef GEDITOR_BRIEFING_H
#define GEDITOR_BRIEFING_H
#include "project.h"
#include "setupload.h"
#include "textbank.h"
#define BRIEFING_OBJECTIVES 6
#define BRIEFING_CONDITIONS 64
#define BRIEFING_TEXT_SIZE 1500
typedef struct BriefingText { DWORD id, custom; char text[BRIEFING_TEXT_SIZE]; } BriefingText;
typedef struct BriefingCondition { DWORD words[5]; } BriefingCondition;
typedef struct BriefingObjective {
    DWORD enabled, menuDifficulty, watchDifficulty, flags, count;
    BriefingText menu, watch;
    BriefingCondition conditions[BRIEFING_CONDITIONS];
} BriefingObjective;
typedef struct BriefingDocument {
    DWORD level, bank;
    char bankname[64], briefname[64];
    unsigned char baseline[48];
    BriefingText menu[4], watch[5];
    BriefingObjective objectives[BRIEFING_OBJECTIVES];
} BriefingDocument;
typedef struct BriefingResource { char name[64]; unsigned char *data; DWORD size; } BriefingResource;
BOOL BriefingNames(LONG level, const char **bank, const char **brief);
BOOL BriefingLoad(const GEditorProject *project, DWORD level, const SetupFile *setup,
    BriefingDocument *out, TextBank *bank, const char **why);
BOOL BriefingEncode(const BriefingDocument *doc, unsigned char **data, DWORD *size, const char **why);
BOOL BriefingDecode(const unsigned char *data, DWORD size, BriefingDocument *doc, const char **why);
BOOL BriefingValidate(const BriefingDocument *doc, const TextBank *bank, const char **why);
BOOL BriefingValidateSetup(const BriefingDocument *doc,const SetupFile *setup,const char **why);
BOOL BriefingRewriteSetup(const unsigned char *data, DWORD size, const BriefingDocument *doc,
    const DWORD ids[21], unsigned char **out, DWORD *outsize, const char **why);
BOOL BriefingCollect(const GEditorProject *project, const RomFile *rom, const RomFile *target,
    BriefingResource **out, DWORD *count, const char **why);
void BriefingFreeResources(BriefingResource *resources, DWORD count);
const char *BriefingConditionName(DWORD type);
DWORD BriefingConditionWords(DWORD type);
#endif
