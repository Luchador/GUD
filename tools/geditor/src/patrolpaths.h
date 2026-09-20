#ifndef GEDITOR_PATROLPATHS_H
#define GEDITOR_PATROLPATHS_H
#include "setupload.h"

#define PATROL_MAX_PATHS 256u
#define PATROL_MAX_POINTS 65535u
#define PATROL_NONE (-1)
#define PATROL_CUSTOM (-2)
typedef struct PatrolPath {
    unsigned char id, flags;
    unsigned short length; /* Preserve untouched authored fields. Runtime recalculates this. */
    DWORD *points, count; /* Native waypoint indices, NOT pad IDs. */
    BOOL changed;
} PatrolPath;
typedef struct PatrolDocument {
    DWORD *pads, waypointcount;
    DWORD originalwaypointcount; /* Draft-only mappings can be discarded until Apply. */
    PatrolPath paths[PATROL_MAX_PATHS];
    DWORD count;
    BOOL changed;
} PatrolDocument;
typedef struct PatrolAssignment {
    DWORD characterindex, sourceoffset;
    unsigned short chrnum, previous;
    int path; /* PATROL_NONE or native path ID. */
} PatrolAssignment;

BOOL PatrolDocumentLoad(const SetupFile *setup, PatrolDocument *out, const char **why);
void PatrolDocumentFree(PatrolDocument *doc);
BOOL PatrolDocumentAdd(PatrolDocument *doc, const SetupFile *setup, DWORD *index, const char **why);
BOOL PatrolDocumentDelete(PatrolDocument *doc, const SetupFile *setup, DWORD index, const char **why);
BOOL PatrolPathInsert(PatrolDocument *doc, DWORD path, DWORD before, DWORD waypoint, const char **why);
BOOL PatrolPathInsertPad(PatrolDocument *doc, const SetupFile *setup, DWORD path,
    DWORD before, DWORD pad, const char **why);
BOOL PatrolDocumentMatches(const PatrolDocument *a, const PatrolDocument *b);
BOOL PatrolPathRemove(PatrolDocument *doc, DWORD path, DWORD point);
BOOL PatrolPathMove(PatrolDocument *doc, DWORD path, DWORD point, int direction);
BOOL PatrolPathSetLoop(PatrolDocument *doc, DWORD path, BOOL loop);
/* Independent compacted result: on failure neither the source nor the draft changes. */
BOOL PatrolDocumentCompile(const PatrolDocument *doc, const SetupFile *source, SetupFile *out, const char **why);
/* Only simple StartPatrol + JumpToSelf starters are inferred. Complex AI is shown as custom. */
int PatrolCharacterPath(const SetupFile *setup, DWORD character);
BOOL PatrolAssignCharacter(const SetupFile *source, const PatrolAssignment *edit,
    SetupFile *out, BOOL *changed, const char **why);
#endif
