#ifndef GEDITOR_PATROLEDITOR_H
#define GEDITOR_PATROLEDITOR_H
#include "patrolpaths.h"
BOOL PatrolEditorShow(HWND owner, const SetupFile *source, DWORD selectedpad,
    SetupFile *edited, BOOL *changed, const char **why);
#endif
