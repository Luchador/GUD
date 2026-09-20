#ifndef GEDITOR_SETUPSTAN_H
#define GEDITOR_SETUPSTAN_H
#include "setupload.h"
#include "stanload.h"

typedef struct SetupStanRefresh {
    DWORD updated, unresolved;
    SetupPadRef firstunresolved;
} SetupStanRefresh;

/* Refresh a serialized copy. Pad indices, positions, bounds, live selections
 * and history never change. Unresolvable names are retained and counted. */
BOOL SetupRefreshPadStanNative(const unsigned char *data, DWORD size,
    const StanFile *stan, unsigned char **out, DWORD *sizeout,
    SetupStanRefresh *stats, const char **reasonout);
BOOL SetupSaveProjectFileWithStan(const char *projectdir, const SetupFile *setup,
    const StanFile *stan, SetupStanRefresh *stats, const char **reasonout);
#endif
