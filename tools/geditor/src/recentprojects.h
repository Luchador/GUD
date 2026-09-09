#ifndef GEDITOR_RECENTPROJECTS_H
#define GEDITOR_RECENTPROJECTS_H

#include <windows.h>

#define RECENT_PROJECTS_MAX 5

typedef struct RecentProjects {
    DWORD count;
    char paths[RECENT_PROJECTS_MAX][MAX_PATH];
} RecentProjects;

/* Per-user editor preferences, separate from project data and undo history. */
void RecentProjectsLoad(RecentProjects *recent);
void RecentProjectsRemember(RecentProjects *recent, const char *path);

#endif
