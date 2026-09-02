#ifndef GEDITOR_PROJECT_H
#define GEDITOR_PROJECT_H

#include <windows.h>
#include "resource.h"

typedef struct GEditorProject {
    char name[GEDITOR_NAME_MAX];
    char dir[MAX_PATH];
    char geppath[MAX_PATH];
} GEditorProject;

BOOL ProjectCreate(const char *name, const char *location, GEditorProject *proj, const char **reasonout);
BOOL ProjectRead(const char *geppath, GEditorProject *proj);
void ProjectClose(GEditorProject *proj);

#endif