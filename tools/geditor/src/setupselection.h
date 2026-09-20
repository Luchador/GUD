#ifndef GEDITOR_SETUPSELECTION_H
#define GEDITOR_SETUPSELECTION_H
#include "setupload.h"

typedef struct SetupFlagSummary {
    DWORD objects, characters, all[3], any[3];
    int objecttype; /* -1 when selected object types differ */
} SetupFlagSummary;

BOOL SetupSelectionValid(const SetupFile *setup, DWORD selection);
BOOL SetupSelectionSimilar(const SetupFile *setup, const DWORD *selected, DWORD count,
    DWORD **out, DWORD *outcount, const char **why);
BOOL SetupSelectionFlags(const SetupFile *setup, const DWORD *selected, DWORD count, SetupFlagSummary *out);
/* Banks 0/1 edit object flags; bank 2 edits saved character setup flags.
 * Other record families and every unrelated bit remain unchanged. Atomic. */
BOOL SetupSelectionSetFlag(SetupFile *setup, const DWORD *selected, DWORD count,
    unsigned int bank, DWORD mask, BOOL enabled, BOOL *changed, const char **why);
#endif
