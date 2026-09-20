#ifndef GEDITOR_ROOMSTATS_H
#define GEDITOR_ROOMSTATS_H

#include "bgdocument.h"
#include "setupload.h"
#include "stanload.h"

enum { ROOM_PRIMARY_TRIS, ROOM_SECONDARY_TRIS, ROOM_STAN_TILES,
       ROOM_OBJECTS, ROOM_CHARACTERS, ROOM_COUNT_COLUMNS };
typedef struct RoomCounts { DWORD values[ROOM_COUNT_COLUMNS]; } RoomCounts;
typedef struct RoomStats {
    RoomCounts *rooms; /* 1..roomcount are BG rooms; 0 is Unassigned. */
    DWORD roomcount;
    RoomCounts total;
} RoomStats;

/* Read-only authored counts, including hidden geometry and all difficulties.
 * Each live placed model counts once, in its pad's resolved STAN room.
 * Carried/contained items are excluded; attached monitors inherit their owner.
 * Missing/invalid placements and non-BG STAN room IDs go into Unassigned. */
BOOL RoomStatsBuild(const BgDocument *bg, const SetupFile *setup, const StanFile *stan,
                    RoomStats *out, const char **why);
void RoomStatsFree(RoomStats *stats);
/* Frame live BG face vertices, then STAN for a BG-empty room, then its origin.
 * Reserved room zero and invalid IDs have no camera target. */
BOOL RoomStatsGetBounds(const BgDocument *bg, const StanFile *stan, DWORD room,
                        double min[3], double max[3]);
#endif
