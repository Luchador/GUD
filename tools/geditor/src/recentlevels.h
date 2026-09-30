#ifndef GEDITOR_RECENTLEVELS_H
#define GEDITOR_RECENTLEVELS_H

#include <windows.h>

#define RECENT_LEVELS_MAX 5

typedef struct RecentLevels {
    char projectpath[MAX_PATH];
    DWORD count;
    LONG ids[RECENT_LEVELS_MAX]; /* Level identity, never table/campaign order. */
} RecentLevels;

/* User preferences, scoped by the project's absolute .gep path. */
void RecentLevelsLoad(RecentLevels *recent, const char *projectpath);
void RecentLevelsRemember(RecentLevels *recent, LONG levelid);

#endif
