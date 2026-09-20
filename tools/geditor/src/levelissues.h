#ifndef GEDITOR_LEVELISSUES_H
#define GEDITOR_LEVELISSUES_H
#include "bgdocument.h"
#include "setupload.h"
#include "stanload.h"

typedef enum LevelIssueKind {
    LEVEL_ISSUE_UNRESOLVED_PAD, LEVEL_ISSUE_PAD_POSITION, LEVEL_ISSUE_MISSING_STAN,
    LEVEL_ISSUE_STAN_LINK, LEVEL_ISSUE_STAN_ROOM, LEVEL_ISSUE_STAN_DATA,
    LEVEL_ISSUE_PORTAL, LEVEL_ISSUE_PATROL, LEVEL_ISSUE_CHARACTER_ID
} LevelIssueKind;
typedef enum LevelIssueTarget {
    LEVEL_ISSUE_NOWHERE, LEVEL_ISSUE_PAD, LEVEL_ISSUE_MODEL,
    LEVEL_ISSUE_STAN, LEVEL_ISSUE_PORTAL_FACE
} LevelIssueTarget;
typedef struct LevelIssue {
    LevelIssueKind kind;
    LevelIssueTarget target;
    BOOL error;
    DWORD index;
    SetupPadRef pad;
    char subject[96], description[320];
} LevelIssue;
typedef struct LevelIssueReport {
    LevelIssue *items;
    DWORD count, capacity, errors, warnings;
    BOOL truncated;
} LevelIssueReport;
typedef struct LevelIssueLocation {
    LevelIssueTarget target;
    DWORD index;
    SetupPadRef pad;
    double min[3], max[3];
} LevelIssueLocation;

/* Read-only checks of current in-memory assets, including unsaved edits.
 * No stored pointers into the level. Invalidate the report after any edit. */
BOOL LevelIssuesBuild(const BgDocument *bg, const SetupFile *setup,
    const StanFile *stan, float scale, LevelIssueReport *out, const char **why);
void LevelIssuesFree(LevelIssueReport *report);
BOOL LevelIssueLocate(const LevelIssue *issue, const BgDocument *bg,
    const SetupFile *setup, const StanFile *stan, float scale, LevelIssueLocation *out);
#endif
