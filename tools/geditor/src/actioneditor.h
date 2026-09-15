#ifndef GEDITOR_ACTIONEDITOR_H
#define GEDITOR_ACTIONEDITOR_H
#include "actionblocks.h"
#include "project.h"
/* Modal draft: Apply returns a separately owned setup; Cancel changes nothing. */
BOOL ActionEditorShow(HWND owner, const GEditorProject *project, const SetupFile *setup,
    DWORD selectedcharacter, SetupFile *result, BOOL *changed, const char **why);
#endif
