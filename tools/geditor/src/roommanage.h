#ifndef GEDITOR_ROOMMANAGE_H
#define GEDITOR_ROOMMANAGE_H
#include "bgdocument.h"
#include "setupload.h"
#include "stanload.h"

/* Add an empty room at the end; the runtime reserves room zero. */
BOOL RoomManageAdd(BgDocument *bg, DWORD *created, const char **why);
/* Empty rooms only. Checks all pads, script pad references (including shared
 * scripts), visibility ranges, portal links, STAN and door shadows. */
BOOL RoomManageCanRemove(const BgDocument *bg, const BgFile *source,
    const SetupFile *setup, const StanFile *stan, DWORD room, char *why, size_t size);
/* Rechecks before modifying. Renumbers remaining BG/portal/STAN/visibility/
 * shadow references together. Caller owns a combined room history edit. */
BOOL RoomManageRemove(BgDocument *bg, const BgFile *source, SetupFile *setup,
    StanFile *stan, DWORD room, char *why, size_t size);
#endif
