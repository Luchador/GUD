#ifndef GEDITOR_ROOMEDIT_H
#define GEDITOR_ROOMEDIT_H
#include "bgdocument.h"
#include "setupload.h"
#include "stanload.h"

/* Resolve the authored placement floor, including overlapping rooms. Zero
 * means no room could be resolved. Shared pads are moved once, in place. */
DWORD RoomEditPadRoom(const StanFile *stan, const SetupPad *pad, float scale);
DWORD RoomEditObjectRoom(const SetupFile *setup, const StanFile *stan, float scale, DWORD object);
BOOL RoomEditPortalConnected(const BgPortalFile *portals, DWORD index, DWORD room);
BOOL RoomEditOffset(float scale, const double requested[3], double applied[3]);
/* Caller owns a combined BG/setup/Stan transaction and rolls back any failure.
 * Includes hidden geometry, all room pads, and door shadows. Room-local BG
 * vertices stay exact; translation changes their shared room origin. */
BOOL RoomEditTranslate(BgDocument *bg, SetupFile *setup, StanFile *stan, DWORD room,
    const double requested[3], BOOL *changed, const char **why);
#endif
