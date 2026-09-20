#ifndef GEDITOR_SETUPSTAN_H
#define GEDITOR_SETUPSTAN_H
#include "setupload.h"
#include "stanload.h"

typedef struct SetupStanRefresh {
    DWORD updated, unresolved;
    SetupPadRef firstunresolved;
} SetupStanRefresh;

/* Called once for each unresolved live pad, in table order. Positions are
 * gameplay world units. FALSE aborts without returning modified setup bytes. */
typedef BOOL (*SetupStanIssueFn)(void *context, SetupPadRef pad,
    const float position[3], const char *stanname);
BOOL SetupRefreshPadStanNativeReport(const unsigned char *data, DWORD size,
    const StanFile *stan, unsigned char **out, DWORD *sizeout,
    SetupStanRefresh *stats, SetupStanIssueFn report, void *context, const char **reasonout);

/* Refresh a serialized copy. Pad indices, positions, bounds, live selections
 * and history never change. Unresolvable names are retained and counted. */
BOOL SetupRefreshPadStanNative(const unsigned char *data, DWORD size,
    const StanFile *stan, unsigned char **out, DWORD *sizeout,
    SetupStanRefresh *stats, const char **reasonout);
BOOL SetupSaveProjectFileWithStan(const char *projectdir, const SetupFile *setup,
    const StanFile *stan, SetupStanRefresh *stats, const char **reasonout);
#endif
