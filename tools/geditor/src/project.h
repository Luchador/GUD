#ifndef GEDITOR_PROJECT_H
#define GEDITOR_PROJECT_H

#include <windows.h>
#include <resource.h>

typedef struct GEditorProject {
    char name[GEDITOR_NAME_MAX];
    char dir[MAX_PATH];
    char geppath[MAX_PATH];
} GEditorProject;

#endif